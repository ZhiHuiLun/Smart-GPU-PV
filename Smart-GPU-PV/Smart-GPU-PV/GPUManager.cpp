#include "GPUManager.h"
#include "PowerShellExecutor.h"
#include "WmiHelper.h"
#include "HyperVException.h"
#include "Utils.h"
#include <algorithm>
#include <dxgi.h>
#include <vector>
#include <map>
#include <wbemidl.h>
#include <comdef.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "wbemuuid.lib")

// 获取所有支持分区的GPU
std::vector<GPUInfo> GPUManager::GetPartitionableGPUs() {
    try {
        return GetPartitionableGPUsViaWMI();
    } catch (...) {
        // WMI失败，降级到PowerShell
        return GetPartitionableGPUsViaPowerShell();
    }
}

// WMI实现：获取所有可分区GPU
std::vector<GPUInfo> GPUManager::GetPartitionableGPUsViaWMI() {
    std::vector<GPUInfo> gpus;
    
    try {
        WmiHelper::Session session(L"root\\virtualization\\v2");
        
        // 查询所有可分区GPU
        auto result = WmiHelper::Query(session,
            L"SELECT * FROM Msvm_PartitionableGpu");
        
        // 获取DXGI GPU详细信息（显存等）
        std::vector<GPUInfo> dxgiGpus = GetGPUDetails();
        
        IWbemClassObject* pGpu = nullptr;
        while (result->Next(&pGpu)) {
            GPUInfo gpuInfo;
            gpuInfo.strFriendlyName = Utils::WStringToString(WmiHelper::GetProperty(pGpu, L"Name"));
            gpuInfo.strInstancePath = Utils::WStringToString(WmiHelper::GetProperty(pGpu, L"Name"));
            
            // 从instancePath提取硬件ID来匹配DXGI信息
            std::string hwId = ExtractHardwareID(gpuInfo.strInstancePath);
            
            // 尝试从DXGI获取准确的显存信息
            for (const auto& dxgiGpu : dxgiGpus) {
                std::string dxgiHwId = ExtractHardwareID(dxgiGpu.strPnpDeviceID);
                if (!dxgiHwId.empty() && hwId.find(dxgiHwId) != std::string::npos) {
                    gpuInfo.ui64VramBytes = dxgiGpu.ui64VramBytes;
                    gpuInfo.strPnpDeviceID = dxgiGpu.strPnpDeviceID;
                    gpuInfo.strDriverPath = dxgiGpu.strDriverPath;
                    gpuInfo.strFriendlyName = dxgiGpu.strFriendlyName;  // 使用更友好的名称
                    break;
                }
            }
            
            // 如果没有找到DXGI匹配，设置默认值
            if (gpuInfo.ui64VramBytes == 0) {
                gpuInfo.ui64VramBytes = 1024 * 1024 * 1024; // 默认1GB
            }
            
            // 构建显示文本
            std::string vramSize = Utils::FormatVRAMSize(gpuInfo.ui64VramBytes);
            std::string strShortGPUName = gpuInfo.strFriendlyName;
            int nMaxGPUName = 30;
            if (strShortGPUName.length() > nMaxGPUName)
                strShortGPUName = strShortGPUName.substr(0, nMaxGPUName) + "...";
            else
                strShortGPUName.append(nMaxGPUName + 3 - strShortGPUName.length(), ' ');
            
            std::string shortPath = gpuInfo.strInstancePath;
            if (shortPath.length() > 20) {
                shortPath = shortPath.substr(0, 20) + "...";
            }
            
            gpuInfo.strDisplayText = strShortGPUName + "\t [ VRAM:" + vramSize + "  Path:" + shortPath + " ] ";
            
            gpus.push_back(gpuInfo);
            pGpu->Release();
        }
    } catch (const std::exception&) {
        throw HyperVException("WMI query failed");
    }
    
    return gpus;
}

// PowerShell实现：获取所有可分区GPU
std::vector<GPUInfo> GPUManager::GetPartitionableGPUsViaPowerShell() {
    std::vector<GPUInfo> gpus;
    
    // 1. 获取可分区GPU的实例路径
    std::vector<std::string> paths = GetPartitionableGPUPaths();
    if (paths.empty()) {
        return gpus;
    }
    
    // 2. 获取GPU详细信息（使用DXGI获取准确显存）
    std::vector<GPUInfo> details = GetGPUDetails();
    if (details.empty()) {
        return gpus;
    }
    
    // 3. 匹配路径和详细信息
    gpus = MatchGPUInfo(paths, details);
    
    return gpus;
}

// 根据友好名称查找GPU
GPUInfo GPUManager::FindGPUByName(const std::vector<GPUInfo>& gpus, const std::string& name) {
    for (const auto& gpu : gpus) {
        if (gpu.strFriendlyName == name) {
            return gpu;
        }
    }
    return GPUInfo();
}

// 检查系统是否支持GPU-PV
bool GPUManager::IsGPUPVSupported() {
    std::string command = "Get-VMHostPartitionableGpu -ErrorAction SilentlyContinue";
    std::string output = PowerShellExecutor::Execute(command);
    return !output.empty();
}

// 获取可分区GPU的实例路径
std::vector<std::string> GPUManager::GetPartitionableGPUPaths() {
    std::vector<std::string> paths;
    
    // 使用Get-VMHostPartitionableGpu获取可分区GPU
    std::string command = "Get-VMHostPartitionableGpu | Select-Object -ExpandProperty Name";
    std::string output = PowerShellExecutor::Execute(command);
    
    if (!output.empty()) {
        // 按行分割
        std::vector<std::string> lines = Utils::Split(output, '\n');
        for (const auto& line : lines) {
            std::string trimmed = Utils::Trim(line);
            // 放宽检查：只要非空且包含PCI标识即可，不再强制要求以\\?\开头
            if (!trimmed.empty() && trimmed.find("PCI#") != std::string::npos) {
                paths.push_back(trimmed);
            }
        }
    }
    
    return paths;
}

// 使用DXGI获取显卡信息
std::vector<GPUInfo> GPUManager::GetGPUDetails() {
    std::vector<GPUInfo> details;
    
    // 1. 先通过WMI获取所有显卡的驱动信息和PNP ID
    std::map<std::string, std::string> wmiDrivers = GetWmiGPUDrivers();

    IDXGIFactory* pFactory = nullptr;
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory))) {
        return details;
    }

    IDXGIAdapter* pAdapter = nullptr;
    for (UINT i = 0; pFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC desc;
        if (SUCCEEDED(pAdapter->GetDesc(&desc))) {
            GPUInfo info;
            info.strFriendlyName = Utils::WStringToString(desc.Description);
            info.ui64VramBytes = desc.DedicatedVideoMemory; // 专用显存
            
            // 只有当有显存时才添加（忽略纯软件适配器）
            if (info.ui64VramBytes > 0) {
                // 尝试匹配WMI信息以获取PNPDeviceID和DriverPath
                // 匹配策略：通过 VendorID 和 DeviceID
                char hwIdPattern[64];
                sprintf_s(hwIdPattern, "VEN_%04X&DEV_%04X", desc.VendorId, desc.DeviceId);
                std::string pattern = hwIdPattern;
                
                // 在WMI结果中查找包含此Vendor/Device ID的条目
                for (const auto& pair : wmiDrivers) {
                    const std::string& pnpId = pair.first;
                    const std::string& driverPath = pair.second;
                    
                    // 查找子串 (忽略大小写通常更好，但ID通常是大写)
                    if (pnpId.find(pattern) != std::string::npos) {
                        info.strPnpDeviceID = pnpId;
                        info.strDriverPath = driverPath;
                        break;
                    }
                }
                
                details.push_back(info);
            }
        }
        pAdapter->Release();
    }
    pFactory->Release();
    
    return details;
}

// 匹配GPU路径和详细信息
std::vector<GPUInfo> GPUManager::MatchGPUInfo(
    const std::vector<std::string>& paths,
    const std::vector<GPUInfo>& details) {
    
    std::vector<GPUInfo> result;
    
    for (const auto& path : paths) {
        // 从路径中提取硬件ID (例如: PCI#VEN_10DE&DEV_28A1...)
        size_t pciPos = path.find("PCI#");
        if (pciPos == std::string::npos) continue;
        
        // 提取VEN和DEV信息
        std::string pathHwId = path.substr(pciPos);
        
        // 在详细信息中查找匹配的GPU
        for (const auto& detail : details) {
            // 如果没有PNPDeviceID，跳过
            if (detail.strPnpDeviceID.empty()) continue;

            std::string detailHwId = ExtractHardwareID(detail.strPnpDeviceID);
            
            // 比较硬件ID（VEN和DEV部分）
            if (!detailHwId.empty() && pathHwId.find(detailHwId) != std::string::npos) {
                GPUInfo matched = detail;
                matched.strInstancePath = path;
                
                // 构建显示文本
                std::string vramSize = Utils::FormatVRAMSize(matched.ui64VramBytes);
                
                // GPU名称对齐
                std::string strShortGPUName = matched.strFriendlyName;
                int nMaxGPUName = 30;
                if (strShortGPUName.length() > nMaxGPUName)
                    strShortGPUName = strShortGPUName.substr(0, nMaxGPUName) + "...";
                else // 补充空格（max_width - 原长度 个空格）
                    strShortGPUName.append(nMaxGPUName+3 - strShortGPUName.length(), ' ');

                // 截断路径显示
                std::string shortPath = matched.strInstancePath;
                if (shortPath.length() > 20) {
                    shortPath = shortPath.substr(0, 20) + "...";
                }
                
                matched.strDisplayText = strShortGPUName + "\t [ VRAM:" + vramSize + "  Path:" + shortPath + " ] ";
                
                result.push_back(matched);
                break; 
            }
        }
    }
    
    return result;
}

// 从PNP设备ID提取硬件ID
std::string GPUManager::ExtractHardwareID(const std::string& pnpDeviceID) {
    // PNP设备ID格式: 
    // 标准: PCI\VEN_10DE&DEV_28A1&SUBSYS_...
    // WMI Name: \\?\PCI#VEN_10DE&DEV_28A1&...
    
    size_t pciPos = pnpDeviceID.find("PCI\\");
    if (pciPos == std::string::npos) {
        pciPos = pnpDeviceID.find("PCI#"); // 支持 WMI Name 格式
    }
    
    if (pciPos == std::string::npos) return "";
    
    std::string hwId = pnpDeviceID.substr(pciPos + 4); // 跳过"PCI\"或"PCI#"
    
    // 提取VEN和DEV部分
    size_t venPos = hwId.find("VEN_");
    size_t devPos = hwId.find("&DEV_");
    
    if (venPos != std::string::npos && devPos != std::string::npos) {
        // 提取到第二个&之前 (确保 VEN 和 DEV 都在)
        size_t endPos = hwId.find("&", devPos + 1);
        if (endPos != std::string::npos) {
            return hwId.substr(venPos, endPos - venPos);
        } else {
            // 如果没有第二个 &，则取到最后
            return hwId.substr(venPos);
        }
    }
    
    return "";
}

// 获取WMI中的显卡驱动信息
std::map<std::string, std::string> GPUManager::GetWmiGPUDrivers() {
    std::map<std::string, std::string> result;
    
    // COM 初始化已在主线程完成，但在某些情况下可能需要在当前线程再次初始化
    // 简单的 CoInitialize 检查
    HRESULT hres; 
    
    IWbemLocator *pLoc = nullptr;
    hres = CoCreateInstance(
        CLSID_WbemLocator,             
        0, 
        CLSCTX_INPROC_SERVER, 
        IID_IWbemLocator, (LPVOID *) &pLoc);
 
    if (FAILED(hres)) return result;

    IWbemServices *pSvc = nullptr;
    hres = pLoc->ConnectServer(
         _bstr_t(L"ROOT\\CIMV2"), 
         NULL, NULL, 0, NULL, 0, 0, &pSvc);
    
    if (FAILED(hres)) {
        pLoc->Release();
        return result;
    }

    hres = CoSetProxyBlanket(
       pSvc,                        
       RPC_C_AUTHN_WINNT,           
       RPC_C_AUTHZ_NONE,            
       NULL,                        
       RPC_C_AUTHN_LEVEL_CALL,      
       RPC_C_IMP_LEVEL_IMPERSONATE, 
       NULL,                        
       EOAC_NONE);

    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        return result;
    }

    IEnumWbemClassObject* pEnumerator = nullptr;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"), 
        bstr_t("SELECT PNPDeviceID, InstalledDisplayDrivers FROM Win32_VideoController"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
        NULL,
        &pEnumerator);
    
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        return result;
    }

    IWbemClassObject *pclsObj = nullptr;
    ULONG uReturn = 0;
   
    while (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn) {
            break;
        }

        VARIANT vtProp;
        
        // 获取 PNPDeviceID
        std::string pnpID;
        if (SUCCEEDED(pclsObj->Get(L"PNPDeviceID", 0, &vtProp, 0, 0)) && vtProp.vt == VT_BSTR) {
            pnpID = Utils::WStringToString(vtProp.bstrVal);
        }
        VariantClear(&vtProp); 

        // 获取 InstalledDisplayDrivers
        std::string driverPath;
        if (SUCCEEDED(pclsObj->Get(L"InstalledDisplayDrivers", 0, &vtProp, 0, 0)) && vtProp.vt == VT_BSTR) {
             std::string rawList = Utils::WStringToString(vtProp.bstrVal);
             
             // InstalledDisplayDrivers 可能是逗号分隔的文件列表，我们只需要第一个文件的路径
             // 例如: "C:\Path\File1.dll,C:\Path\File2.dll"
             size_t commaPos = rawList.find(',');
             std::string firstFile = (commaPos != std::string::npos) ? rawList.substr(0, commaPos) : rawList;
             
             // 只需要目录路径
             size_t lastSlash = firstFile.find_last_of("\\");
             if (lastSlash != std::string::npos) {
                 driverPath = firstFile.substr(0, lastSlash);
             } else {
                 driverPath = firstFile;
             }
        }
        VariantClear(&vtProp);

        if (!pnpID.empty()) {
            result[pnpID] = driverPath;
        }

        pclsObj->Release();
    }

    pEnumerator->Release();
    pSvc->Release();
    pLoc->Release();
    
    return result;
}
