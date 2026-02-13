/********************************************************************************
* 文件名称：VMManager.h
* 文件功能：提供Hyper-V虚拟机管理功能
* 
* 类说明：
*    VMManager封装了Hyper-V虚拟机的查询、启动、停止等操作。采用"WMI优先+
*    PowerShell降级"的双重实现策略，确保最大兼容性和可靠性。
* 
* 主要功能：
*    1. 查询所有虚拟机及其状态（运行中、已关闭等）
*    2. 查询虚拟机GPU-PV配置状态
*    3. 启动和停止虚拟机
*    4. 检查虚拟机是否存在
* 
* 技术实现：
*    - WMI方式（优先）：通过Msvm_ComputerSystem类查询和控制虚拟机
*    - PowerShell方式（降级）：执行Get-VM、Stop-VM、Start-VM等cmdlet
*    - 自动fallback机制：WMI失败时自动降级到PowerShell
* 
* 数据结构：
*    VMInfo：虚拟机信息结构体，包含名称、状态、GPU配置等信息
* 
* 依赖项：
*    - WmiHelper（WMI操作）
*    - PowerShellExecutor（PowerShell命令执行）
* 
* 使用注意：
*    - 需要管理员权限
*    - 需要安装Hyper-V功能
*    - GPU-PV信息需要虚拟机关闭状态下才能完整获取
* 
* 作者：Smart-GPU-PV Team
* 日期：2026-01-26
* 版本：v2.0
*********************************************************************************/

#pragma once
#include <string>
#include <vector>

/********************************************************************************
* 结构体名称：虚拟机信息
* 结构体功能：存储单个虚拟机的详细信息
* 
* 成员说明：
*    strName：虚拟机名称
*    strState：运行状态（Running=运行中，Off=已关闭，Saved=已保存等）
*    strGPUStatus：GPU-PV配置状态（On=已启用，Off=未启用，Not supported=不支持）
*    ui64VramBytes：已分配的显存大小（字节），未配置时为0
*    strGPUInstancePath：关联的GPU实例路径（PCI路径）
*    strGPUName：关联的GPU名称
*    strDisplayText：用于UI显示的格式化文本
*********************************************************************************/
struct VMInfo {
    std::string strName;              // 虚拟机名称
    std::string strState;             // 运行状态
    std::string strGPUStatus;         // GPU-PV状态
    uint64_t ui64VramBytes = 0;       // 显存大小（字节）
    std::string strGPUInstancePath;   // GPU实例路径
    std::string strGPUName;           // GPU名称
    std::string strDisplayText;       // 显示文本
};

/********************************************************************************
* 类名称：虚拟机管理器
* 类功能：提供Hyper-V虚拟机的查询和控制功能
*********************************************************************************/
class VMManager {
public:
    //==============================================================================
    // 公共接口
    //==============================================================================
    
    /********************************************************************************
    * 函数名称：获取所有虚拟机
    * 函数功能：查询系统中所有Hyper-V虚拟机的信息，包括GPU-PV配置状态
    * 函数参数：
    *    无
    * 返回类型：std::vector<VMInfo>
    *    虚拟机信息数组，若查询失败返回空数组
    * 调用示例：
    *    std::vector<VMInfo> vecVMs = VMManager::GetAllVMs();
    *    for (const auto& stcVM : vecVMs) {
    *        std::cout << stcVM.strName << ": " << stcVM.strState << std::endl;
    *    }
    * 注意事项：
    *    - 优先使用WMI查询，失败时自动降级到PowerShell
    *    - GPU-PV信息仅在虚拟机关闭时完整
    *********************************************************************************/
    static std::vector<VMInfo> GetAllVMs();
    
    /********************************************************************************
    * 函数名称：停止虚拟机
    * 函数功能：强制停止指定的虚拟机（相当于拔电源）
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    *    [OUT] std::string& strError：返回的错误信息
    * 返回类型：bool
    *    执行成功：true
    *    执行失败：false
    * 调用示例：
    *    std::string strError;
    *    if (!VMManager::StopVM("Windows 11", strError)) {
    *        std::cerr << "停止失败: " << strError << std::endl;
    *    }
    * 注意事项：
    *    - 这是强制停止，不会正常关机
    *    - 可能导致虚拟机内数据丢失
    *    - 优先使用WMI，失败时降级到PowerShell
    *********************************************************************************/
    static bool StopVM(const std::string& strVMName, std::string& strError);
    
    /********************************************************************************
    * 函数名称：启动虚拟机
    * 函数功能：启动指定的虚拟机
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    *    [OUT] std::string& strError：返回的错误信息
    * 返回类型：bool
    *    执行成功：true
    *    执行失败：false
    * 调用示例：
    *    std::string strError;
    *    if (!VMManager::StartVM("Windows 11", strError)) {
    *        std::cerr << "启动失败: " << strError << std::endl;
    *    }
    * 注意事项：
    *    - 虚拟机必须处于关闭状态
    *    - 优先使用WMI，失败时降级到PowerShell
    *********************************************************************************/
    static bool StartVM(const std::string& strVMName, std::string& strError);
    
    /********************************************************************************
    * 函数名称：获取虚拟机状态
    * 函数功能：查询指定虚拟机的当前运行状态
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    * 返回类型：std::string
    *    状态字符串（"Running"、"Off"、"Saved"等），若查询失败返回"Unknown"
    * 调用示例：
    *    std::string strState = VMManager::GetVMState("Windows 11");
    *    if (strState == "Running") {
    *        // 虚拟机正在运行
    *    }
    *********************************************************************************/
    static std::string GetVMState(const std::string& strVMName);
    
    /********************************************************************************
    * 函数名称：检查虚拟机是否存在
    * 函数功能：判断指定名称的虚拟机是否存在
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    * 返回类型：bool
    *    存在返回true，不存在返回false
    * 调用示例：
    *    if (VMManager::VMExists("Windows 11")) {
    *        // 虚拟机存在
    *    }
    *********************************************************************************/
    static bool VMExists(const std::string& strVMName);
    
private:
    //==============================================================================
    // 内部实现（WMI方式）
    //==============================================================================
    
    /********************************************************************************
    * 函数名称：通过WMI获取所有虚拟机（内部方法）
    * 函数功能：使用WMI查询所有虚拟机信息
    * 返回类型：std::vector<VMInfo>
    *    虚拟机信息数组
    *********************************************************************************/
    static std::vector<VMInfo> GetAllVMsViaWMI();
    
    /********************************************************************************
    * 函数名称：通过WMI停止虚拟机（内部方法）
    * 函数功能：使用WMI方法停止虚拟机
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    *    [OUT] std::string& strError：错误信息
    * 返回类型：bool
    *********************************************************************************/
    static bool StopVMViaWMI(const std::string& strVMName, std::string& strError);
    
    /********************************************************************************
    * 函数名称：通过WMI启动虚拟机（内部方法）
    * 函数功能：使用WMI方法启动虚拟机
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    *    [OUT] std::string& strError：错误信息
    * 返回类型：bool
    *********************************************************************************/
    static bool StartVMViaWMI(const std::string& strVMName, std::string& strError);
    
    //==============================================================================
    // 内部实现（PowerShell方式）
    //==============================================================================
    
    /********************************************************************************
    * 函数名称：通过PowerShell获取所有虚拟机（内部方法）
    * 函数功能：使用PowerShell cmdlet查询所有虚拟机信息
    * 返回类型：std::vector<VMInfo>
    *    虚拟机信息数组
    *********************************************************************************/
    static std::vector<VMInfo> GetAllVMsViaPowerShell();
    
    /********************************************************************************
    * 函数名称：通过PowerShell停止虚拟机（内部方法）
    * 函数功能：使用Stop-VM cmdlet停止虚拟机
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    *    [OUT] std::string& strError：错误信息
    * 返回类型：bool
    *********************************************************************************/
    static bool StopVMViaPowerShell(const std::string& strVMName, std::string& strError);
    
    /********************************************************************************
    * 函数名称：通过PowerShell启动虚拟机（内部方法）
    * 函数功能：使用Start-VM cmdlet启动虚拟机
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    *    [OUT] std::string& strError：错误信息
    * 返回类型：bool
    *********************************************************************************/
    static bool StartVMViaPowerShell(const std::string& strVMName, std::string& strError);
    
    //==============================================================================
    // 辅助方法
    //==============================================================================
    
    /********************************************************************************
    * 函数名称：解析虚拟机JSON（内部方法）
    * 函数功能：解析PowerShell ConvertTo-Json输出的虚拟机信息
    * 函数参数：
    *    [IN]  const std::string& strJson：JSON格式字符串
    * 返回类型：std::vector<VMInfo>
    *    虚拟机信息数组
    *********************************************************************************/
    static std::vector<VMInfo> ParseVMJson(const std::string& strJson);
};
