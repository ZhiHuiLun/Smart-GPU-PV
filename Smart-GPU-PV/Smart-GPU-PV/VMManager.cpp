#include "VMManager.h"
#include "GPUManager.h"
#include "PowerShellExecutor.h"
#include "WmiHelper.h"
#include "HyperVException.h"
#include "Utils.h"
#include <iostream>

// 获取所有虚拟机列表
std::vector<VMInfo> VMManager::GetAllVMs() {
    try {
        return GetAllVMsViaWMI();
    } catch (...) {
        // WMI失败，降级到PowerShell
        return GetAllVMsViaPowerShell();
    }
}

// 停止虚拟机
bool VMManager::StopVM(const std::string& vmName, std::string& error) {
    try {
        return StopVMViaWMI(vmName, error);
    } catch (...) {
        return StopVMViaPowerShell(vmName, error);
    }
}

// 启动虚拟机
bool VMManager::StartVM(const std::string& vmName, std::string& error) {
    try {
        return StartVMViaWMI(vmName, error);
    } catch (...) {
        return StartVMViaPowerShell(vmName, error);
    }
}

// WMI实现：获取所有虚拟机
std::vector<VMInfo> VMManager::GetAllVMsViaWMI() {
    std::vector<VMInfo> vms;
    
    try {
        WmiHelper::Session session(L"root\\virtualization\\v2");
        
        // 查询所有计算机系统（包括主机和虚拟机）
        // 移除 WHERE Caption='Virtual Machine' 以避免本地化问题（例如中文系统下可能是 "虚拟机"）
        auto result = WmiHelper::Query(session, 
            L"SELECT * FROM Msvm_ComputerSystem");
        
        IWbemClassObject* pVM = nullptr;
        while (result->Next(&pVM)) {
            std::wstring caption = WmiHelper::GetProperty(pVM, L"Caption");
            std::wstring description = WmiHelper::GetProperty(pVM, L"Description");
            
            // 简单的过滤逻辑：
            // 1. 如果 Caption 是 "Virtual Machine" (英文) -> 是虚拟机
            // 2. 如果 Description 包含 "Virtual Machine" -> 是虚拟机
            // 3. 排除主机：主机的 Name 通常是计算机名，而虚拟机的 Name 是 GUID
            // 这里我们采用最宽松的策略：如果 Caption 不包含 "Host" 且 Name 看起来像 GUID，或者 Caption 包含 "Virtual Machine"/"虚拟机"
            
            // 更稳健的方式：检查 Name 是否为 GUID 格式。
            // 虚拟机的 Name 属性是 GUID，例如 "5B3B0A..."
            std::wstring name = WmiHelper::GetProperty(pVM, L"Name");
            bool isGuid = (name.length() == 36 || name.length() == 38); // 简单长度检查
            
            // 如果不是 GUID 且 Caption 不包含 "Virtual"/"虚拟机"，则跳过（可能是主机）
            if (!isGuid && caption.find(L"Virtual") == std::wstring::npos && caption.find(L"虚拟机") == std::wstring::npos) {
                pVM->Release();
                continue;
            }

            VMInfo vmInfo;
            vmInfo.strName = Utils::WStringToString(WmiHelper::GetProperty(pVM, L"ElementName"));
            
            // 获取状态
            uint64_t enabledState = WmiHelper::GetPropertyUInt64(pVM, L"EnabledState");
            switch (enabledState) {
                case 2: vmInfo.strState = "Running"; break;
                case 3: vmInfo.strState = "Off"; break;
                case 9: vmInfo.strState = "Paused"; break;
                case 6: vmInfo.strState = "Saved"; break;
                default: vmInfo.strState = "Unknown"; break;
            }
            
            // 检查虚拟机世代
            std::wstring vmPath = WmiHelper::GetObjectPath(pVM);
            
            // 查询虚拟机设置数据
            // Msvm_ComputerSystem -> Msvm_VirtualSystemSettingData (通过 Msvm_SettingsDefineState 关联)
            std::wstring settingQuery = L"ASSOCIATORS OF {" + vmPath + 
                L"} WHERE AssocClass=Msvm_SettingsDefineState ResultClass=Msvm_VirtualSystemSettingData";
            auto settingResult = WmiHelper::Query(session, settingQuery);
            
            IWbemClassObject* pSetting = nullptr;
            if (settingResult->Next(&pSetting)) {
                // VirtualSystemSubType 用来判断世代，但在某些系统上可能为空或格式不同
                // 更可靠的方式是检查 VirtualSystemIdentifier 或 DefaultSecureBootEnabled 等
                // 但这里暂时保持原样，如果获取不到默认为 Generation 2
                std::wstring subType = WmiHelper::GetProperty(pSetting, L"VirtualSystemSubType");
                bool isGen2 = (subType.find(L"Microsoft:Hyper-V:SubType:2") != std::wstring::npos);
                
                // 如果为空，可能是Gen1或旧版本，暂时放宽检查
                if (subType.empty()) isGen2 = true; 

                // 检查GPU分区适配器
                // Msvm_VirtualSystemSettingData -> Msvm_GpuPartitionSettingData (通过 Msvm_VirtualSystemSettingDataComponent 关联)
                std::wstring gpuQuery = L"ASSOCIATORS OF {" + WmiHelper::GetObjectPath(pSetting) +
                    L"} WHERE AssocClass=Msvm_VirtualSystemSettingDataComponent ResultClass=Msvm_GpuPartitionSettingData";
                auto gpuResult = WmiHelper::Query(session, gpuQuery);
                
                IWbemClassObject* pGpu = nullptr;
                if (gpuResult->Next(&pGpu)) {
                    vmInfo.strGPUStatus = "On";
                    vmInfo.ui64VramBytes = WmiHelper::GetPropertyUInt64(pGpu, L"MaxPartitionVRAM");
                    vmInfo.strGPUInstancePath = Utils::WStringToString(WmiHelper::GetProperty(pGpu, L"InstancePath"));
                    pGpu->Release();
                } else {
                    vmInfo.strGPUStatus = isGen2 ? "Off" : "Not supported";
                }
                
                pSetting->Release();
            } else {
                vmInfo.strGPUStatus = "Off";
            }
            
            vms.push_back(vmInfo);
            pVM->Release();
        }
    } catch (const std::exception& e) {
        // 如果失败，记录错误（这里没有日志系统，只能抛出让上层处理降级）
        // 实际上，如果没有任何虚拟机被枚举到，可能不是异常，而是真的没有，或者权限不足
        // 但如果这里抛出异常，就会降级到PowerShell
        throw HyperVException(std::string("WMI query failed: ") + e.what());
    }
    
    // 如果有GPU-PV开启的VM，获取GPU名称
    bool hasGpuPvOn = false;
    for (const auto& vm : vms) {
        if (vm.strGPUStatus == "On") {
            hasGpuPvOn = true;
            break;
        }
    }
    
    std::vector<GPUInfo> gpus;
    if (hasGpuPvOn) {
        gpus = GPUManager::GetPartitionableGPUs();
    }
    
    // 构建显示文本
    for (auto& vm : vms) {
        if (vm.strGPUStatus == "On" && !vm.strGPUInstancePath.empty()) {
            for (const auto& gpu : gpus) {
                if (vm.strGPUInstancePath == gpu.strInstancePath ||
                    gpu.strInstancePath.find(vm.strGPUInstancePath) != std::string::npos ||
                    vm.strGPUInstancePath.find(gpu.strInstancePath) != std::string::npos) {
                    vm.strName = gpu.strFriendlyName;
                    break;
                }
            }
        }
        
        vm.strDisplayText = vm.strName + "(" + vm.strState + ")  [";
        if (vm.strGPUStatus == "On") {
            vm.strDisplayText += "VRAM:" + Utils::FormatVRAMSize(vm.ui64VramBytes);
            if (!vm.strName.empty()) {
                vm.strDisplayText += " (" + vm.strName + ")";
            }
        } else if (vm.strGPUStatus == "Not supported") {
            vm.strDisplayText += "GPU-PV: Not supported";
        } else {
            vm.strDisplayText += "GPU-PV: Supported";
        }
        vm.strDisplayText += "]";
    }
    
    return vms;
}

// WMI实现：停止虚拟机
bool VMManager::StopVMViaWMI(const std::string& vmName, std::string& error) {
    try {
        WmiHelper::Session session(L"root\\virtualization\\v2");
        std::wstring wVmName = Utils::StringToWString(vmName);
        
        // 查找虚拟机 (移除 WHERE Caption 以避免本地化问题)
        auto result = WmiHelper::Query(session,
            L"SELECT * FROM Msvm_ComputerSystem WHERE ElementName='" + wVmName + L"'");
        
        IWbemClassObject* pVM = nullptr;
        bool found = false;
        
        while (result->Next(&pVM)) {
            // 简单的验证：检查 Name 是否为 GUID 格式，或者 Caption/Description 是否符合特征
            std::wstring name = WmiHelper::GetProperty(pVM, L"Name");
            std::wstring caption = WmiHelper::GetProperty(pVM, L"Caption");
            
            bool isGuid = (name.length() == 36 || name.length() == 38);
            if (isGuid || caption.find(L"Virtual") != std::wstring::npos || caption.find(L"虚拟机") != std::wstring::npos) {
                found = true;
                break; // 找到了
            }
            pVM->Release();
        }
        
        if (!found || !pVM) {
            error = "VM not found"; // Changed to English to avoid encoding issues
            return false;
        }
        
        // 检查当前状态
        uint64_t enabledState = WmiHelper::GetPropertyUInt64(pVM, L"EnabledState");
        if (enabledState == 3) { // 3 = Disabled/Off
            pVM->Release();
            return true; // Already off
        }
        
        std::wstring vmPath = WmiHelper::GetObjectPath(pVM);
        pVM->Release();
        
        // 调用RequestStateChange方法 (3 = Disabled/Off)
        IWbemClassObject* pInParams = WmiHelper::CreateMethodParams(session,
            L"Msvm_ComputerSystem", L"RequestStateChange");
        
        if (pInParams) {
            WmiHelper::SetParam(pInParams, L"RequestedState", (int)3); // 使用 int 类型
            
            IWbemClassObject* pOutParams = nullptr;
            HRESULT hr = WmiHelper::ExecuteMethod(session, vmPath, L"RequestStateChange", pInParams, &pOutParams);
            
            pInParams->Release();
            
            if (FAILED(hr)) {
                if (pOutParams) pOutParams->Release();
                char buffer[64];
                sprintf_s(buffer, "0x%08X", hr);
                error = "停止虚拟机失败 (WMI ExecMethod Error: " + std::string(buffer) + ")";
                return false;
            }
            
            // 检查方法的返回值 (ReturnValue)
            bool success = true;
            if (pOutParams) {
                uint64_t retVal = WmiHelper::GetPropertyUInt64(pOutParams, L"ReturnValue");
                if (retVal != 0 && retVal != 4096) { // 0=Success, 4096=Job Started
                    error = "停止虚拟机失败 (ReturnValue: " + std::to_string(retVal) + ")";
                    success = false;
                }
                pOutParams->Release(); // 使用完毕后释放
            }
            
            if (!success) return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        error = std::string("WMI错误: ") + e.what();
        throw;
    }
}

// WMI实现：启动虚拟机
bool VMManager::StartVMViaWMI(const std::string& vmName, std::string& error) {
    try {
        WmiHelper::Session session(L"root\\virtualization\\v2");
        std::wstring wVmName = Utils::StringToWString(vmName);
        
        // 查找虚拟机 (移除 WHERE Caption 以避免本地化问题)
        auto result = WmiHelper::Query(session,
            L"SELECT * FROM Msvm_ComputerSystem WHERE ElementName='" + wVmName + L"'");
        
        IWbemClassObject* pVM = nullptr;
        bool found = false;
        
        while (result->Next(&pVM)) {
            // 简单的验证：检查 Name 是否为 GUID 格式，或者 Caption/Description 是否符合特征
            std::wstring name = WmiHelper::GetProperty(pVM, L"Name");
            std::wstring caption = WmiHelper::GetProperty(pVM, L"Caption");
            
            bool isGuid = (name.length() == 36 || name.length() == 38);
            if (isGuid || caption.find(L"Virtual") != std::wstring::npos || caption.find(L"虚拟机") != std::wstring::npos) {
                found = true;
                break; // 找到了
            }
            pVM->Release();
        }
        
        if (!found || !pVM) {
            error = "虚拟机不存在";
            return false;
        }
        
        std::wstring vmPath = WmiHelper::GetObjectPath(pVM);
        pVM->Release();
        
        // 调用RequestStateChange方法 (2 = Enabled/Running)
        IWbemClassObject* pInParams = WmiHelper::CreateMethodParams(session,
            L"Msvm_ComputerSystem", L"RequestStateChange");
        
        if (pInParams) {
            WmiHelper::SetParam(pInParams, L"RequestedState", (uint64_t)2);
            
            IWbemClassObject* pOutParams = nullptr;
            HRESULT hr = WmiHelper::ExecuteMethod(session, vmPath, L"RequestStateChange", pInParams, &pOutParams);
            
            pInParams->Release();
            if (pOutParams) pOutParams->Release();
            
            if (FAILED(hr)) {
                error = "启动虚拟机失败";
                return false;
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        error = std::string("WMI错误: ") + e.what();
        throw;
    }
}

// PowerShell实现：获取所有虚拟机
std::vector<VMInfo> VMManager::GetAllVMsViaPowerShell() {
    std::vector<VMInfo> vms;
    
    // 使用PowerShell获取虚拟机列表及其GPU状态
    std::string command =
        "Get-VM | ForEach-Object { "
        "$g = $_ | Get-VMGpuPartitionAdapter -ErrorAction SilentlyContinue; "
        "$s = 'Off'; $v = 0; $ip = ''; "
        "if ($g) { $s = 'On'; $v = $g.MaxPartitionVRAM; $ip = $g.InstancePath } "
        "if ($_.Generation -ne 2) { $s = 'Not supported' } "
        "[PSCustomObject]@{ "
        "Name = $_.Name; "
        "State = $_.State.ToString(); "
        "GpuStatus = $s; "
        "VRAM = $v; "
        "InstancePath = $ip "
        "} } | ConvertTo-Json";

    std::string output, error;
    PowerShellExecutor::ExecuteWithCheck(command, output, error);
    
    if (!output.empty()) {
        vms = ParseVMJson(output);
    }
    
    // 如果有开启GPU-PV的虚拟机，尝试获取GPU名称
    bool hasGpuPvOn = false;
    for (const auto& vm : vms) {
        if (vm.strGPUStatus == "On") {
            hasGpuPvOn = true;
            break;
        }
    }
    
    std::vector<GPUInfo> gpus;
    if (hasGpuPvOn) {
        gpus = GPUManager::GetPartitionableGPUs();
    }
    
    // 构建显示文本
    for (auto& vm : vms) {
        // 如果开启了GPU-PV，查找GPU名称
        if (vm.strGPUStatus == "On" && !vm.strGPUInstancePath.empty()) {
            for (const auto& gpu : gpus) {
                // 简单的匹配：比较路径是否相互包含
                // 注意：路径大小写可能不一致，这里简单处理，实际可能需要更严谨的比较
                if (vm.strGPUInstancePath == gpu.strInstancePath ||
                    gpu.strInstancePath.find(vm.strGPUInstancePath) != std::string::npos ||
                    vm.strGPUInstancePath.find(gpu.strInstancePath) != std::string::npos) {
                    vm.strGPUName = gpu.strFriendlyName;
                    break;
                }
            }
        }
        
        // 格式化显示文本
        // VM_Name(State)  [GPU-PV: Supported]
        // VM_Name(State)  [GPU-PV: Not supported]
        // VM_Name(State)  [VRAM:1024MB (RTX 4050)]
        
        vm.strDisplayText = vm.strName + "(" + vm.strState + ")  [";
        
        if (vm.strGPUStatus == "On") {
            vm.strDisplayText += "VRAM:" + Utils::FormatVRAMSize(vm.ui64VramBytes);
            if (!vm.strGPUName.empty()) {
                vm.strDisplayText += " (" + vm.strGPUName + ")";
            }
        } else if (vm.strGPUStatus == "Not supported") {
            vm.strDisplayText += "GPU-PV: Not supported";
        } else {
            // Off
            vm.strDisplayText += "GPU-PV: Supported";
        }
        
        vm.strDisplayText += "]";
    }
    
    return vms;
}

// PowerShell实现：停止虚拟机
bool VMManager::StopVMViaPowerShell(const std::string& vmName, std::string& error) {
    std::string command = "Stop-VM -Name '" + vmName + "' -Force -WarningAction SilentlyContinue";
    std::string output;
    
    if (!PowerShellExecutor::ExecuteWithCheck(command, output, error)) {
        if (error.empty()) {
            error = "停止虚拟机失败";
        }
        return false;
    }
    
    return true;
}

// PowerShell实现：启动虚拟机
bool VMManager::StartVMViaPowerShell(const std::string& vmName, std::string& error) {
    std::string command = "Start-VM -Name '" + vmName + "'";
    std::string output;
    
    if (!PowerShellExecutor::ExecuteWithCheck(command, output, error)) {
        if (error.empty()) {
            error = "启动虚拟机失败";
        }
        return false;
    }
    
    return true;
}

// 获取虚拟机状态
std::string VMManager::GetVMState(const std::string& vmName) {
    std::string command = "(Get-VM -Name '" + vmName + "').State.ToString()";
    std::string output = PowerShellExecutor::Execute(command);
    return Utils::Trim(output);
}

// 检查虚拟机是否存在
bool VMManager::VMExists(const std::string& vmName) {
    std::string command = "Get-VM -Name '" + vmName + "' -ErrorAction SilentlyContinue";
    std::string output = PowerShellExecutor::Execute(command);
    return !output.empty();
}

// 解析虚拟机JSON输出
std::vector<VMInfo> VMManager::ParseVMJson(const std::string& json) {
    std::vector<VMInfo> vms;
    
    // 简单的JSON解析（处理单个对象和数组）
    std::string trimmedJson = Utils::Trim(json);
    if (trimmedJson.empty()) return vms;
    
    // 如果是数组，移除外层的[]
    if (trimmedJson[0] == '[') {
        // 找到最后一个 ]
        size_t lastBracket = trimmedJson.find_last_of(']');
        if (lastBracket != std::string::npos) {
            trimmedJson = trimmedJson.substr(1, lastBracket - 1);
        }
    }
    
    // 分割每个虚拟机对象（通过},{来分割，或者单纯找 {}）
    size_t pos = 0;
    while (pos < trimmedJson.length()) {
        // 查找下一个对象的开始
        size_t objStart = trimmedJson.find('{', pos);
        if (objStart == std::string::npos) break;
        
        // 查找对象的结束
        size_t objEnd = trimmedJson.find('}', objStart);
        if (objEnd == std::string::npos) break;
        
        std::string objStr = trimmedJson.substr(objStart, objEnd - objStart + 1);
        
        // 提取信息
        VMInfo vmInfo;
        vmInfo.strName = Utils::ExtractJsonValue(objStr, "Name");
        vmInfo.strState = Utils::ExtractJsonValue(objStr, "State");
        vmInfo.strGPUStatus = Utils::ExtractJsonValue(objStr, "GpuStatus");
        vmInfo.strGPUInstancePath = Utils::ExtractJsonValue(objStr, "InstancePath");
        
        std::string vramStr = Utils::ExtractJsonValue(objStr, "VRAM");
        if (!vramStr.empty()) {
            try {
                vmInfo.ui64VramBytes = std::stoull(vramStr);
            } catch (...) {
                vmInfo.ui64VramBytes = 0;
            }
        }
        
        if (!vmInfo.strName.empty()) {
            vms.push_back(vmInfo);
        }
        
        pos = objEnd + 1;
    }
    
    return vms;
}
