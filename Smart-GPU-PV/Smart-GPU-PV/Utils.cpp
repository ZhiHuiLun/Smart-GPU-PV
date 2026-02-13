/********************************************************************************
* 文件名称：Utils.cpp
* 文件功能：实现通用工具函数，包括字符串处理、编码转换、日志输出等
* 
* 实现说明：
*    本文件实现了Utils类中声明的所有静态工具函数。主要包括Windows API
*    的字符串编码转换、标准库字符串处理、简单JSON解析、UI辅助函数等。
* 
* 作者：Smart-GPU-PV Team  
* 日期：2026-01-26
* 版本：v2.0
*********************************************************************************/

#include "Utils.h"
#include <sstream>
#include <algorithm>
#include <locale>
#include <codecvt>
#include <cstdio>

/********************************************************************************
* 函数实现：窄字符串转宽字符串
*********************************************************************************/
std::wstring Utils::StringToWString(const std::string& str) {
    // 1. 处理空字符串
    if (str.empty()) return std::wstring();
    
    // 2. 计算转换后需要的宽字符缓冲区大小
    int nSize = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (nSize <= 0) return std::wstring();
    
    // 3. 分配缓冲区并执行转换
    std::wstring wstr(nSize, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], nSize);
    
    // 4. 移除末尾的空字符（如果存在）
    if (nSize > 0 && wstr[nSize - 1] == L'\0') {
        wstr.resize(nSize - 1);
    }
    
    return wstr;
}

/********************************************************************************
* 函数实现：宽字符串转窄字符串
*********************************************************************************/
std::string Utils::WStringToString(const std::wstring& wstr) {
    // 1. 处理空字符串
    if (wstr.empty()) return std::string();
    
    // 2. 计算转换后需要的字节缓冲区大小
    int nSize = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (nSize <= 0) return std::string();
    
    // 3. 分配缓冲区并执行转换
    std::string str(nSize, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], nSize, nullptr, nullptr);
    
    // 4. 移除末尾的空字符（如果存在）
    if (nSize > 0 && str[nSize - 1] == '\0') {
        str.resize(nSize - 1);
    }
    
    return str;
}

/********************************************************************************
* 函数实现：字符串分割
*********************************************************************************/
std::vector<std::string> Utils::Split(const std::string& str, char cDelimiter) {
    // 1. 初始化结果容器
    std::vector<std::string> vecTokens;
    std::string strToken;
    
    // 2. 使用istringstream按分隔符读取
    std::istringstream objTokenStream(str);
    while (std::getline(objTokenStream, strToken, cDelimiter)) {
        vecTokens.push_back(strToken);
    }
    
    return vecTokens;
}

/********************************************************************************
* 函数实现：字符串修剪
*********************************************************************************/
std::string Utils::Trim(const std::string& str) {
    // 1. 查找第一个非空白字符位置
    size_t nFirst = str.find_first_not_of(" \t\r\n");
    if (nFirst == std::string::npos) return "";
    
    // 2. 查找最后一个非空白字符位置
    size_t nLast = str.find_last_not_of(" \t\r\n");
    
    // 3. 提取子字符串
    return str.substr(nFirst, nLast - nFirst + 1);
}

/********************************************************************************
* 函数实现：字符串包含检查
*********************************************************************************/
bool Utils::Contains(const std::string& str, const std::string& substr) {
    return str.find(substr) != std::string::npos;
}

/********************************************************************************
* 函数实现：格式化显存大小
*********************************************************************************/
std::string Utils::FormatVRAMSize(uint64_t ui64Bytes) {
    // 1. 将字节转换为MB
    double dMB = ui64Bytes / (1024.0 * 1024);
    
    // 2. 格式化为字符串
    char szBuffer[64] = {0};
    sprintf_s(szBuffer, "%.0fMB", dMB);
    
    return std::string(szBuffer);
}

/********************************************************************************
* 函数名称：JSON字符串反转义（内部辅助函数）
* 函数功能：处理JSON字符串中的转义字符（\n, \t, \", \\, \uXXXX等）
* 函数参数：
*    [IN]  const std::string& str：包含转义字符的JSON字符串
* 返回类型：std::string
*    反转义后的字符串
*********************************************************************************/
std::string UnescapeJsonString(const std::string& str) {
    // 1. 初始化结果字符串
    std::string strResult;
    strResult.reserve(str.length());
    
    // 2. 逐字符处理转义序列
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            char cNext = str[i + 1];
            
            // 2.1 处理常见转义字符
            switch (cNext) {
                case '"': strResult += '"'; i++; break;
                case '\\': strResult += '\\'; i++; break;
                case '/': strResult += '/'; i++; break;
                case 'b': strResult += '\b'; i++; break;
                case 'f': strResult += '\f'; i++; break;
                case 'n': strResult += '\n'; i++; break;
                case 'r': strResult += '\r'; i++; break;
                case 't': strResult += '\t'; i++; break;
                
                // 2.2 处理Unicode转义序列 \uXXXX
                case 'u': {
                    if (i + 5 < str.length()) {
                        std::string strHex = str.substr(i + 2, 4);
                        try {
                            // 2.2.1 解析十六进制数
                            int nCode = std::stoi(strHex, nullptr, 16);
                            
                            // 2.2.2 根据Unicode码点转换为UTF-8
                            if (nCode < 128) {
                                // ASCII范围，单字节
                                strResult += static_cast<char>(nCode);
                            } else if (nCode < 0x800) {
                                // 双字节UTF-8
                                strResult += static_cast<char>(0xC0 | (nCode >> 6));
                                strResult += static_cast<char>(0x80 | (nCode & 0x3F));
                            } else {
                                // 三字节UTF-8
                                strResult += static_cast<char>(0xE0 | (nCode >> 12));
                                strResult += static_cast<char>(0x80 | ((nCode >> 6) & 0x3F));
                                strResult += static_cast<char>(0x80 | (nCode & 0x3F));
                            }
                            i += 5;
                        } catch (...) {
                            // 2.2.3 解析失败，保留原样
                            strResult += "\\u";
                            i++;
                        }
                    } else {
                        strResult += "\\u";
                        i++;
                    }
                    break;
                }
                
                default:
                    // 2.3 未知转义序列，保留反斜杠
                    strResult += '\\';
                    break;
            }
        } else {
            // 2.4 普通字符，直接添加
            strResult += str[i];
        }
    }
    
    return strResult;
}

/********************************************************************************
* 函数实现：提取JSON值
*********************************************************************************/
std::string Utils::ExtractJsonValue(const std::string& strJson, const std::string& strKey) {
    // 1. 构造要查找的键名模式
    std::string strSearchKey = "\"" + strKey + "\"";
    size_t nKeyPos = strJson.find(strSearchKey);
    if (nKeyPos == std::string::npos) return "";
    
    // 2. 查找冒号位置
    size_t nColonPos = strJson.find(":", nKeyPos);
    if (nColonPos == std::string::npos) return "";
    
    // 3. 跳过冒号后的空白字符，定位值的起始位置
    size_t nValueStart = strJson.find_first_not_of(" \t\r\n", nColonPos + 1);
    if (nValueStart == std::string::npos) return "";
    
    // 4. 处理字符串类型的值（带引号）
    if (strJson[nValueStart] == '"') {
        size_t nCurrent = nValueStart + 1;
        
        // 4.1 查找字符串结束位置（考虑转义）
        while (nCurrent < strJson.length()) {
            if (strJson[nCurrent] == '"') {
                // 4.1.1 计算前面连续反斜杠的数量
                int nBackslashes = 0;
                if (nCurrent > 0) {
                    size_t p = nCurrent - 1;
                    while (p > nValueStart && strJson[p] == '\\') {
                        nBackslashes++;
                        p--;
                    }
                }
                
                // 4.1.2 偶数个反斜杠表示引号未被转义，是字符串结束
                if (nBackslashes % 2 == 0) {
                    std::string strRawValue = strJson.substr(nValueStart + 1, nCurrent - nValueStart - 1);
                    return UnescapeJsonString(strRawValue);
                }
            }
            nCurrent++;
        }
        return ""; // 未找到结束引号
    }
    
    // 5. 处理数字/布尔/null类型的值（不带引号）
    size_t nValueEnd = strJson.find_first_of(",}\r\n", nValueStart);
    if (nValueEnd == std::string::npos) nValueEnd = strJson.length();
    
    return Trim(strJson.substr(nValueStart, nValueEnd - nValueStart));
}

/********************************************************************************
* 函数实现：追加日志到编辑框
*********************************************************************************/
void Utils::AppendLog(HWND hEdit, const std::wstring& wstrMessage) {
    // 1. 获取编辑框当前文本长度
    int nLength = GetWindowTextLength(hEdit);
    
    // 2. 生成带时间戳的日志消息
    // 2.1 获取当前系统时间
    SYSTEMTIME stTime = {0};
    GetLocalTime(&stTime);
    
    // 2.2 格式化时间戳
    wchar_t wszTimestamp[64] = {0};
    swprintf_s(wszTimestamp, L"[%02d:%02d:%02d] ", stTime.wHour, stTime.wMinute, stTime.wSecond);
    
    // 2.3 组合完整日志消息
    std::wstring wstrLogMessage = wszTimestamp + wstrMessage + L"\r\n";
    
    // 3. 将光标移动到文本末尾
    SendMessage(hEdit, EM_SETSEL, nLength, nLength);
    
    // 4. 追加文本
    SendMessage(hEdit, EM_REPLACESEL, FALSE, (LPARAM)wstrLogMessage.c_str());
    
    // 5. 滚动到底部以显示最新日志
    SendMessage(hEdit, EM_SCROLLCARET, 0, 0);
}

/********************************************************************************
* 函数实现：显示错误消息框
*********************************************************************************/
void Utils::ShowError(HWND hWnd, const std::wstring& wstrMessage) {
    MessageBox(hWnd, wstrMessage.c_str(), L"错误", MB_OK | MB_ICONERROR);
}

/********************************************************************************
* 函数实现：显示信息消息框
*********************************************************************************/
void Utils::ShowInfo(HWND hWnd, const std::wstring& wstrMessage) {
    MessageBox(hWnd, wstrMessage.c_str(), L"信息", MB_OK | MB_ICONINFORMATION);
}

/********************************************************************************
* 函数实现：检查UTF-8有效性
*********************************************************************************/
bool Utils::IsValidUTF8(const std::string& str) {
    // 1. 获取字节指针
    const unsigned char* pBytes = (const unsigned char*)str.c_str();
    
    // 2. 逐字节检查UTF-8编码规则
    while (*pBytes) {
        if ((*pBytes & 0x80) == 0) {
            // 2.1 ASCII字符（0xxxxxxx）
            pBytes++;
        } else if ((*pBytes & 0xE0) == 0xC0) {
            // 2.2 双字节字符（110xxxxx 10xxxxxx）
            if ((pBytes[1] & 0xC0) != 0x80) return false;
            pBytes += 2;
        } else if ((*pBytes & 0xF0) == 0xE0) {
            // 2.3 三字节字符（1110xxxx 10xxxxxx 10xxxxxx）
            if ((pBytes[1] & 0xC0) != 0x80 || (pBytes[2] & 0xC0) != 0x80) return false;
            pBytes += 3;
        } else if ((*pBytes & 0xF8) == 0xF0) {
            // 2.4 四字节字符（11110xxx 10xxxxxx 10xxxxxx 10xxxxxx）
            if ((pBytes[1] & 0xC0) != 0x80 || (pBytes[2] & 0xC0) != 0x80 || (pBytes[3] & 0xC0) != 0x80) 
                return false;
            pBytes += 4;
        } else {
            // 2.5 无效的UTF-8字节序列
            return false;
        }
    }
    
    return true;
}

/********************************************************************************
* 函数实现：GBK转UTF-8
*********************************************************************************/
std::string Utils::ConvertGBKToUTF8(const std::string& str) {
    // 1. 先将GBK转换为宽字符（Unicode）
    int nLen = MultiByteToWideChar(936, 0, str.c_str(), -1, NULL, 0);
    if (nLen <= 0) return str;
    
    // 2. 分配缓冲区并执行转换
    std::wstring wstr(nLen, 0);
    MultiByteToWideChar(936, 0, str.c_str(), -1, &wstr[0], nLen);
    
    // 3. 移除末尾空字符
    if (nLen > 0 && wstr[nLen-1] == L'\0') 
        wstr.resize(nLen-1);
    
    // 4. 将宽字符转换为UTF-8
    return WStringToString(wstr);
}

/********************************************************************************
* 函数实现：修复乱码字符串
*********************************************************************************/
std::string Utils::RepairString(const std::string& str) {
    // 1. 处理空字符串
    if (str.empty()) return str;
    
    // 2. 检查是否已经是有效的UTF-8编码
    if (IsValidUTF8(str)) {
        return str;
    }
    
    // 3. 不是有效UTF-8，尝试作为GBK转换
    return ConvertGBKToUTF8(str);
}
