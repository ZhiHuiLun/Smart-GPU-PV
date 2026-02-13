/********************************************************************************
* 文件名称：PowerShellExecutor.cpp
* 文件功能：实现PowerShell命令执行功能
* 
* 实现说明：
*    本文件通过CreateProcess API创建PowerShell进程，使用匿名管道捕获
*    标准输出和标准错误。支持长时间运行的命令，并自动处理编码问题。
* 
* 技术要点：
*    1. 命令行转义：正确处理双引号和反斜杠的转义
*    2. 管道操作：使用两个独立线程并行读取stdout和stderr，防止死锁
*    3. 编码处理：设置UTF-8输出编码，并自动修复GBK乱码
*    4. BOM处理：自动移除UTF-8 BOM标记
* 
* 作者：Smart-GPU-PV Team
* 日期：2026-01-26
* 版本：v2.0
*********************************************************************************/

#include "PowerShellExecutor.h"
#include "Utils.h"
#include <vector>
#include <thread>
#include <string>

/********************************************************************************
* 函数名称：移除UTF-8 BOM（内部辅助函数）
* 函数功能：移除字符串开头的UTF-8 BOM标记（0xEF 0xBB 0xBF）
* 函数参数：
*    [IN/OUT] std::string& str：待处理的字符串
* 返回类型：void
*********************************************************************************/
void RemoveBOM(std::string& str) {
    // 1. 检查是否存在UTF-8 BOM（3字节：0xEF 0xBB 0xBF）
    if (str.size() >= 3 && 
        static_cast<unsigned char>(str[0]) == 0xEF && 
        static_cast<unsigned char>(str[1]) == 0xBB && 
        static_cast<unsigned char>(str[2]) == 0xBF) {
        // 2. 移除BOM
        str = str.substr(3);
    }
}

/********************************************************************************
* 函数实现：执行PowerShell命令
*********************************************************************************/
std::string PowerShellExecutor::Execute(const std::string& strCommand) {
    // 1. 调用带错误检查的执行函数
    std::string strOutput, strError;
    ExecuteWithCheck(strCommand, strOutput, strError);
    
    // 2. 返回标准输出（忽略错误输出）
    return strOutput;
}

/********************************************************************************
* 函数实现：执行PowerShell命令并检查
*********************************************************************************/
bool PowerShellExecutor::ExecuteWithCheck(const std::string& strCommand, 
                                          std::string& strOutput, 
                                          std::string& strError) {
    // 1. 构造完整的PowerShell命令（设置UTF-8输出编码）
    std::string strFullCommand = "[Console]::OutputEncoding = [System.Text.Encoding]::UTF8; " + strCommand;
    
    // 2. 转义命令行参数（处理双引号和反斜杠）
    // 2.1 初始化转义后的命令字符串
    std::string strEscapedCommand;
    int nBackslashes = 0;
    
    // 2.2 逐字符处理转义
    for (char c : strFullCommand) {
        if (c == '\\') {
            // 2.2.1 计数连续的反斜杠
            nBackslashes++;
        } else if (c == '"') {
            // 2.2.2 双引号前需要双倍反斜杠进行转义
            strEscapedCommand.append(nBackslashes * 2, '\\');
            nBackslashes = 0;
            strEscapedCommand += "\\\"";
        } else {
            // 2.2.3 普通字符，输出之前累积的反斜杠
            strEscapedCommand.append(nBackslashes, '\\');
            nBackslashes = 0;
            strEscapedCommand += c;
        }
    }
    
    // 2.3 处理末尾的反斜杠（因为后面还要接一个闭合引号）
    strEscapedCommand.append(nBackslashes * 2, '\\');

    // 3. 构建最终的CreateProcess命令行
    std::string strCmdLine = "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"";
    strCmdLine += strEscapedCommand;
    strCmdLine += "\"";
    
    // 4. 执行命令
    return ExecuteCommand(strCmdLine, strOutput, strError);
}

/********************************************************************************
* 函数实现：执行PowerShell脚本文件
*********************************************************************************/
std::string PowerShellExecutor::ExecuteScript(const std::string& strScriptPath) {
    // 1. 构建脚本执行命令行
    std::string strCmdLine = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" + strScriptPath + "\"";
    
    // 2. 执行脚本
    std::string strOutput, strError;
    ExecuteCommand(strCmdLine, strOutput, strError);
    
    return strOutput;
}

/********************************************************************************
* 函数实现：创建管道并执行命令
*********************************************************************************/
bool PowerShellExecutor::ExecuteCommand(const std::string& strCmdLine, 
                                        std::string& strOutput, 
                                        std::string& strError) {
    // 1. 初始化安全属性（允许句柄继承）
    SECURITY_ATTRIBUTES stcSA = {0};
    stcSA.nLength = sizeof(SECURITY_ATTRIBUTES);
    stcSA.bInheritHandle = TRUE;
    stcSA.lpSecurityDescriptor = nullptr;
    
    // 2. 创建标准输出管道
    HANDLE hStdoutRead = nullptr;
    HANDLE hStdoutWrite = nullptr;
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &stcSA, 0)) {
        strError = "无法创建输出管道";
        return false;
    }
    
    // 3. 创建标准错误管道
    HANDLE hStderrRead = nullptr;
    HANDLE hStderrWrite = nullptr;
    if (!CreatePipe(&hStderrRead, &hStderrWrite, &stcSA, 0)) {
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        strError = "无法创建错误管道";
        return false;
    }
    
    // 4. 确保读取端不被子进程继承
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);
    
    // 5. 配置进程启动信息
    STARTUPINFOA stcSI = {0};
    stcSI.cb = sizeof(STARTUPINFOA);
    stcSI.hStdOutput = hStdoutWrite;
    stcSI.hStdError = hStderrWrite;
    stcSI.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    stcSI.wShowWindow = SW_HIDE;  // 隐藏PowerShell窗口
    
    PROCESS_INFORMATION stcPI = {0};
    
    // 6. 准备可修改的命令行缓冲区（CreateProcessA要求）
    std::vector<char> vecCmdLineBuf(strCmdLine.begin(), strCmdLine.end());
    vecCmdLineBuf.push_back(0);  // 添加空终止符
    
    // 7. 创建PowerShell进程
    BOOL bSuccess = CreateProcessA(
        nullptr,                    // 应用程序名称（使用命令行中的）
        vecCmdLineBuf.data(),      // 命令行
        nullptr,                    // 进程安全属性
        nullptr,                    // 线程安全属性
        TRUE,                       // 继承句柄
        CREATE_NO_WINDOW,          // 创建标志（无窗口）
        nullptr,                    // 环境变量
        nullptr,                    // 当前目录
        &stcSI,                    // 启动信息
        &stcPI                     // 进程信息
    );
    
    // 8. 父进程不需要写入端，立即关闭
    CloseHandle(hStdoutWrite);
    CloseHandle(hStderrWrite);
    
    // 9. 检查进程创建是否成功
    if (!bSuccess) {
        CloseHandle(hStdoutRead);
        CloseHandle(hStderrRead);
        strError = "无法启动PowerShell进程";
        return false;
    }
    
    // 10. 使用两个线程并行读取输出和错误（防止管道缓冲区满导致死锁）
    std::string strRawOutput, strRawError;
    
    // 10.1 创建标准输出读取线程
    std::thread objStdoutThread([&]() {
        strRawOutput = ReadFromPipe(hStdoutRead);
    });
    
    // 10.2 创建标准错误读取线程
    std::thread objStderrThread([&]() {
        strRawError = ReadFromPipe(hStderrRead);
    });
    
    // 11. 等待进程结束（最多60秒超时）
    WaitForSingleObject(stcPI.hProcess, 60000);
    
    // 12. 等待读取线程完成
    if (objStdoutThread.joinable()) objStdoutThread.join();
    if (objStderrThread.joinable()) objStderrThread.join();
    
    // 13. 获取进程退出码
    DWORD dwExitCode = 0;
    GetExitCodeProcess(stcPI.hProcess, &dwExitCode);
    
    // 14. 清理进程和管道句柄
    CloseHandle(stcPI.hProcess);
    CloseHandle(stcPI.hThread);
    CloseHandle(hStdoutRead);
    CloseHandle(hStderrRead);
    
    // 15. 处理输出数据
    // 15.1 移除BOM标记
    RemoveBOM(strRawOutput);
    RemoveBOM(strRawError);
    
    // 15.2 修复可能的GBK乱码
    strRawOutput = Utils::RepairString(strRawOutput);
    strRawError = Utils::RepairString(strRawError);
    
    // 15.3 修剪首尾空白
    strOutput = Utils::Trim(strRawOutput);
    strError = Utils::Trim(strRawError);
    
    // 16. 返回执行结果（退出码0表示成功）
    return (dwExitCode == 0);
}

/********************************************************************************
* 函数实现：从管道读取数据
*********************************************************************************/
std::string PowerShellExecutor::ReadFromPipe(HANDLE hPipe) {
    // 1. 初始化结果字符串和缓冲区
    std::string strResult;
    char szBuffer[4096] = {0};
    DWORD dwBytesRead = 0;
    
    // 2. 循环读取管道数据直到管道关闭
    while (true) {
        // 2.1 从管道读取数据
        BOOL bSuccess = ReadFile(hPipe, szBuffer, sizeof(szBuffer) - 1, &dwBytesRead, nullptr);
        
        // 2.2 检查读取是否成功或已到达数据末尾
        if (!bSuccess || dwBytesRead == 0) {
            break;
        }
        
        // 2.3 添加空终止符并追加到结果
        szBuffer[dwBytesRead] = '\0';
        strResult += szBuffer;
    }
    
    // 3. 返回原始数据（不在此处处理，由ExecuteCommand统一处理）
    return strResult;
}
