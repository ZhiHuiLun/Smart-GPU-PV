/********************************************************************************
* 文件名称：GPUPVConfigurator.h
* 文件功能：提供GPU-PV（GPU分区虚拟化）配置功能
* 
* 类说明：
*    GPUPVConfigurator封装了为Hyper-V虚拟机配置GPU分区的完整流程，包括
*    添加GPU分区适配器、配置资源分配、启用缓存控制、复制驱动文件等。
*    采用事务性操作模式，失败时自动回滚到初始状态。
* 
* 主要功能：
*    1. 配置GPU-PV完整流程（一键式）
*    2. 添加GPU分区适配器
*    3. 配置VRAM、Encode、Decode、Compute资源
*    4. 启用GuestControlledCacheTypes
*    5. 复制GPU驱动文件到虚拟机
*    6. 自动备份和恢复配置
* 
* 技术实现：
*    - PowerShell cmdlet：Add-VMGpuPartitionAdapter、Set-VMGpuPartitionAdapter等
*    - VHD挂载：使用Virtual Disk API挂载虚拟机磁盘
*    - 文件操作：递归复制驱动文件夹
*    - 事务模式：操作失败时自动恢复原始配置
* 
* 配置流程：
*    1. 备份当前GPU-PV配置（如果有）
*    2. 停止虚拟机（如果运行中）
*    3. 添加GPU分区适配器
*    4. 配置GPU资源分配
*    5. 启用缓存控制
*    6. 挂载虚拟机磁盘
*    7. 复制GPU驱动文件
*    8. 卸载虚拟机磁盘
*    9. 失败时恢复原始配置
* 
* 依赖项：
*    - PowerShellExecutor（执行Hyper-V cmdlet）
*    - VhdHelper（VHD挂载操作）
*    - VMManager（虚拟机控制）
* 
* 使用注意：
*    - 需要管理员权限
*    - 虚拟机必须处于关闭状态
*    - 确保GPU驱动路径正确
*    - 配置过程中不要中断
* 
* 作者：Smart-GPU-PV Team
* 日期：2026-01-26
* 版本：v2.0
*********************************************************************************/

#pragma once
#include <string>
#include <functional>

/********************************************************************************
* 类型定义：进度回调函数
* 功能说明：用于向调用者报告配置进度的回调函数类型
* 
* 参数说明：
*    const std::string& strMessage：进度消息文本
* 
* 使用示例：
*    ProgressCallback callback = [](const std::string& strMsg) {
*        std::cout << strMsg << std::endl;
*    };
*********************************************************************************/
using ProgressCallback = std::function<void(const std::string& strMessage)>;

/********************************************************************************
* 结构体名称：GPU-PV配置备份
* 结构体功能：存储GPU-PV配置的备份信息，用于失败时回滚
* 
* 成员说明：
*    bHasAdapter：是否已有GPU适配器
*    strInstancePath：GPU实例路径
*    ui64VramBytes：显存大小（字节）
*    bGuestControlledCacheTypes：是否启用缓存控制
*********************************************************************************/
struct GPUPVBackup {
    bool        bHasAdapter                = false; // 是否已有适配器
    std::string strInstancePath;                    // GPU实例路径
    uint64_t    ui64VramBytes              = 0;     // 显存大小
    bool        bGuestControlledCacheTypes = false; // 缓存控制标志
};

/********************************************************************************
* 类名称：GPU-PV配置器
* 类功能：提供GPU分区虚拟化的完整配置流程
*********************************************************************************/
class GPUPVConfigurator {
public:
    //==============================================================================
    // 公共接口
    //==============================================================================
    
    /********************************************************************************
    * 函数名称：配置GPU-PV（完整流程）
    * 函数功能：为虚拟机配置GPU分区虚拟化，包括所有必要步骤
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    *    [IN]  const std::string& strGPUName：GPU名称（用于日志显示）
    *    [IN]  const std::string& strGPUInstancePath：GPU实例路径（PCI路径）
    *    [IN]  const std::string& strDriverPath：GPU驱动文件路径
    *    [IN]  int nVramMB：要分配的显存大小（MB）
    *    [IN]  ProgressCallback callback：进度回调函数
    * 返回类型：bool
    *    配置成功：true
    *    配置失败：false（自动回滚）
    * 调用示例：
    *    bool bSuccess = GPUPVConfigurator::ConfigureGPUPV(
    *        "Windows 11",
    *        "NVIDIA GeForce RTX 4050",
    *        "\\\\?\\PCI#VEN_10DE&DEV_...",
    *        "C:\\Windows\\System32\\DriverStore\\FileRepository\\...",
    *        4096,
    *        [](const std::string& strMsg) { std::cout << strMsg << std::endl; }
    *    );
    * 注意事项：
    *    - 虚拟机必须处于关闭状态
    *    - 配置过程可能需要几分钟
    *    - 失败时会自动恢复原始配置
    *********************************************************************************/
    static bool ConfigureGPUPV(
        const std::string& strVMName,
        const std::string& strGPUName,
        const std::string& strGPUInstancePath,
        const std::string& strDriverPath,
        int nVramMB,
        ProgressCallback callback
    );
    
private:
    //==============================================================================
    // 内部实现（配置步骤）
    //==============================================================================
    
    /********************************************************************************
    * 函数名称：备份当前状态（内部方法）
    * 函数功能：保存虚拟机当前的GPU-PV配置，用于失败回滚
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    * 返回类型：GPUPVBackup
    *    备份信息结构
    *********************************************************************************/
    static GPUPVBackup BackupState(const std::string& strVMName);
    
    /********************************************************************************
    * 函数名称：恢复状态（内部方法）
    * 函数功能：将虚拟机GPU-PV配置恢复到备份状态
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    *    [IN]  const GPUPVBackup& stcBackup：备份信息
    *    [IN]  ProgressCallback callback：进度回调函数
    * 返回类型：void
    *********************************************************************************/
    static void RestoreState(const std::string& strVMName, const GPUPVBackup& stcBackup, ProgressCallback callback);

    /********************************************************************************
    * 函数名称：添加GPU分区适配器（内部方法）
    * 函数功能：向虚拟机添加GPU分区适配器
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    *    [IN]  const std::string& strGPUInstancePath：GPU实例路径
    *    [OUT] std::string& strError：错误信息
    * 返回类型：bool
    *    成功返回true，失败返回false
    *********************************************************************************/
    static bool AddGPUPartitionAdapter(
        const std::string& strVMName,
        const std::string& strGPUInstancePath,
        std::string& strError
    );
    
    /********************************************************************************
    * 函数名称：配置GPU资源分配（内部方法）
    * 函数功能：设置VRAM、Encode、Decode、Compute资源的最小值、最大值、最优值
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    *    [IN]  uint64_t ui64VramBytes：显存大小（字节）
    *    [OUT] std::string& strError：错误信息
    * 返回类型：bool
    *    成功返回true，失败返回false
    * 注意事项：
    *    - Encode、Decode、Compute资源与VRAM设置相同
    *********************************************************************************/
    static bool ConfigureGPUResources(
        const std::string& strVMName,
        uint64_t ui64VramBytes,
        std::string& strError
    );
    
    /********************************************************************************
    * 函数名称：启用缓存控制（内部方法）
    * 函数功能：启用GuestControlledCacheTypes，允许虚拟机控制GPU缓存
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    *    [OUT] std::string& strError：错误信息
    * 返回类型：bool
    *    成功返回true，失败返回false
    *********************************************************************************/
    static bool EnableGuestControlledCacheTypes(
        const std::string& strVMName,
        std::string& strError
    );
    
    /********************************************************************************
    * 函数名称：配置MMIO空间（内部方法）
    * 函数功能：设置LowMemoryMappedIoSpace和HighMemoryMappedIoSpace，GPU-PV关键配置
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    *    [OUT] std::string& strError：错误信息
    * 返回类型：bool
    *    成功返回true，失败返回false
    * 注意事项：
    *    - LowMemoryMappedIoSpace：32位地址空间的MMIO（通常1-3GB）
    *    - HighMemoryMappedIoSpace：64位地址空间的MMIO（通常16-32GB）
    *    - 这是GPU-PV正常工作的必要条件
    *********************************************************************************/
    static bool ConfigureMMIOSpace(
        const std::string& strVMName,
        std::string& strError
    );
    
    /********************************************************************************
    * 函数名称：复制驱动文件（内部方法）
    * 函数功能：将GPU驱动文件复制到虚拟机系统分区
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    *    [IN]  const std::string& strDriverPath：驱动源路径
    *    [IN]  ProgressCallback callback：进度回调函数
    *    [OUT] std::string& strError：错误信息
    * 返回类型：bool
    *    成功返回true，失败返回false
    * 注意事项：
    *    - 需要挂载虚拟机磁盘
    *    - 复制到C:\Windows\System32\HostDriverStore\FileRepository
    *********************************************************************************/
    static bool CopyDriverFiles(
        const std::string& strVMName,
        const std::string& strDriverPath,
        ProgressCallback callback,
        std::string& strError
    );
    
    //==============================================================================
    // 辅助方法
    //==============================================================================
    
    /********************************************************************************
    * 函数名称：挂载虚拟机磁盘（内部方法）
    * 函数功能：挂载虚拟机的虚拟磁盘并返回系统盘符
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    *    [OUT] std::string& strError：错误信息
    * 返回类型：std::string
    *    系统盘符（如"C:"），失败返回空字符串
    *********************************************************************************/
    static std::string MountVMDisk(const std::string& strVMName, std::string& strError);
    
    /********************************************************************************
    * 函数名称：卸载虚拟机磁盘（内部方法）
    * 函数功能：卸载虚拟机的虚拟磁盘
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    *    [OUT] std::string& strError：错误信息
    * 返回类型：bool
    *    成功返回true，失败返回false
    *********************************************************************************/
    static bool DismountVMDisk(const std::string& strVMName, std::string& strError);
    
    /********************************************************************************
    * 函数名称：复制驱动文件夹（内部方法）
    * 函数功能：递归复制驱动文件夹到虚拟机
    * 函数参数：
    *    [IN]  const std::string& strSourcePath：源文件夹路径
    *    [IN]  const std::string& strDriveLetter：目标驱动器号
    *    [OUT] std::string& strError：错误信息
    * 返回类型：bool
    *    成功返回true，失败返回false
    *********************************************************************************/
    static bool CopyDriverFolder(
        const std::string& strSourcePath,
        const std::string& strDriveLetter,
        std::string& strError
    );

    /********************************************************************************
    * 函数名称：拷贝GPU服务驱动目录（内部方法）
    * 函数功能：获取GPU服务关联的驱动目录并拷贝到虚拟机
    * 函数参数：
    *    [IN]  const std::string& strGPUName：GPU名称
    *    [IN]  const std::string& strDriveLetter：目标驱动器号
    *    [IN]  ProgressCallback callback：进度回调
    *    [OUT] std::string& strError：错误信息
    * 返回类型：bool
    *    成功返回true，失败返回false
    *********************************************************************************/
    static bool CopyGPUServiceDriver(
        const std::string& strGPUName,
        const std::string& strDriveLetter,
        ProgressCallback callback,
        std::string& strError
    );

    /********************************************************************************
    * 函数名称：拷贝所有PnP驱动文件（内部方法）
    * 函数功能：遍历并拷贝所有与GPU关联的PnP驱动文件
    * 函数参数：
    *    [IN]  const std::string& strGPUName：GPU名称
    *    [IN]  const std::string& strDriveLetter：目标驱动器号
    *    [IN]  ProgressCallback callback：进度回调
    *    [OUT] std::string& strError：错误信息
    * 返回类型：bool
    *    成功返回true，失败返回false
    *********************************************************************************/
    static bool CopyPnPDriverFiles(
        const std::string& strGPUName,
        const std::string& strDriveLetter,
        ProgressCallback callback,
        std::string& strError
    );

    /********************************************************************************
    * 函数名称：拷贝NVIDIA特殊文件（内部方法）
    * 函数功能：为NVIDIA显卡拷贝特殊的Corporation目录
    * 函数参数：
    *    [IN]  const std::string& strGPUName：GPU名称
    *    [IN]  const std::string& strDriveLetter：目标驱动器号
    *    [IN]  ProgressCallback callback：进度回调
    *    [OUT] std::string& strError：错误信息
    * 返回类型：bool
    *    成功返回true，失败返回false
    *********************************************************************************/
    static bool CopyNvidiaSpecialFiles(
        const std::string& strGPUName,
        const std::string& strDriveLetter,
        ProgressCallback callback,
        std::string& strError
    );

    /********************************************************************************
    * 函数名称：验证虚拟机中的GPU设备状态（内部方法）
    * 函数功能：通过Enter-PSSession或WMI检查虚拟机中GPU设备的状态
    * 函数参数：
    *    [IN]  const std::string& strVMName：虚拟机名称
    *    [IN]  const std::string& strGPUName：GPU名称
    *    [IN]  ProgressCallback callback：进度回调函数
    * 返回类型：bool
    *    成功返回true，失败返回false
    * 注意事项：
    *    - 需要虚拟机正在运行
    *    - 需要虚拟机启用PowerShell远程或WMI访问
    *    - 如果无法连接，不会阻塞配置流程
    *********************************************************************************/
    static bool VerifyGPUDeviceInVM(
        const std::string& strVMName,
        const std::string& strGPUName,
        ProgressCallback callback
    );
};
