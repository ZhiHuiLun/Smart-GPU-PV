/********************************************************************************
* 文件名称：PowerShellExecutor.h
* 文件功能：提供PowerShell命令执行功能，用于调用Hyper-V管理cmdlet
* 
* 类说明：
*    PowerShellExecutor封装了通过CreateProcess创建PowerShell进程并捕获
*    输出的功能。主要用作WMI API的降级方案，当WMI操作失败时，通过执行
*    PowerShell cmdlet来完成相应操作。
* 
* 主要功能：
*    1. 执行PowerShell命令并获取标准输出
*    2. 执行PowerShell命令并分别获取标准输出和错误输出
*    3. 执行PowerShell脚本文件
* 
* 技术实现：
*    - 使用CreateProcess创建PowerShell.exe进程
*    - 通过匿名管道捕获标准输出和标准错误
*    - 自动处理UTF-8和GBK编码
*    - 支持长时间运行的命令
* 
* 使用注意：
*    - PowerShell命令需要在当前用户权限下可执行
*    - 建议使用-NoProfile参数加快启动速度
*    - 中文输出可能需要编码转换（见Utils::RepairString）
* 
* 依赖项：
*    - Windows API（CreateProcess、管道操作）
* 
* 作者：Smart-GPU-PV Team
* 日期：2026-01-26
* 版本：v2.0
*********************************************************************************/

#pragma once
#include <string>
#include <windows.h>

/********************************************************************************
* 类名称：PowerShell命令执行器
* 类功能：封装PowerShell进程创建和输出捕获功能
*********************************************************************************/
class PowerShellExecutor {
public:
    /********************************************************************************
    * 函数名称：执行PowerShell命令
    * 函数功能：执行指定的PowerShell命令并返回标准输出
    * 函数参数：
    *    [IN]  const std::string& strCommand：要执行的PowerShell命令
    * 返回类型：std::string
    *    命令的标准输出内容，若执行失败返回空字符串
    * 调用示例：
    *    std::string strOutput = PowerShellExecutor::Execute("Get-VM");
    * 注意事项：
    *    - 此函数不捕获错误输出
    *    - 建议使用ExecuteWithCheck以获取更详细的错误信息
    *********************************************************************************/
    static std::string Execute(const std::string& strCommand);
    
    /********************************************************************************
    * 函数名称：执行PowerShell命令并检查
    * 函数功能：执行PowerShell命令并分别获取标准输出和错误输出
    * 函数参数：
    *    [IN]  const std::string& strCommand：要执行的PowerShell命令
    *    [OUT] std::string& strOutput：返回的标准输出内容
    *    [OUT] std::string& strError：返回的错误输出内容
    * 返回类型：bool
    *    执行成功：true
    *    执行失败：false
    * 调用示例：
    *    std::string strOutput, strError;
    *    if (!PowerShellExecutor::ExecuteWithCheck(cmd, strOutput, strError)) {
    *        std::cout << "错误: " << strError << std::endl;
    *    }
    *********************************************************************************/
    static bool ExecuteWithCheck(const std::string& strCommand, std::string& strOutput, std::string& strError);
    
    /********************************************************************************
    * 函数名称：执行PowerShell脚本文件
    * 函数功能：执行指定路径的PowerShell脚本文件(.ps1)
    * 函数参数：
    *    [IN]  const std::string& strScriptPath：脚本文件的完整路径
    * 返回类型：std::string
    *    脚本的输出内容
    * 调用示例：
    *    std::string strResult = PowerShellExecutor::ExecuteScript("C:\\Scripts\\test.ps1");
    * 注意事项：
    *    - 需要确保PowerShell执行策略允许运行脚本
    *    - 脚本路径中如有空格需要正确处理
    *********************************************************************************/
    static std::string ExecuteScript(const std::string& strScriptPath);
    
private:
    /********************************************************************************
    * 函数名称：创建管道并执行命令（内部辅助）
    * 函数功能：创建匿名管道，启动进程并捕获输出
    * 函数参数：
    *    [IN]  const std::string& strCmdLine：完整的命令行字符串
    *    [OUT] std::string& strOutput：捕获的标准输出
    *    [OUT] std::string& strError：捕获的错误输出
    * 返回类型：bool
    *    成功返回true，失败返回false
    *********************************************************************************/
    static bool ExecuteCommand(const std::string& strCmdLine, std::string& strOutput, std::string& strError);
    
    /********************************************************************************
    * 函数名称：从管道读取数据（内部辅助）
    * 函数功能：从指定管道句柄读取所有可用数据
    * 函数参数：
    *    [IN]  HANDLE hPipe：管道句柄
    * 返回类型：std::string
    *    读取到的数据
    *********************************************************************************/
    static std::string ReadFromPipe(HANDLE hPipe);
};
