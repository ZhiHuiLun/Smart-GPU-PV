/********************************************************************************
* 文件名称：WmiHelper.h
* 文件功能：提供WMI（Windows Management Instrumentation）操作的封装类
* 
* 类说明：
*    WmiHelper封装了WMI编程的常用操作，简化了WMI查询、对象属性读取、
*    方法调用等复杂过程。主要用于查询Hyper-V虚拟机信息、GPU配置等。
* 
* 主要功能：
*    1. WMI会话管理（Session类，RAII自动管理连接）
*    2. WQL查询执行和结果遍历（QueryResult类）
*    3. 对象属性读取（支持字符串、整数、布尔类型）
*    4. WMI方法调用（如启动/停止虚拟机）
*    5. 方法参数设置
* 
* 技术实现：
*    - 使用IWbemLocator和IWbemServices进行WMI连接
*    - 采用RAII模式自动管理COM对象生命周期
*    - 支持root\virtualization\v2命名空间（Hyper-V V2 WMI）
*    - 通过IWbemClassObject读取和设置属性
* 
* 使用模式：
*    1. 调用InitializeCOM()初始化COM库
*    2. 创建Session对象连接到WMI命名空间
*    3. 使用Query()执行WQL查询
*    4. 遍历QueryResult获取每个对象
*    5. 使用GetProperty系列函数读取属性
*    6. 使用ExecuteMethod()调用WMI方法
* 
* 依赖项：
*    - wbemuuid.lib（WMI UUID库，已通过pragma自动链接）
*    - Windows API（COM、WMI）
* 
* 使用注意：
*    - 必须先调用InitializeCOM()
*    - Session对象会自动管理COM对象释放
*    - 建议在程序结束时调用UninitializeCOM()
* 
* 作者：Smart-GPU-PV Team
* 日期：2026-01-26
* 版本：v2.0
*********************************************************************************/

#pragma once
#include <windows.h>
#include <wbemidl.h>
#include <comdef.h>
#include <string>
#include <vector>
#include <map>
#include <memory>

#pragma comment(lib, "wbemuuid.lib")

/********************************************************************************
* 类名称：WMI辅助类
* 类功能：封装WMI操作，提供简化的查询、属性读取、方法调用接口
*********************************************************************************/
class WmiHelper {
public:
    //==============================================================================
    // 内嵌类：WMI会话管理
    //==============================================================================
    
    /********************************************************************************
    * 类名称：WMI连接会话（RAII管理）
    * 类功能：管理WMI命名空间连接，自动释放COM资源
    * 
    * 使用说明：
    *    Session类采用RAII模式，构造时自动连接WMI命名空间，析构时自动
    *    释放COM对象。无需手动管理资源。
    * 
    * 调用示例：
    *    WmiHelper::Session objSession(L"root\\virtualization\\v2");
    *    if (objSession.IsValid()) {
    *        // 使用会话进行查询
    *    }
    *********************************************************************************/
    class Session {
    public:
        /********************************************************************************
        * 函数名称：构造函数
        * 函数功能：连接到指定的WMI命名空间
        * 函数参数：
        *    [IN]  const std::wstring& wstrNamespace：WMI命名空间路径，
        *          默认为"root\\virtualization\\v2"（Hyper-V V2）
        * 返回类型：无（构造函数）
        * 调用示例：
        *    WmiHelper::Session objSession;  // 使用默认命名空间
        *    WmiHelper::Session objSession(L"root\\cimv2");  // 指定命名空间
        *********************************************************************************/
        Session(const std::wstring& wstrNamespace = L"root\\virtualization\\v2");
        
        /********************************************************************************
        * 函数名称：析构函数
        * 函数功能：自动释放WMI连接和相关COM对象
        *********************************************************************************/
        ~Session();
        
        /********************************************************************************
        * 函数名称：获取服务对象
        * 函数功能：返回IWbemServices指针，用于执行查询和方法调用
        * 函数参数：
        *    无
        * 返回类型：IWbemServices*
        *    WMI服务接口指针
        *********************************************************************************/
        IWbemServices* GetServices() const { return m_pSvc; }
        
        /********************************************************************************
        * 函数名称：检查连接有效性
        * 函数功能：判断WMI连接是否成功建立
        * 函数参数：
        *    无
        * 返回类型：bool
        *    连接有效返回true，否则返回false
        *********************************************************************************/
        bool IsValid() const { return m_pSvc != nullptr; }
        
    private:
        IWbemLocator* m_pLoc;     // WMI定位器对象
        IWbemServices* m_pSvc;    // WMI服务对象
    };
    
    //==============================================================================
    // 内嵌类：查询结果管理
    //==============================================================================
    
    /********************************************************************************
    * 类名称：查询结果（RAII管理）
    * 类功能：封装WMI查询结果枚举器，提供遍历查询结果的接口
    * 
    * 使用说明：
    *    QueryResult类封装了IEnumWbemClassObject枚举器，提供Next()方法
    *    遍历查询结果。析构时自动释放枚举器资源。
    * 
    * 调用示例：
    *    auto pResult = WmiHelper::Query(objSession, L"SELECT * FROM Msvm_ComputerSystem");
    *    IWbemClassObject* pObj = nullptr;
    *    while (pResult->Next(&pObj)) {
    *        // 处理对象
    *        pObj->Release();
    *    }
    *********************************************************************************/
    class QueryResult {
    public:
        /********************************************************************************
        * 函数名称：构造函数
        * 函数功能：创建查询结果对象，接管枚举器的所有权
        * 函数参数：
        *    [IN]  IEnumWbemClassObject* pEnumerator：WMI查询结果枚举器
        * 返回类型：无（构造函数）
        *********************************************************************************/
        QueryResult(IEnumWbemClassObject* pEnumerator);
        
        /********************************************************************************
        * 函数名称：析构函数
        * 函数功能：自动释放枚举器对象
        *********************************************************************************/
        ~QueryResult();
        
        /********************************************************************************
        * 函数名称：获取下一个对象
        * 函数功能：从查询结果中获取下一个WMI对象
        * 函数参数：
        *    [OUT] IWbemClassObject** ppObject：接收WMI对象指针
        * 返回类型：bool
        *    成功获取返回true，已遍历完返回false
        * 调用示例：
        *    IWbemClassObject* pObj = nullptr;
        *    while (pResult->Next(&pObj)) {
        *        // 处理pObj
        *        pObj->Release();  // 记得释放
        *    }
        * 注意事项：
        *    调用者负责释放返回的IWbemClassObject对象
        *********************************************************************************/
        bool Next(IWbemClassObject** ppObject);
        
    private:
        IEnumWbemClassObject* m_pEnumerator;  // WMI枚举器对象
    };
    
    //==============================================================================
    // 静态方法：查询操作
    //==============================================================================
    
    /********************************************************************************
    * 函数名称：执行WQL查询
    * 函数功能：在指定会话中执行WQL（WMI查询语言）查询
    * 函数参数：
    *    [IN]  Session& objSession：WMI会话对象
    *    [IN]  const std::wstring& wstrQuery：WQL查询语句
    * 返回类型：std::unique_ptr<QueryResult>
    *    查询结果对象智能指针，失败返回nullptr
    * 调用示例：
    *    auto pResult = WmiHelper::Query(objSession, 
    *        L"SELECT * FROM Msvm_ComputerSystem WHERE Caption='Virtual Machine'");
    *********************************************************************************/
    static std::unique_ptr<QueryResult> Query(
        Session& objSession,
        const std::wstring& wstrQuery
    );
    
    //==============================================================================
    // 静态方法：属性读取
    //==============================================================================
    
    /********************************************************************************
    * 函数名称：获取字符串属性
    * 函数功能：从WMI对象中读取指定属性的字符串值
    * 函数参数：
    *    [IN]  IWbemClassObject* pObject：WMI对象指针
    *    [IN]  const std::wstring& wstrPropName：属性名称
    * 返回类型：std::wstring
    *    属性值，若属性不存在或类型不匹配返回空字符串
    * 调用示例：
    *    std::wstring wstrName = WmiHelper::GetProperty(pObj, L"ElementName");
    *********************************************************************************/
    static std::wstring GetProperty(IWbemClassObject* pObject, const std::wstring& wstrPropName);
    
    /********************************************************************************
    * 函数名称：获取64位整数属性
    * 函数功能：从WMI对象中读取指定属性的64位无符号整数值
    * 函数参数：
    *    [IN]  IWbemClassObject* pObject：WMI对象指针
    *    [IN]  const std::wstring& wstrPropName：属性名称
    * 返回类型：uint64_t
    *    属性值，若属性不存在或类型不匹配返回0
    * 调用示例：
    *    uint64_t ui64State = WmiHelper::GetPropertyUInt64(pObj, L"EnabledState");
    *********************************************************************************/
    static uint64_t GetPropertyUInt64(IWbemClassObject* pObject, const std::wstring& wstrPropName);
    
    /********************************************************************************
    * 函数名称：获取布尔属性
    * 函数功能：从WMI对象中读取指定属性的布尔值
    * 函数参数：
    *    [IN]  IWbemClassObject* pObject：WMI对象指针
    *    [IN]  const std::wstring& wstrPropName：属性名称
    * 返回类型：bool
    *    属性值，若属性不存在或类型不匹配返回false
    * 调用示例：
    *    bool bEnabled = WmiHelper::GetPropertyBool(pObj, L"GuestControlledCacheTypes");
    *********************************************************************************/
    static bool GetPropertyBool(IWbemClassObject* pObject, const std::wstring& wstrPropName);
    
    /********************************************************************************
    * 函数名称：获取对象路径
    * 函数功能：获取WMI对象的完整路径（用于方法调用）
    * 函数参数：
    *    [IN]  IWbemClassObject* pObject：WMI对象指针
    * 返回类型：std::wstring
    *    对象路径，如"\\.\root\virtualization\v2:Msvm_ComputerSystem.CreationClassName=..."
    * 调用示例：
    *    std::wstring wstrPath = WmiHelper::GetObjectPath(pObj);
    *********************************************************************************/
    static std::wstring GetObjectPath(IWbemClassObject* pObject);
    
    //==============================================================================
    // 静态方法：方法调用
    //==============================================================================
    
    /********************************************************************************
    * 函数名称：执行WMI方法
    * 函数功能：在指定WMI对象上调用方法
    * 函数参数：
    *    [IN]  Session& objSession：WMI会话对象
    *    [IN]  const std::wstring& wstrObjectPath：对象路径
    *    [IN]  const std::wstring& wstrMethodName：方法名称
    *    [IN]  IWbemClassObject* pInParams：输入参数对象，默认为nullptr
    *    [OUT] IWbemClassObject** ppOutParams：输出参数对象指针，默认为nullptr
    * 返回类型：HRESULT
    *    Windows错误码，S_OK表示成功
    * 调用示例：
    *    HRESULT hr = WmiHelper::ExecuteMethod(objSession, wstrVMPath, 
    *        L"RequestStateChange", pInParams, &pOutParams);
    * 注意事项：
    *    调用者负责释放输出参数对象
    *********************************************************************************/
    static HRESULT ExecuteMethod(
        Session& objSession,
        const std::wstring& wstrObjectPath,
        const std::wstring& wstrMethodName,
        IWbemClassObject* pInParams = nullptr,
        IWbemClassObject** ppOutParams = nullptr
    );
    
    /********************************************************************************
    * 函数名称：创建方法参数对象
    * 函数功能：为指定类的方法创建输入参数对象
    * 函数参数：
    *    [IN]  Session& objSession：WMI会话对象
    *    [IN]  const std::wstring& wstrClassName：类名称
    *    [IN]  const std::wstring& wstrMethodName：方法名称
    * 返回类型：IWbemClassObject*
    *    参数对象指针，失败返回nullptr
    * 调用示例：
    *    IWbemClassObject* pParams = WmiHelper::CreateMethodParams(objSession,
    *        L"Msvm_ComputerSystem", L"RequestStateChange");
    * 注意事项：
    *    调用者负责释放返回的对象
    *********************************************************************************/
    static IWbemClassObject* CreateMethodParams(
        Session& objSession,
        const std::wstring& wstrClassName,
        const std::wstring& wstrMethodName
    );
    
    //==============================================================================
    // 静态方法：参数设置（重载）
    //==============================================================================
    
    /********************************************************************************
    * 函数名称：设置字符串参数
    * 函数功能：向参数对象设置字符串类型的参数值
    * 函数参数：
    *    [IN]  IWbemClassObject* pParams：参数对象
    *    [IN]  const std::wstring& wstrName：参数名称
    *    [IN]  const std::wstring& wstrValue：参数值
    * 返回类型：void
    * 调用示例：
    *    WmiHelper::SetParam(pParams, L"InstancePath", wstrGPUPath);
    *********************************************************************************/
    static void SetParam(IWbemClassObject* pParams, const std::wstring& wstrName, const std::wstring& wstrValue);
    
    /********************************************************************************
    * 函数名称：设置整数参数
    * 函数功能：向参数对象设置整数类型的参数值
    * 函数参数：
    *    [IN]  IWbemClassObject* pParams：参数对象
    *    [IN]  const std::wstring& wstrName：参数名称
    *    [IN]  int nValue：参数值
    * 返回类型：void
    * 调用示例：
    *    WmiHelper::SetParam(pParams, L"RequestedState", 2);
    *********************************************************************************/
    static void SetParam(IWbemClassObject* pParams, const std::wstring& wstrName, int nValue);
    
    /********************************************************************************
    * 函数名称：设置64位整数参数
    * 函数功能：向参数对象设置64位无符号整数类型的参数值
    * 函数参数：
    *    [IN]  IWbemClassObject* pParams：参数对象
    *    [IN]  const std::wstring& wstrName：参数名称
    *    [IN]  uint64_t ui64Value：参数值
    * 返回类型：void
    * 调用示例：
    *    WmiHelper::SetParam(pParams, L"MaxPartitionVRAM", ui64VRAM);
    *********************************************************************************/
    static void SetParam(IWbemClassObject* pParams, const std::wstring& wstrName, uint64_t ui64Value);
    
    /********************************************************************************
    * 函数名称：设置布尔参数
    * 函数功能：向参数对象设置布尔类型的参数值
    * 函数参数：
    *    [IN]  IWbemClassObject* pParams：参数对象
    *    [IN]  const std::wstring& wstrName：参数名称
    *    [IN]  bool bValue：参数值
    * 返回类型：void
    * 调用示例：
    *    WmiHelper::SetParam(pParams, L"GuestControlledCacheTypes", true);
    *********************************************************************************/
    static void SetParam(IWbemClassObject* pParams, const std::wstring& wstrName, bool bValue);
    
    /********************************************************************************
    * 函数名称：设置字符串数组参数
    * 函数功能：向参数对象设置字符串数组类型的参数值
    * 函数参数：
    *    [IN]  IWbemClassObject* pParams：参数对象
    *    [IN]  const std::wstring& wstrName：参数名称
    *    [IN]  const std::vector<std::wstring>& vecValues：参数值数组
    * 返回类型：void
    * 调用示例：
    *    std::vector<std::wstring> vecPaths = {L"path1", L"path2"};
    *    WmiHelper::SetParam(pParams, L"Paths", vecPaths);
    *********************************************************************************/
    static void SetParam(IWbemClassObject* pParams, const std::wstring& wstrName, const std::vector<std::wstring>& vecValues);
    
    //==============================================================================
    // 静态方法：COM初始化
    //==============================================================================
    
    /********************************************************************************
    * 函数名称：初始化COM库
    * 函数功能：初始化COM库和设置COM安全级别，必须在使用WMI前调用
    * 函数参数：
    *    无
    * 返回类型：bool
    *    初始化成功返回true，失败返回false
    * 调用示例：
    *    if (!WmiHelper::InitializeCOM()) {
    *        std::cerr << "COM初始化失败" << std::endl;
    *        return 1;
    *    }
    * 注意事项：
    *    - 应在程序开始时调用一次
    *    - 对应的应在程序结束时调用UninitializeCOM()
    *********************************************************************************/
    static bool InitializeCOM();
    
    /********************************************************************************
    * 函数名称：反初始化COM库
    * 函数功能：清理COM库资源，应在程序结束时调用
    * 函数参数：
    *    无
    * 返回类型：void
    * 调用示例：
    *    WmiHelper::UninitializeCOM();
    *********************************************************************************/
    static void UninitializeCOM();
};
