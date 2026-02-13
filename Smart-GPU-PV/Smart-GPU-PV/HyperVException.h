/********************************************************************************
* 文件名称：HyperVException.h
* 文件功能：定义Hyper-V操作异常类，用于统一处理WMI和系统API调用过程中的错误
* 
* 类说明：
*    HyperVException继承自std::runtime_error，提供了对Windows HRESULT错误码
*    的封装，便于在WMI操作失败时传递详细的错误信息。
* 
* 主要特性：
*    - 支持std::string和std::wstring两种消息类型
*    - 保存HRESULT错误码供上层分析
*    - 与标准C++异常体系兼容
* 
* 使用场景：
*    主要用于WMI操作失败时抛出异常，让调用方决定是重试、降级到PowerShell
*    还是向用户报告错误。
* 
* 作者：Smart-GPU-PV Team
* 日期：2026-01-26
* 版本：v2.0
*********************************************************************************/

#pragma once
#include <stdexcept>
#include <string>
#include <windows.h>

/********************************************************************************
* 类名称：Hyper-V操作异常
* 类功能：提供统一的异常处理机制，封装HRESULT错误码和错误消息
*********************************************************************************/
class HyperVException : public std::runtime_error {
public:
    /********************************************************************************
    * 函数名称：构造函数（字符串版本）
    * 函数功能：使用std::string消息和可选的HRESULT错误码创建异常对象
    * 函数参数：
    *    [IN]  const std::string& strMessage：错误消息
    *    [IN]  HRESULT hrResult：Windows错误码，默认为0
    * 返回类型：无（构造函数）
    * 调用示例：
    *    throw HyperVException("WMI查询失败", hr);
    *********************************************************************************/
    HyperVException(const std::string& strMessage, HRESULT hrResult = 0)
        : std::runtime_error(strMessage), m_hrResult(hrResult) {
    }
    
    /********************************************************************************
    * 函数名称：构造函数（宽字符串版本）
    * 函数功能：使用std::wstring消息和可选的HRESULT错误码创建异常对象
    * 函数参数：
    *    [IN]  const std::wstring& wstrMessage：错误消息（宽字符）
    *    [IN]  HRESULT hrResult：Windows错误码，默认为0
    * 返回类型：无（构造函数）
    * 调用示例：
    *    throw HyperVException(L"虚拟机不存在", E_INVALIDARG);
    *********************************************************************************/
    HyperVException(const std::wstring& wstrMessage, HRESULT hrResult = 0)
        : std::runtime_error(WStringToString(wstrMessage)), m_hrResult(hrResult) {
    }
    
    /********************************************************************************
    * 函数名称：获取错误码
    * 函数功能：返回异常关联的HRESULT错误码
    * 函数参数：
    *    无
    * 返回类型：HRESULT
    *    Windows错误码
    * 调用示例：
    *    catch (const HyperVException& e) {
    *        HRESULT hr = e.GetHResult();
    *    }
    *********************************************************************************/
    HRESULT GetHResult() const { return m_hrResult; }
    
private:
    HRESULT m_hrResult;  // 存储的HRESULT错误码
    
    /********************************************************************************
    * 函数名称：宽字符串转窄字符串（内部辅助）
    * 函数功能：将std::wstring转换为UTF-8编码的std::string
    * 函数参数：
    *    [IN]  const std::wstring& wstr：待转换的宽字符串
    * 返回类型：std::string
    *    转换后的UTF-8字符串
    *********************************************************************************/
    static std::string WStringToString(const std::wstring& wstr) {
        // 1. 处理空字符串
        if (wstr.empty()) return std::string();
        
        // 2. 计算转换后需要的缓冲区大小
        int nSize = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        
        // 3. 分配缓冲区并执行转换
        std::string str(nSize, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], nSize, nullptr, nullptr);
        
        // 4. 移除末尾的空字符
        if (nSize > 0 && str[nSize - 1] == '\0') {
            str.resize(nSize - 1);
        }
        
        return str;
    }
};
