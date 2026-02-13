/********************************************************************************
* 文件名称：GPUManager.h
* 文件功能：提供GPU设备管理和查询功能
* 
* 类说明：
*    GPUManager封装了支持GPU分区（GPU-PV）的显卡查询功能。采用"WMI优先+
*    PowerShell降级"策略，能够获取GPU的名称、显存大小、PCI路径、驱动
*    位置等信息。
* 
* 主要功能：
*    1. 查询所有支持GPU-PV的显卡
*    2. 获取GPU详细信息（名称、显存、实例路径、驱动路径）
*    3. 根据名称查找特定GPU
*    4. 检查系统是否支持GPU-PV功能
* 
* 技术实现：
*    - WMI方式（优先）：查询Msvm_PartitionableGpu和Win32_VideoController
*    - PowerShell方式（降级）：执行Get-VMHostPartitionableGpu cmdlet
*    - DXGI方式：使用DirectX Graphics Infrastructure获取准确显存
*    - 驱动路径查询：通过INF文件定位驱动文件夹
* 
* 数据结构：
*    GPUInfo：GPU信息结构体，包含名称、路径、显存、驱动位置等
* 
* 依赖项：
*    - WmiHelper（WMI操作）
*    - PowerShellExecutor（PowerShell命令执行）
*    - DXGI（显存查询）
* 
* 使用注意：
*    - 需要管理员权限
*    - 需要Hyper-V功能支持
*    - 仅返回支持GPU-PV的显卡（通常为独立显卡）
* 
* 作者：Smart-GPU-PV Team
* 日期：2026-01-26
* 版本：v2.0
*********************************************************************************/

#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>

/********************************************************************************
* 结构体名称：GPU信息
* 结构体功能：存储单个GPU设备的详细信息
* 
* 成员说明：
*    strFriendlyName：GPU友好名称（如"NVIDIA GeForce RTX 4050"）
*    strInstancePath：GPU实例路径（PCI路径，用于GPU-PV配置）
*    ui64VramBytes：显存大小（字节）
*    strPnpDeviceID：即插即用设备ID
*    strDriverPath：驱动文件所在路径
*    strDisplayText：UI显示文本（包含名称、显存、路径的格式化字符串）
*********************************************************************************/
struct GPUInfo {
    std::string strFriendlyName;    // GPU友好名称
    std::string strInstancePath;    // GPU实例路径（PCI路径）
    uint64_t ui64VramBytes = 0;     // 显存大小（字节）
    std::string strPnpDeviceID;     // PNP设备ID
    std::string strDriverPath;      // 驱动文件路径
    std::string strDisplayText;     // 显示文本
};

/********************************************************************************
* 类名称：GPU管理器
* 类功能：提供GPU设备的查询和信息获取功能
*********************************************************************************/
class GPUManager {
public:
    //==============================================================================
    // 公共接口
    //==============================================================================
    
    /********************************************************************************
    * 函数名称：获取可分区GPU列表
    * 函数功能：查询系统中所有支持GPU分区（GPU-PV）的显卡信息
    * 函数参数：
    *    无
    * 返回类型：std::vector<GPUInfo>
    *    GPU信息数组，若无可用GPU返回空数组
    * 调用示例：
    *    std::vector<GPUInfo> vecGPUs = GPUManager::GetPartitionableGPUs();
    *    for (const auto& stcGPU : vecGPUs) {
    *        std::cout << stcGPU.strFriendlyName << ": " 
    *                  << Utils::FormatVRAMSize(stcGPU.ui64VramBytes) << std::endl;
    *    }
    * 注意事项：
    *    - 优先使用WMI查询，失败时自动降级到PowerShell
    *    - 仅返回支持分区的GPU（通常为独立显卡）
    *    - 集成显卡一般不支持GPU-PV
    *********************************************************************************/
    static std::vector<GPUInfo> GetPartitionableGPUs();
    
    /********************************************************************************
    * 函数名称：根据名称查找GPU
    * 函数功能：在GPU列表中查找指定名称的GPU
    * 函数参数：
    *    [IN]  const std::vector<GPUInfo>& vecGPUs：GPU信息数组
    *    [IN]  const std::string& strName：要查找的GPU名称（模糊匹配）
    * 返回类型：GPUInfo
    *    找到的GPU信息，若未找到返回空的GPUInfo结构
    * 调用示例：
    *    GPUInfo stcGPU = GPUManager::FindGPUByName(vecGPUs, "RTX");
    *    if (!stcGPU.strFriendlyName.empty()) {
    *        // 找到了包含"RTX"的GPU
    *    }
    * 注意事项：
    *    - 使用模糊匹配，不需要完整名称
    *    - 区分大小写
    *********************************************************************************/
    static GPUInfo FindGPUByName(const std::vector<GPUInfo>& vecGPUs, const std::string& strName);
    
    /********************************************************************************
    * 函数名称：检查GPU-PV支持
    * 函数功能：检查系统是否支持GPU分区（GPU-PV）功能
    * 函数参数：
    *    无
    * 返回类型：bool
    *    支持返回true，不支持返回false
    * 调用示例：
    *    if (!GPUManager::IsGPUPVSupported()) {
    *        std::cerr << "系统不支持GPU-PV功能" << std::endl;
    *    }
    * 注意事项：
    *    - 需要Windows 10 20H1+或Windows 11
    *    - 需要启用Hyper-V功能
    *    - 需要支持GPU-PV的显卡
    *********************************************************************************/
    static bool IsGPUPVSupported();
    
private:
    //==============================================================================
    // 内部实现（WMI方式）
    //==============================================================================
    
    /********************************************************************************
    * 函数名称：通过WMI获取可分区GPU（内部方法）
    * 函数功能：使用WMI查询支持分区的GPU列表
    * 返回类型：std::vector<GPUInfo>
    *    GPU信息数组
    *********************************************************************************/
    static std::vector<GPUInfo> GetPartitionableGPUsViaWMI();
    
    /********************************************************************************
    * 函数名称：获取WMI中的GPU驱动信息（内部方法）
    * 函数功能：查询GPU驱动文件路径映射表
    * 返回类型：std::map<std::string, std::string>
    *    PNP设备ID到驱动路径的映射
    *********************************************************************************/
    static std::map<std::string, std::string> GetWmiGPUDrivers();
    
    //==============================================================================
    // 内部实现（PowerShell方式）
    //==============================================================================
    
    /********************************************************************************
    * 函数名称：通过PowerShell获取可分区GPU（内部方法）
    * 函数功能：使用PowerShell cmdlet查询支持分区的GPU
    * 返回类型：std::vector<GPUInfo>
    *    GPU信息数组
    *********************************************************************************/
    static std::vector<GPUInfo> GetPartitionableGPUsViaPowerShell();
    
    /********************************************************************************
    * 函数名称：获取可分区GPU路径（内部方法）
    * 函数功能：使用PowerShell获取GPU实例路径列表
    * 返回类型：std::vector<std::string>
    *    GPU实例路径数组
    *********************************************************************************/
    static std::vector<std::string> GetPartitionableGPUPaths();
    
    /********************************************************************************
    * 函数名称：获取GPU详细信息（内部方法）
    * 函数功能：查询GPU的名称、显存等详细信息
    * 返回类型：std::vector<GPUInfo>
    *    GPU详细信息数组
    *********************************************************************************/
    static std::vector<GPUInfo> GetGPUDetails();
    
    //==============================================================================
    // 辅助方法
    //==============================================================================
    
    /********************************************************************************
    * 函数名称：匹配GPU信息（内部方法）
    * 函数功能：将GPU路径列表与详细信息列表进行匹配
    * 函数参数：
    *    [IN]  const std::vector<std::string>& vecPaths：GPU路径数组
    *    [IN]  const std::vector<GPUInfo>& vecDetails：GPU详细信息数组
    * 返回类型：std::vector<GPUInfo>
    *    匹配后的完整GPU信息数组
    *********************************************************************************/
    static std::vector<GPUInfo> MatchGPUInfo(
        const std::vector<std::string>& vecPaths,
        const std::vector<GPUInfo>& vecDetails
    );
    
    /********************************************************************************
    * 函数名称：提取硬件ID（内部方法）
    * 函数功能：从PNP设备ID中提取硬件ID部分
    * 函数参数：
    *    [IN]  const std::string& strPnpDeviceID：PNP设备ID
    * 返回类型：std::string
    *    硬件ID字符串
    *********************************************************************************/
    static std::string ExtractHardwareID(const std::string& strPnpDeviceID);
};
