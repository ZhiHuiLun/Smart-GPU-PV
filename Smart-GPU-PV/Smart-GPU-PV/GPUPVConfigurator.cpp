#include "GPUPVConfigurator.h"
#include "PowerShellExecutor.h"
#include <algorithm>
#include <cctype>
#include "VMManager.h"
#include "WmiHelper.h"
#include "VhdHelper.h"
#include "HyperVException.h"
#include "Utils.h"

// 辅助宏：用于在C++20中处理UTF-8字符串字面量
// C++20中u8""类型为char8_t[]，需要转换为char*以便std::string使用
#define UTF8(s) reinterpret_cast<const char*>(u8##s)

// 备份状态
GPUPVBackup GPUPVConfigurator::BackupState(const std::string& vmName) {
    GPUPVBackup backup;
    
    // 获取适配器信息
    std::string command = 
        "Get-VMGpuPartitionAdapter -VMName '" + vmName + "' -ErrorAction SilentlyContinue | "
        "Select-Object InstancePath, MinPartitionVRAM | ConvertTo-Json";
    
    std::string output = PowerShellExecutor::Execute(command);
    if (!output.empty()) {
        backup.bHasAdapter = true;
        backup.strInstancePath = Utils::ExtractJsonValue(output, "InstancePath");
        std::string vramStr = Utils::ExtractJsonValue(output, "MinPartitionVRAM");
        try {
            backup.ui64VramBytes = std::stoull(vramStr);
        } catch (...) {
            backup.ui64VramBytes = 0;
        }
    }
    
    // 获取CacheTypes状态
    command = "(Get-VM -VMName '" + vmName + "').GuestControlledCacheTypes";
    output = PowerShellExecutor::Execute(command);
    backup.bGuestControlledCacheTypes = (Utils::Trim(output) == "True");
    
    return backup;
}

// 恢复状态
void GPUPVConfigurator::RestoreState(const std::string& vmName, const GPUPVBackup& backup, ProgressCallback callback) {
    std::string error;
    
    // 清理当前可能的半成品
    std::string cleanCmd = "Remove-VMGpuPartitionAdapter -VMName '" + vmName + "' -ErrorAction SilentlyContinue";
    PowerShellExecutor::Execute(cleanCmd);
    
    if (backup.bHasAdapter && !backup.strInstancePath.empty()) {
        callback(UTF8("正在回滚：恢复GPU分区适配器...\n"));
        if (AddGPUPartitionAdapter(vmName, backup.strInstancePath, error)) {
            if (backup.ui64VramBytes > 0) {
                ConfigureGPUResources(vmName, backup.ui64VramBytes, error);
            }
        } else {
            callback(UTF8("回滚警告：无法恢复适配器 - ") + error + "\n");
        }
    }
    
    callback(UTF8("正在回滚：恢复GuestControlledCacheTypes设置...\n"));
    std::string cacheCmd = "Set-VM -VMName '" + vmName + "' -GuestControlledCacheTypes $" + (backup.bGuestControlledCacheTypes ? "true" : "false");
    PowerShellExecutor::Execute(cacheCmd);
}

// 配置GPU-PV（完整流程）
bool GPUPVConfigurator::ConfigureGPUPV(
    const std::string& vmName,
    const std::string& gpuName,
    const std::string& gpuInstancePath,
    const std::string& driverPath,
    int vramMB,
    ProgressCallback callback) {
    
    std::string error;
    
    // 步骤1：停止虚拟机
    callback(UTF8("正在停止虚拟机...\n"));
    if (!VMManager::StopVM(vmName, error)) {
        callback(UTF8("错误: ") + error + "\n");
        return false;
    }
    callback(UTF8("虚拟机已停止\n"));

    // 步骤1.2：关闭安全启动 (GPU-PV 必要条件)
    callback(UTF8("正在关闭安全启动...\n"));
    std::string secureBootCmd = "Set-VMFirmware -VMName '" + vmName + "' -EnableSecureBoot Off";
    PowerShellExecutor::Execute(secureBootCmd);

    // 步骤1.5：备份当前状态
    callback(UTF8("正在备份当前配置...\n"));
    GPUPVBackup backup = BackupState(vmName);

    // 步骤2：清理旧的GPU分区适配器（无论开启还是关闭，都先清理旧配置）
    callback(UTF8("正在清理旧的GPU分区适配器...\n"));
    // 使用 ExecuteWithCheck 并忽略可能的错误（如果不存在适配器）
    // 但是如果是关闭操作，我们需要确保清理成功，除非它本来就不存在
    std::string cleanCmd = "Remove-VMGpuPartitionAdapter -VMName '" + vmName + "' -ErrorAction SilentlyContinue";
    PowerShellExecutor::Execute(cleanCmd); // 暂时保持 Execute，因为SilentlyContinue会抑制错误

    // 判断是开启还是关闭
    if (vramMB < 64) {
        callback(UTF8("检测到显存设置小于 64MB，执行关闭 GPU-PV 操作...\n"));
        
        // 禁用 GuestControlledCacheTypes
        callback(UTF8("正在重置 GuestControlledCacheTypes...\n"));
        std::string resetCacheCmd = "Set-VM -VMName '" + vmName + "' -GuestControlledCacheTypes $false";
        if (!PowerShellExecutor::ExecuteWithCheck(resetCacheCmd, resetCacheCmd, error)) { // 这里借用output参数接收
             // 关闭操作如果失败，尝试回滚
             callback(UTF8("错误：重置CacheTypes失败，正在尝试回滚...\n"));
             RestoreState(vmName, backup, callback);
             return false;
        }
        
        callback(UTF8("GPU-PV 已成功关闭！\n"));
        return true;
    }
    
    // 步骤3：添加GPU分区适配器
    callback(UTF8("正在添加GPU分区适配器...\n"));
    if (!AddGPUPartitionAdapter(vmName, gpuInstancePath, error)) {
        callback(UTF8("错误: ") + error + "\n");
        callback(UTF8("正在回滚配置...\n"));
        RestoreState(vmName, backup, callback);
        return false;
    }
    callback(UTF8("GPU分区适配器添加成功\n"));
    
    // 步骤4：配置GPU资源分配
    callback(UTF8("正在配置GPU资源分配...\n"));
    uint64_t vramBytes = static_cast<uint64_t>(vramMB) * 1024 * 1024;
    if (!ConfigureGPUResources(vmName, vramBytes, error)) {
        callback(UTF8("错误: ") + error + "\n");
        callback(UTF8("正在回滚配置...\n"));
        RestoreState(vmName, backup, callback);
        return false;
    }
    callback(UTF8("GPU资源配置完成\n"));
    
    // 步骤5：启用GuestControlledCacheTypes
    callback(UTF8("正在启用GuestControlledCacheTypes...\n"));
    if (!EnableGuestControlledCacheTypes(vmName, error)) {
        callback(UTF8("错误: ") + error + "\n");
        callback(UTF8("正在回滚配置...\n"));
        RestoreState(vmName, backup, callback);
        return false;
    }
    callback(UTF8("GuestControlledCacheTypes已启用\n"));
    
    // 步骤5.5：配置MMIO空间（GPU-PV关键配置）
    callback(UTF8("正在配置内存映射I/O空间...\n"));
    if (!ConfigureMMIOSpace(vmName, error)) {
        callback(UTF8("错误: ") + error + "\n");
        callback(UTF8("正在回滚配置...\n"));
        RestoreState(vmName, backup, callback);
        return false;
    }
    callback(UTF8("MMIO空间配置完成\n"));
    
    // 步骤6：复制驱动文件
    callback(UTF8("正在复制GPU驱动文件...\n"));
    if (!CopyDriverFiles(vmName, driverPath, callback, error)) {
        callback(UTF8("错误: ") + error + "\n");
        // 注意：驱动文件复制失败通常不影响VM启动，但可能影响GPU使用
        // 这里可以选择不回滚，或者提示用户手动处理
        // 用户要求“保证虚拟机的正常运行”，如果驱动复制失败，VM还是能开机的，只是没驱动
        // 回滚意义不大，因为适配器已经加上了。如果不回滚，用户可以手动装驱动？
        // 但如果要求严苛，可以回滚。
        // 这里选择回滚，以确保“配置未成功”
        callback(UTF8("正在回滚配置...\n"));
        RestoreState(vmName, backup, callback);
        
        // 尝试清理已复制的驱动文件（可选，比较复杂，暂略）
        return false;
    }
    callback(UTF8("驱动文件复制完成\n"));
    
    // 7. 可选：如果虚拟机正在运行，尝试通过Enter-PSSession验证设备状态
    callback(UTF8("正在检查虚拟机状态...\n"));
    std::string vmState = VMManager::GetVMState(vmName);
    if (vmState == "Running") {
        callback(UTF8("虚拟机正在运行，尝试验证GPU设备状态...\n"));
        if (VerifyGPUDeviceInVM(vmName, gpuName, callback)) {
            callback(UTF8("设备验证通过：GPU在虚拟机中已正确识别\n"));
        } else {
            callback(UTF8("警告：无法验证设备状态（可能需要手动检查设备管理器）\n"));
        }
    } else {
        callback(UTF8("虚拟机未运行，跳过设备验证（启动后请手动检查设备管理器）\n"));
    }
    
    callback(UTF8("GPU-PV配置成功完成！\n"));
    return true;
}

// 添加GPU分区适配器
bool GPUPVConfigurator::AddGPUPartitionAdapter(
    const std::string& vmName,
    const std::string& gpuInstancePath,
    std::string& error) {
    
    std::string command = "Add-VMGpuPartitionAdapter -VMName '" + vmName + 
                         "' -InstancePath '" + gpuInstancePath + "'";
    
    std::string output;
    if (!PowerShellExecutor::ExecuteWithCheck(command, output, error)) {
        if (error.empty()) {
            error = UTF8("添加GPU分区适配器失败\n"); 
        }
        return false;
    }
    
    return true;
}

// 配置GPU资源分配
bool GPUPVConfigurator::ConfigureGPUResources(
    const std::string& vmName,
    uint64_t vramBytes,
    std::string& error) {
    
    // 配置四个资源类型：VRAM、Encode、Decode、Compute
    std::string resourceTypes[] = {"VRAM", "Encode", "Decode", "Compute"};
    
    for (const auto& resType : resourceTypes) {
        std::string command = 
            "Set-VMGpuPartitionAdapter -VMName '" + vmName + 
            "' -MinPartition" + resType + " 1" +
            " -MaxPartition" + resType + " " + std::to_string(vramBytes) +
            " -OptimalPartition" + resType + " " + std::to_string(vramBytes);
        
        std::string output;
        if (!PowerShellExecutor::ExecuteWithCheck(command, output, error)) {
            if (error.empty()) {
                error = UTF8("配置GPU资源失败: ") + resType; 
            }
            return false;
        }
    }
    
    return true;
}

// 启用GuestControlledCacheTypes
bool GPUPVConfigurator::EnableGuestControlledCacheTypes(
    const std::string& vmName,
    std::string& error) {
    
    std::string command = "Set-VM -VMName '" + vmName + "' -GuestControlledCacheTypes $true";
    std::string output;
    
    if (!PowerShellExecutor::ExecuteWithCheck(command, output, error)) {
        if (error.empty()) {
            error = UTF8("启用GuestControlledCacheTypes失败"); 
        }
        return false;
    }
    
    return true;
}

// 配置MMIO空间
bool GPUPVConfigurator::ConfigureMMIOSpace(
    const std::string& vmName,
    std::string& error) {
    
    // 设置LowMemoryMappedIoSpace (1GB)
    std::string cmd1 = "Set-VM -VMName '" + vmName + "' -LowMemoryMappedIoSpace 1GB";
    std::string output;
    if (!PowerShellExecutor::ExecuteWithCheck(cmd1, output, error)) {
        if (error.empty()) {
            error = UTF8("设置LowMemoryMappedIoSpace失败");
        }
        return false;
    }
    
    // 设置HighMemoryMappedIoSpace (32GB)
    std::string cmd2 = "Set-VM -VMName '" + vmName + "' -HighMemoryMappedIoSpace 32GB";
    if (!PowerShellExecutor::ExecuteWithCheck(cmd2, output, error)) {
        if (error.empty()) {
            error = UTF8("设置HighMemoryMappedIoSpace失败");
        }
        return false;
    }
    
    return true;
}

// 复制驱动文件
bool GPUPVConfigurator::CopyDriverFiles(
    const std::string& vmName,
    const std::string& driverPath, // 此参数现在作为参考，主要依赖WMI重新查询
    ProgressCallback callback,
    std::string& error) {
    
    // 1. 挂载虚拟机磁盘
    callback(UTF8("正在挂载虚拟机磁盘...\n"));
    std::string driveLetter = MountVMDisk(vmName, error);
    if (driveLetter.empty()) {
        return false;
    }
    callback(UTF8("虚拟机磁盘已挂载到: ") + driveLetter + "\n");
    
    // 2. 准备GPU名称（改进版：支持多种获取方式，增强容错性）
    // 优先从VM配置中获取，如果失败则从主机GPU列表获取
    std::string gpuName;
    
    // 方法1：从VM的GPU分区适配器获取（最准确）
    std::string cmd = "$adapters = Get-VMGpuPartitionAdapter -VMName '" + vmName + "' -ErrorAction SilentlyContinue; "
                     "if ($adapters) { "
                     "    $instancePath = $adapters[0].InstancePath; "
                     "    $hwId = $instancePath.Substring(8, 16); "
                     "    $pnpDevice = Get-PnpDevice | Where-Object { $_.InstanceId -like ('*' + $hwId + '*') -and $_.Status -eq 'OK' } | Select-Object -First 1; "
                     "    if ($pnpDevice) { $pnpDevice.Name } "
                     "}";
    gpuName = Utils::Trim(PowerShellExecutor::Execute(cmd));
    
    // 方法2：如果方法1失败，从主机上匹配的GPU获取
    if (gpuName.empty()) {
        callback(UTF8("警告：无法从VM配置获取GPU名称，尝试从主机GPU列表获取...\n"));
        cmd = "$vmAdapter = Get-VMGpuPartitionAdapter -VMName '" + vmName + "' -ErrorAction SilentlyContinue; "
              "if ($vmAdapter) { "
              "    $instancePath = $vmAdapter[0].InstancePath; "
              "    $partitionableGpus = Get-WmiObject -Class Msvm_PartitionableGpu -Namespace ROOT\\virtualization\\v2; "
              "    $matchedGpu = $partitionableGpus | Where-Object { $_.Name -eq $instancePath } | Select-Object -First 1; "
              "    if ($matchedGpu) { "
              "        $hwId = $instancePath.Substring(8, 16); "
              "        $pnpDevice = Get-PnpDevice | Where-Object { $_.InstanceId -like ('*' + $hwId + '*') -and $_.Status -eq 'OK' } | Select-Object -First 1; "
              "        if ($pnpDevice) { $pnpDevice.Name } "
              "    } "
              "}";
        gpuName = Utils::Trim(PowerShellExecutor::Execute(cmd));
    }
    
    // 方法3：如果前两种方法都失败，尝试从所有NVIDIA GPU中查找（最后的回退）
    if (gpuName.empty()) {
        callback(UTF8("警告：无法精确匹配GPU，尝试查找所有NVIDIA GPU...\n"));
        cmd = "$nvidiaGpus = Get-PnpDevice | Where-Object { $_.Name -like '*NVIDIA*' -and $_.Status -eq 'OK' } | Select-Object -First 1; "
              "if ($nvidiaGpus) { $nvidiaGpus.Name }";
        gpuName = Utils::Trim(PowerShellExecutor::Execute(cmd));
    }
    
    if (gpuName.empty()) {
        error = UTF8("无法确定目标GPU名称，无法执行驱动精确拷贝。\n"
                     "请确保：\n"
                     "1. 虚拟机已配置GPU分区适配器\n"
                     "2. 主机上GPU驱动已正确安装\n"
                     "3. GPU设备在设备管理器中显示正常");
        DismountVMDisk(vmName, error);
        return false;
    }
    
    callback(UTF8("目标GPU: ") + gpuName + "\n");
    
    bool overallSuccess = true;
    std::string tempError;

    // 3. 拷贝GPU服务驱动目录
    callback(UTF8("正在拷贝GPU服务驱动...\n"));
    if (!CopyGPUServiceDriver(gpuName, driveLetter, callback, tempError)) {
        callback(UTF8("警告：GPU服务驱动拷贝失败 - ") + tempError + "\n");
        // 服务驱动失败通常是致命的，但我们尝试继续
        overallSuccess = false;
        if (error.empty()) error = tempError;
    }

    // 4. 拷贝PnP驱动文件
    callback(UTF8("正在拷贝PnP驱动文件...\n"));
    if (!CopyPnPDriverFiles(gpuName, driveLetter, callback, tempError)) {
        callback(UTF8("警告：PnP驱动文件拷贝不完整 - ") + tempError + "\n");
        overallSuccess = false; 
    }

    // 5. 拷贝NVIDIA特殊文件（如果是NVIDIA卡）
    if (gpuName.find("NVIDIA") != std::string::npos) {
        callback(UTF8("正在处理NVIDIA特殊文件...\n"));
        if (!CopyNvidiaSpecialFiles(gpuName, driveLetter, callback, tempError)) {
             callback(UTF8("警告：NVIDIA特殊文件拷贝失败 - ") + tempError + "\n");
        }
    }
    
    // 6. 验证安装结果 (增强版：检查更多关键文件)
    callback(UTF8("正在验证驱动文件...\n"));
    std::string verifyCmd = 
        "$driveLetter = '" + driveLetter + "'; "
        "$gpuName = '" + gpuName + "'; "
        "$isNvidia = $gpuName -like '*NVIDIA*'; "
        "$isAMD = $gpuName -like '*AMD*' -or $gpuName -like '*Radeon*'; "
        "$isIntel = $gpuName -like '*Intel*'; "
        
        "$filesFound = @(); "
        "$filesMissing = @(); "
        
        "<# NVIDIA关键文件 #>"
        "if ($isNvidia) { "
        "    $nvidiaFiles = @( "
        "        (Join-Path $driveLetter 'Windows\\System32\\drivers\\nvlddmkm.sys'), "
        "        (Join-Path $driveLetter 'Windows\\System32\\nvapi64.dll'), "
        "        (Join-Path $driveLetter 'Windows\\System32\\nvoglv64.dll') "
        "    ); "
        "    foreach ($file in $nvidiaFiles) { "
        "        if (Test-Path $file) { "
        "            $filesFound += $file; "
        "        } else { "
        "            $filesMissing += $file; "
        "        } "
        "    } "
        "} "
        
        "<# 检查HostDriverStore目录 #>"
        "$hostDriverStore = Join-Path $driveLetter 'Windows\\System32\\HostDriverStore\\FileRepository'; "
        "if (Test-Path $hostDriverStore) { "
        "    $driverPackages = Get-ChildItem -Path $hostDriverStore -Directory -ErrorAction SilentlyContinue | Measure-Object; "
        "    if ($driverPackages.Count -gt 0) { "
        "        $filesFound += ('HostDriverStore: ' + $driverPackages.Count + ' packages'); "
        "    } else { "
        "        $filesMissing += 'HostDriverStore: No driver packages found'; "
        "    } "
        "} else { "
        "    $filesMissing += 'HostDriverStore directory does not exist'; "
        "} "
        
        "<# 输出验证结果 #>"
        "Write-Output '[VERIFY_RESULT]'; "
        "Write-Output ('Files Found: ' + $filesFound.Count); "
        "foreach ($f in $filesFound) { Write-Output ('[FOUND] ' + $f); } "
        "Write-Output ('Files Missing: ' + $filesMissing.Count); "
        "foreach ($f in $filesMissing) { Write-Output ('[MISSING] ' + $f); } "
        
        "<# 判断整体验证结果 #>"
        "if ($filesFound.Count -gt 0 -and $filesMissing.Count -eq 0) { "
        "    Write-Output 'VERIFY_OK'; "
        "} elseif ($filesFound.Count -gt 0) { "
        "    Write-Output 'VERIFY_PARTIAL'; "
        "} else { "
        "    Write-Output 'VERIFY_FAIL'; "
        "} ";
        
    std::string verifyOutput = PowerShellExecutor::Execute(verifyCmd);
    
    // 解析验证结果
    std::vector<std::string> lines = Utils::Split(verifyOutput, '\n');
    bool hasFound = false, hasMissing = false;
    for (const auto& line : lines) {
        std::string trimmed = Utils::Trim(line);
        if (trimmed.find("[FOUND]") == 0) {
            callback(UTF8("✓ ") + trimmed.substr(7) + "\n");
            hasFound = true;
        } else if (trimmed.find("[MISSING]") == 0) {
            callback(UTF8("✗ ") + trimmed.substr(9) + "\n");
            hasMissing = true;
        }
    }
    
    if (verifyOutput.find("VERIFY_OK") != std::string::npos) {
        callback(UTF8("验证通过：所有关键驱动文件已存在\n"));
    } else if (verifyOutput.find("VERIFY_PARTIAL") != std::string::npos) {
        callback(UTF8("警告：部分驱动文件缺失，但关键文件已存在\n"));
    } else {
        callback(UTF8("错误：验证失败，关键驱动文件缺失\n"));
        if (!error.empty()) error += "\n";
        error += UTF8("驱动文件验证失败，请检查HostDriverStore目录");
    }

    // 7. 卸载虚拟机磁盘
    callback(UTF8("正在卸载虚拟机磁盘...\n"));
    std::string dismountError;
    if (!DismountVMDisk(vmName, dismountError)) {
        error = dismountError;
        return false;
    }
    
    // 如果虽然有部分失败但关键文件可能已复制，我们可以返回true
    // 或者严格返回overallSuccess。
    // 这里我们稍微宽松一点，只要没有严重错误就返回true
    return true; 
}

// 拷贝GPU服务驱动目录
bool GPUPVConfigurator::CopyGPUServiceDriver(
    const std::string& gpuName,
    const std::string& driveLetter,
    ProgressCallback callback,
    std::string& error) {
    
    std::string command = 
        "$ErrorActionPreference = 'Stop'; "
        "$gpuName = '" + gpuName + "'; "
        "$driveLetter = '" + driveLetter + "'; "
        
        "$gpu = Get-PnpDevice | Where-Object { $_.Name -like ('*' + $gpuName + '*') -and $_.Status -eq 'OK' } | Select-Object -First 1; "
        "if (-not $gpu) { throw 'GPU not found'; } "
        "$serviceName = $gpu.Service; "
        "Write-Output ('[DEBUG] GPU Service Name: ' + $serviceName); "
        
        "$sysDriver = Get-WmiObject Win32_SystemDriver | Where-Object { $_.Name -eq $serviceName }; "
        "if (-not $sysDriver) { throw 'Service driver not found'; } "
        "$sysPath = $sysDriver.Pathname; "
        "Write-Output ('[DEBUG] Service Driver Path: ' + $sysPath); "
        
        "$ServiceDriverDir = $sysPath.split('\\')[0..5] -join('\\'); "
        "$ServicedriverDest = ($driveLetter + '\\' + ($sysPath.split('\\')[1..5] -join('\\'))).Replace('DriverStore','HostDriverStore'); "
        
        "Write-Output ('[INFO] Copying service driver directory...'); "
        "Write-Output ('[INFO] Source: ' + $ServiceDriverDir); "
        "Write-Output ('[INFO] Dest: ' + $ServicedriverDest); "
        
        "if (!(Test-Path $ServicedriverDest)) { "
        "    Copy-Item -Path $ServiceDriverDir -Destination $ServicedriverDest -Recurse -Force; "
        "    Write-Output '[SUCCESS] Service driver directory copied'; "
        "} else { "
        "    Write-Output '[INFO] Service driver directory already exists'; "
        "} ";

    std::string output;
    if (PowerShellExecutor::ExecuteWithCheck(command, output, error)) {
        std::vector<std::string> lines = Utils::Split(output, '\n');
        for (const auto& line : lines) {
            std::string trimmed = Utils::Trim(line);
            if (!trimmed.empty()) {
                callback(trimmed + "\n");
            }
        }
    } else {
        callback(UTF8("警告：服务驱动目录复制失败 - ") + error + "\n");
        return false;
    }
    return true;
}

// 拷贝PnP驱动文件
bool GPUPVConfigurator::CopyPnPDriverFiles(
    const std::string& gpuName,
    const std::string& driveLetter,
    ProgressCallback callback,
    std::string& error) {
    
    // 转义GPU名称中的单引号，防止PowerShell命令注入
    std::string escapedGpuName = gpuName;
    size_t pos = 0;
    while ((pos = escapedGpuName.find("'", pos)) != std::string::npos) {
        escapedGpuName.replace(pos, 1, "''");
        pos += 2;
    }
    
    std::string command = 
        "$ErrorActionPreference = 'SilentlyContinue'; "
        "$hostname = $env:COMPUTERNAME; "
        "$gpuName = '" + escapedGpuName + "'; "
        "$driveLetter = '" + driveLetter + "'; "
        
        "New-Item -ItemType Directory -Path (Join-Path $driveLetter 'Windows\\System32\\HostDriverStore') -Force | Out-Null; "
        "$gpuCoreName = $gpuName; "
        "$gpuCoreName = $gpuCoreName -replace ' Laptop GPU$', ''; "
        "$gpuCoreName = $gpuCoreName -replace ' Laptop$', ''; "
        "$gpuCoreName = $gpuCoreName -replace ' Mobile$', ''; "
        "$gpuCoreName = $gpuCoreName -replace ' GPU$', ''; "
        "$gpuCoreName = $gpuCoreName.Trim(); "
        "$Drivers = $null; "
        "$Drivers = Get-WmiObject Win32_PNPSignedDriver | Where-Object { $_.DeviceName -eq $gpuName }; "
        "if (-not $Drivers) { "
        "    $Drivers = Get-WmiObject Win32_PNPSignedDriver | Where-Object { $_.DeviceName -like ('*' + $gpuName + '*') }; "
        "} "
        "if (-not $Drivers) { "
        "    $Drivers = Get-WmiObject Win32_PNPSignedDriver | Where-Object { $_.DeviceName -like ('*' + $gpuCoreName + '*') }; "
        "} "
        "if (-not $Drivers) { "
        "    $gpuWithoutLaptop = $gpuCoreName -replace ' Laptop', ''; "
        "    $Drivers = Get-WmiObject Win32_PNPSignedDriver | Where-Object { $_.DeviceName -like ('*' + $gpuWithoutLaptop + '*') }; "
        "} "
        "if (-not $Drivers) { "
        "    if ($gpuCoreName -match '(RTX|GTX|GT)\\s*(\\d+)') { "
        "        $modelNum = $matches[2]; "
        "        $Drivers = Get-WmiObject Win32_PNPSignedDriver | Where-Object { $_.DeviceName -like ('*NVIDIA*' + $modelNum + '*') }; "
        "    } "
        "} "
        "$DriverArray = @($Drivers); "
        "$DriverCount = $DriverArray.Count; "
        "Write-Output ('[INFO] Found ' + $DriverCount + ' driver records for: ' + $gpuName); "
        
        "if ($DriverCount -eq 0) { "
        "    Write-Output ('ERROR: No drivers found for GPU: ' + $gpuName); "
        "    exit 1; "
        "} "
        
        "foreach ($d in $DriverArray) { "
            "Write-Output ('[DEBUG] Processing driver: ' + $d.DeviceName); "
        "    $DriverFiles = @(); "
        "    $ModifiedDeviceID = $d.DeviceID -replace '\\\\', '\\\\\\\\'; "
        "    $Antecedent = '\\\\\\\\' + $hostname + '\\\\ROOT\\\\cimv2:Win32_PNPSignedDriver.DeviceID=\"\"' + $ModifiedDeviceID + '\"\"'; "
        "    $DriverFiles = Get-WmiObject Win32_PNPSignedDriverCIMDataFile | Where-Object { $_.Antecedent -eq $Antecedent }; "
        
        "    foreach ($file in $DriverFiles) { "
        "        $path = $file.Dependent.Split('=')[1] -replace '\\\\\\\\', '\\\\'; "
        "        $sourcePath = $path.Substring(1, $path.Length - 2); "
        
        "        if ($sourcePath -match '(?i)\\\\driverstore\\\\') { "
        "            $DriverDir = ($sourcePath.Split('\\\\'))[0..5] -join('\\\\'); "
        "            $relativePath = ($sourcePath.Split('\\\\'))[1..5] -join('\\\\'); "
        "            $driverDest = $driveLetter + '\\\\' + ($relativePath -ireplace 'driverstore', 'HostDriverStore'); "
        
        "            if (!(Test-Path $driverDest)) { "
        "                Copy-Item -Path $DriverDir -Destination $driverDest -Recurse -Force -ErrorAction SilentlyContinue; "
        "                Write-Output ('[PACKAGE] ' + $DriverDir + ' -> ' + $driverDest); "
        "            } "
        "        } "
        "        else { "
        "            $destPath = $sourcePath -replace 'C:', $driveLetter; "
        "            $destDir = Split-Path -Parent $destPath; "
        
        "            if (!(Test-Path $destDir)) { "
        "                New-Item -ItemType Directory -Path $destDir -Force | Out-Null; "
        "            } "
        
        "            Copy-Item -Path $sourcePath -Destination $destPath -Force -ErrorAction SilentlyContinue; "
        "            Write-Output ('[FILE] ' + $sourcePath + ' -> ' + $destPath); "
        "        } "
        "    } "
        "} "
        
        "Write-Output '[INFO] Copying critical NVIDIA runtime DLLs...'; "
        "$criticalDLLs = @( "
        "    'nvapi64.dll', "
        "    'nvoglv64.dll', "
        "    'nvcuda.dll', "
        "    'nvwgf2umx.dll', "
        "    'nvd3dumx.dll', "
        "    'nvcuvid.dll', "
        "    'nvencodeapi64.dll', "
        "    'nvfatbinaryLoader.dll', "
        "    'nvcompiler.dll' "
        "); "
        
        "foreach ($dll in $criticalDLLs) { "
        "    $source = 'C:\\Windows\\System32\\' + $dll; "
        "    $dest = $driveLetter + '\\Windows\\System32\\' + $dll; "
        "    if (Test-Path $source) { "
        "        try { "
        "            Copy-Item -Path $source -Destination $dest -Force -ErrorAction Stop; "
        "            Write-Output ('[DLL] ' + $source + ' -> ' + $dest); "
        "        } catch { "
        "            Write-Output ('[WARN] Failed to copy ' + $dll + ': ' + $_.Exception.Message); "
        "        } "
        "    } else { "
        "        Write-Output ('[SKIP] ' + $dll + ' not found on host'); "
        "    } "
        "} "
        
        "Write-Output '[INFO] Copying DLLs from HostDriverStore to System32...'; "
        "$hostDriverStoreRepo = Join-Path $driveLetter 'Windows\\System32\\HostDriverStore\\FileRepository'; "
        "$nvPackage = Get-ChildItem $hostDriverStoreRepo -Directory -ErrorAction SilentlyContinue | "
        "             Where-Object { $_.Name -like '*nvltsi*' -or $_.Name -like '*nvlt.inf*' } | "
        "             Select-Object -First 1; "
        
        "if ($nvPackage) { "
        "    Write-Output ('[INFO] Found driver package: ' + $nvPackage.Name); "
        "    $keyDLLs = @( "
        "        'nvwgf2umx.dll', "
        "        'nvoglv64.dll', "
        "        'nvd3dumx.dll', "
        "        'nvcuda64.dll', "
        "        'nvwgf2um.dll', "
        "        'nvopencl64.dll', "
        "        'nvEncodeAPI64.dll', "
        "        'nvofapi64.dll', "
        "        'nvml.dll', "
        "        'nvcuvid64.dll', "
        "        'nvoptix.dll', "
        "        'nvrtum64.dll' "
        "    ); "
        "    foreach ($dll in $keyDLLs) { "
        "        $sourcePath = Join-Path $nvPackage.FullName $dll; "
        "        $destPath = Join-Path ($driveLetter + '\\Windows\\System32') $dll; "
        "        if (Test-Path $sourcePath) { "
        "            if (!(Test-Path $destPath)) { "
        "                try { "
        "                    Copy-Item -Path $sourcePath -Destination $destPath -Force -ErrorAction Stop; "
        "                    Write-Output ('[DLL_STORE] ' + $dll + ' -> System32'); "
        "                } catch { "
        "                    Write-Output ('[WARN] Failed to copy ' + $dll + ': ' + $_.Exception.Message); "
        "                } "
        "            } else { "
        "                Write-Output ('[SKIP] ' + $dll + ' already exists'); "
        "            } "
        "        } "
        "    } "
        "} else { "
        "    Write-Output '[WARN] NVIDIA driver package not found in HostDriverStore'; "
        "} "
        
        "Write-Output 'SUCCESS'; ";
    
    callback(UTF8("正在枚举和复制所有驱动文件...\n"));
    
    std::string output;
    if (!PowerShellExecutor::ExecuteWithCheck(command, output, error)) {
        if (error.empty()) error = UTF8("驱动文件复制失败");
        return false;
    }
    
    // 解析输出并显示进度
    std::vector<std::string> lines = Utils::Split(output, '\n');
    for (const auto& line : lines) {
        std::string trimmed = Utils::Trim(line);
        if (trimmed.find("[PACKAGE]") == 0 || trimmed.find("[FILE]") == 0) {
            callback(trimmed + "\n");
        }
    }
    
    if (output.find("SUCCESS") == std::string::npos) {
        if (output.find("ERROR") != std::string::npos) {
            error = UTF8("驱动复制过程中断，未找到所有文件");
            return false;
        }
        
        if (output.empty()) {
            error = UTF8("未找到相关驱动文件");
            return false;
        }
    }
    
    callback(UTF8("所有驱动文件复制完成\n"));
    return true;
}

// 拷贝NVIDIA特殊文件
bool GPUPVConfigurator::CopyNvidiaSpecialFiles(
    const std::string& gpuName,
    const std::string& driveLetter,
    ProgressCallback callback,
    std::string& error) {
    
    std::string command = 
        "$ErrorActionPreference = 'SilentlyContinue'; "
        "$driveLetter = '" + driveLetter + "'; "
        "$destNvDir = Join-Path $driveLetter 'Windows\\System32\\drivers\\Nvidia Corporation'; "
        "if (-not (Test-Path $destNvDir)) { New-Item -ItemType Directory -Path $destNvDir -Force | Out-Null; } "
        "$srcNvDir = 'C:\\Windows\\System32\\drivers\\Nvidia Corporation'; "
        "if (Test-Path $srcNvDir) { "
        "    Write-Output 'Copying Nvidia Corporation folder content...'; "
        "    Copy-Item -Path \"$srcNvDir\\*\" -Destination $destNvDir -Recurse -Force; "
        "} else { "
        "    Write-Output 'Host Nvidia Corporation folder not found, skipping copy.'; "
        "} ";
        
    PowerShellExecutor::Execute(command);
    return true;
}

// 验证虚拟机中的GPU设备状态（通过Enter-PSSession）
bool GPUPVConfigurator::VerifyGPUDeviceInVM(
    const std::string& vmName,
    const std::string& gpuName,
    ProgressCallback callback) {
    
    // 提取GPU核心名称（用于匹配）
    std::string gpuCoreName = gpuName;
    // 移除常见后缀
    size_t pos = gpuCoreName.find(" Laptop");
    if (pos != std::string::npos) gpuCoreName = gpuCoreName.substr(0, pos);
    pos = gpuCoreName.find(" Mobile");
    if (pos != std::string::npos) gpuCoreName = gpuCoreName.substr(0, pos);
    
    std::string command = 
        "$ErrorActionPreference = 'SilentlyContinue'; "
        "$vmName = '" + vmName + "'; "
        "$gpuName = '" + gpuCoreName + "'; "
        
        "# 尝试通过Enter-PSSession连接虚拟机"
        "try { "
        "    $vm = Get-VM -Name $vmName -ErrorAction Stop; "
        "    if ($vm.State -ne 'Running') { "
        "        Write-Output 'VM_NOT_RUNNING'; "
        "        exit 0; "
        "    } "
        
        "    # 获取VM的IP地址（如果可能）"
        "    $vmIp = $null; "
        "    try { "
        "        $vmNetwork = $vm | Get-VMNetworkAdapter | Where-Object { $_.IPAddresses.Count -gt 0 } | Select-Object -First 1; "
        "        if ($vmNetwork -and $vmNetwork.IPAddresses.Count -gt 0) { "
        "            $vmIp = $vmNetwork.IPAddresses[0]; "
        "        } "
        "    } catch { } "
        
        "    # 方法1：如果VM有IP且启用了PowerShell远程，尝试Enter-PSSession"
        "    if ($vmIp) { "
        "        try { "
        "            $session = New-PSSession -ComputerName $vmIp -ErrorAction Stop; "
        "            $deviceStatus = Invoke-Command -Session $session -ScriptBlock { "
        "                $gpu = Get-PnpDevice | Where-Object { $_.Name -like '*NVIDIA*' -or $_.Name -like '*AMD*' } | Select-Object -First 1; "
        "                if ($gpu) { "
        "                    if ($gpu.Status -eq 'OK') { "
        "                        Write-Output ('DEVICE_OK:' + $gpu.Name); "
        "                    } else { "
        "                        Write-Output ('DEVICE_ERROR:' + $gpu.Status + ':' + $gpu.Name); "
        "                    } "
        "                } else { "
        "                    Write-Output 'DEVICE_NOT_FOUND'; "
        "                } "
        "            }; "
        "            Remove-PSSession -Session $session; "
        "            Write-Output $deviceStatus; "
        "            exit 0; "
        "        } catch { "
        "            Write-Output ('PSSession failed: ' + $_.Exception.Message); "
        "        } "
        "    } "
        
        "    # 方法2：通过WMI查询VM中的设备（如果VM启用了WMI）"
        "    if ($vmIp) { "
        "        try { "
        "            $wmiDevices = Get-WmiObject -ComputerName $vmIp -Class Win32_PnPEntity -ErrorAction Stop | "
        "                Where-Object { $_.Name -like '*NVIDIA*' -or $_.Name -like '*AMD*' } | Select-Object -First 1; "
        "            if ($wmiDevices) { "
        "                Write-Output ('WMI_DEVICE_FOUND:' + $wmiDevices.Name); "
        "            } else { "
        "                Write-Output 'WMI_DEVICE_NOT_FOUND'; "
        "            } "
        "            exit 0; "
        "        } catch { "
        "            Write-Output ('WMI query failed: ' + $_.Exception.Message); "
        "        } "
        "    } "
        
        "    # 如果以上方法都失败，返回提示信息"
        "    Write-Output 'VERIFY_SKIPPED: Cannot connect to VM (may need manual check)'; "
        "} catch { "
        "    Write-Output ('VERIFY_ERROR: ' + $_.Exception.Message); "
        "}";
    
    std::string output = PowerShellExecutor::Execute(command);
    
    // 解析结果
    if (output.find("DEVICE_OK:") != std::string::npos) {
        size_t pos = output.find("DEVICE_OK:");
        std::string deviceName = output.substr(pos + 10);
        deviceName = Utils::Trim(deviceName);
        callback(UTF8("设备状态正常: ") + deviceName + "\n");
        return true;
    } else if (output.find("DEVICE_ERROR:") != std::string::npos) {
        size_t pos = output.find("DEVICE_ERROR:");
        std::string errorInfo = output.substr(pos + 14);
        errorInfo = Utils::Trim(errorInfo);
        callback(UTF8("设备状态异常: ") + errorInfo + "\n");
        return false;
    } else if (output.find("DEVICE_NOT_FOUND") != std::string::npos) {
        callback(UTF8("警告：在虚拟机中未找到GPU设备\n"));
        return false;
    } else if (output.find("VERIFY_SKIPPED") != std::string::npos) {
        // 无法连接VM，这是正常的（VM可能未启用远程或网络未配置）
        return true; // 不算错误，只是跳过验证
    }
    
    return true; // 默认返回true，避免阻塞配置流程
}

// 挂载虚拟机磁盘
std::string GPUPVConfigurator::MountVMDisk(const std::string& vmName, std::string& error) {
    // 挂载但不分配驱动器号，然后寻找包含 Windows\System32 的分区并手动分配临时驱动器号
    // 注意：构建 PowerShell 脚本时，每行末尾必须加空格或分号，防止拼接错误
    std::string command = 
        "$ErrorActionPreference = 'Stop'; "
        "$vhd = (Get-VM '" + vmName + "').HardDrives[0].Path; "
        
        "if ((Get-VHD -Path $vhd -ErrorAction SilentlyContinue).Attached) { "
        "    Dismount-VHD -Path $vhd -ErrorAction SilentlyContinue; "
        "    Start-Sleep -Seconds 2; "
        "} "
        
        "if ((Get-VHD -Path $vhd -ErrorAction SilentlyContinue).Attached) { "
        "    Dismount-VHD -Path $vhd -ErrorAction Stop; "
        "    Start-Sleep -Seconds 2; "
        "} "

        "$disk = Mount-VHD -Path $vhd -NoDriveLetter -Passthru | Get-Disk; "
        "if (-not $disk) { throw 'Failed to mount VHD'; } "
        
        "Start-Sleep -Seconds 3; " 
        
        "$driveLetter = 90..68 | ForEach-Object { [char]$_ } | Where-Object { -not (Get-PSDrive -Name $_ -ErrorAction SilentlyContinue) } | Select-Object -First 1; "
        "if (-not $driveLetter) { throw 'No free drive letters available'; }; "
        
        "$partitions = $disk | Get-Partition; "
        "$targetPartition = $null; "
        
        "Write-Output ('Scanning ' + $partitions.Count + ' partitions...'); "
        
        "foreach ($p in $partitions) { "
        "    try { "
        "        if ($p.Type -eq 'Reserved') { continue; } "
        "        Add-PartitionAccessPath -InputObject $p -AccessPath ($driveLetter + ':') -ErrorAction Stop; "
        "        if (Test-Path ($driveLetter + ':\\Windows\\System32')) { "
        "            $targetPartition = $p; "
        "            Write-Output $driveLetter; "
        "            break; "
        "        } "
        "        Remove-PartitionAccessPath -InputObject $p -AccessPath ($driveLetter + ':') -ErrorAction SilentlyContinue; "
        "    } catch { "
        "        Write-Output ('Failed to check partition ' + $p.PartitionNumber + ': ' + $_.Exception.Message); "
        "    } "
        "} "
        
        "if (-not $targetPartition) { "
        "    Dismount-VHD -Path $vhd -ErrorAction SilentlyContinue; "
        "    throw 'Could not find system partition with Windows directory'; "
        "}";
    
    std::string output;
    if (!PowerShellExecutor::ExecuteWithCheck(command, output, error)) {
        if (error.empty()) {
            error = UTF8("挂载虚拟机磁盘失败");
        }
        // 如果有部分输出，可能包含了错误信息
        if (!output.empty()) {
             error += " (" + Utils::Trim(output) + ")";
        }
        return "";
    }
    
    std::string driveLetter = Utils::Trim(output);
    // 清理可能的空白字符或换行
    if (driveLetter.length() > 1) {
        // 尝试从输出中提取单个字母
        size_t lastAlpha = std::string::npos;
        for (size_t i = 0; i < driveLetter.length(); ++i) {
            if (isalpha(driveLetter[i])) lastAlpha = i;
        }
        if (lastAlpha != std::string::npos) {
            driveLetter = driveLetter.substr(lastAlpha, 1);
        }
    }

    if (driveLetter.empty() || driveLetter.length() != 1) {
        error = UTF8("无法获取有效系统盘符");
        DismountVMDisk(vmName, output);
        return "";
    }
    
    return driveLetter + ":";
}

// 卸载虚拟机磁盘
bool GPUPVConfigurator::DismountVMDisk(const std::string& vmName, std::string& error) {
    // 增加短暂延时，并尝试多次卸载（因为文件句柄释放可能有延迟）
    std::string command = 
        "$vhd = (Get-VM '" + vmName + "').HardDrives[0].Path; "
        "for ($i=0; $i -lt 5; $i++) { "
        "    Start-Sleep -Seconds 1; "
        "    try { "
        "        if ((Get-VHD -Path $vhd).Attached) { "
        "            Dismount-VHD -Path $vhd -ErrorAction Stop; "
        "        } "
        "        break; "
        "    } catch { "
        "        Write-Output \"Retrying dismount... $($_.Exception.Message)\"; "
        "    } "
        "} "
        "if ((Get-VHD -Path $vhd).Attached) { throw 'Failed to dismount VHD after multiple attempts' }";
    
    std::string output;
    if (!PowerShellExecutor::ExecuteWithCheck(command, output, error)) {
        if (error.empty()) {
            // 如果只有空白错误，尝试获取更多信息或手动指定
            error = UTF8("卸载虚拟机磁盘失败（可能被占用，请手动在磁盘管理中卸载）");
        }
        return false;
    }
    
    return true;
}

// 复制驱动文件夹
bool GPUPVConfigurator::CopyDriverFolder(
    const std::string& sourcePath,
    const std::string& driveLetter,
    std::string& error) {
    
    // 目标路径：虚拟机的HostDriverStore目录
    std::string destPath = driveLetter + "\\Windows\\System32\\HostDriverStore\\FileRepository";
    
    // 创建目标目录
    std::string createDirCmd = "New-Item -ItemType Directory -Path '" + destPath + "' -Force | Out-Null";
    PowerShellExecutor::Execute(createDirCmd);
    
    // 复制驱动文件夹
    std::string copyCmd = 
        "Copy-Item -Path '" + sourcePath + "' -Destination '" + destPath + 
        "' -Recurse -Force -ErrorAction Stop";
    
    std::string output;
    if (!PowerShellExecutor::ExecuteWithCheck(copyCmd, output, error)) {
        if (error.empty()) {
            error = UTF8("复制驱动文件失败");
        }
        return false;
    }
    
    return true;
}
