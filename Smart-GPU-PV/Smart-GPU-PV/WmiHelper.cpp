#include "WmiHelper.h"
#include <stdexcept>

// Session 实现
WmiHelper::Session::Session(const std::wstring& wmiNamespace) 
    : m_pLoc(nullptr), m_pSvc(nullptr) {
    
    HRESULT hr = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&m_pLoc
    );
    
    if (FAILED(hr) || !m_pLoc) {
        throw std::runtime_error("Failed to create WbemLocator");
    }
    
    hr = m_pLoc->ConnectServer(
        _bstr_t(wmiNamespace.c_str()),
        NULL,
        NULL,
        0,
        NULL,
        0,
        0,
        &m_pSvc
    );
    
    if (FAILED(hr) || !m_pSvc) {
        if (m_pLoc) m_pLoc->Release();
        throw std::runtime_error("Failed to connect to WMI namespace");
    }
    
    hr = CoSetProxyBlanket(
        m_pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE
    );
    
    if (FAILED(hr)) {
        if (m_pSvc) m_pSvc->Release();
        if (m_pLoc) m_pLoc->Release();
        throw std::runtime_error("Failed to set proxy blanket");
    }
}

WmiHelper::Session::~Session() {
    if (m_pSvc) m_pSvc->Release();
    if (m_pLoc) m_pLoc->Release();
}

// QueryResult 实现
WmiHelper::QueryResult::QueryResult(IEnumWbemClassObject* pEnumerator)
    : m_pEnumerator(pEnumerator) {
}

WmiHelper::QueryResult::~QueryResult() {
    if (m_pEnumerator) {
        m_pEnumerator->Release();
    }
}

bool WmiHelper::QueryResult::Next(IWbemClassObject** ppObject) {
    if (!m_pEnumerator) return false;
    
    ULONG uReturn = 0;
    HRESULT hr = m_pEnumerator->Next(WBEM_INFINITE, 1, ppObject, &uReturn);
    
    return (SUCCEEDED(hr) && uReturn > 0);
}

// 执行WQL查询
std::unique_ptr<WmiHelper::QueryResult> WmiHelper::Query(
    Session& session,
    const std::wstring& query) {
    
    if (!session.IsValid()) {
        throw std::runtime_error("Invalid WMI session");
    }
    
    IEnumWbemClassObject* pEnumerator = nullptr;
    HRESULT hr = session.GetServices()->ExecQuery(
        bstr_t("WQL"),
        bstr_t(query.c_str()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator
    );
    
    if (FAILED(hr)) {
        throw std::runtime_error("WMI query failed");
    }
    
    return std::make_unique<QueryResult>(pEnumerator);
}

// 获取属性值（字符串）
std::wstring WmiHelper::GetProperty(IWbemClassObject* pObject, const std::wstring& propName) {
    if (!pObject) return L"";
    
    VARIANT vtProp;
    VariantInit(&vtProp);
    
    HRESULT hr = pObject->Get(propName.c_str(), 0, &vtProp, 0, 0);
    
    std::wstring result;
    if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
        result = vtProp.bstrVal;
    }
    
    VariantClear(&vtProp);
    return result;
}

// 获取属性值（整数）
uint64_t WmiHelper::GetPropertyUInt64(IWbemClassObject* pObject, const std::wstring& propName) {
    if (!pObject) return 0;
    
    VARIANT vtProp;
    VariantInit(&vtProp);
    
    HRESULT hr = pObject->Get(propName.c_str(), 0, &vtProp, 0, 0);
    
    uint64_t result = 0;
    if (SUCCEEDED(hr)) {
        if (vtProp.vt == VT_I4) {
            result = vtProp.lVal;
        } else if (vtProp.vt == VT_UI4) {
            result = vtProp.ulVal;
        } else if (vtProp.vt == VT_I8) {
            result = vtProp.llVal;
        } else if (vtProp.vt == VT_UI8) {
            result = vtProp.ullVal;
        } else if (vtProp.vt == VT_BSTR) {
            // 有时WMI返回字符串形式的数字
            try {
                result = std::stoull(vtProp.bstrVal);
            } catch (...) {
                result = 0;
            }
        }
    }
    
    VariantClear(&vtProp);
    return result;
}

// 获取属性值（布尔）
bool WmiHelper::GetPropertyBool(IWbemClassObject* pObject, const std::wstring& propName) {
    if (!pObject) return false;
    
    VARIANT vtProp;
    VariantInit(&vtProp);
    
    HRESULT hr = pObject->Get(propName.c_str(), 0, &vtProp, 0, 0);
    
    bool result = false;
    if (SUCCEEDED(hr) && vtProp.vt == VT_BOOL) {
        result = (vtProp.boolVal == VARIANT_TRUE);
    }
    
    VariantClear(&vtProp);
    return result;
}

// 获取对象路径
std::wstring WmiHelper::GetObjectPath(IWbemClassObject* pObject) {
    return GetProperty(pObject, L"__PATH");
}

// 执行WMI方法
HRESULT WmiHelper::ExecuteMethod(
    Session& session,
    const std::wstring& objectPath,
    const std::wstring& methodName,
    IWbemClassObject* pInParams,
    IWbemClassObject** ppOutParams) {
    
    if (!session.IsValid()) {
        return E_FAIL;
    }
    
    return session.GetServices()->ExecMethod(
        _bstr_t(objectPath.c_str()),
        _bstr_t(methodName.c_str()),
        0,
        NULL,
        pInParams,
        ppOutParams,
        NULL
    );
}

// 创建方法参数对象
IWbemClassObject* WmiHelper::CreateMethodParams(
    Session& session,
    const std::wstring& className,
    const std::wstring& methodName) {
    
    if (!session.IsValid()) return nullptr;
    
    IWbemClassObject* pClass = nullptr;
    HRESULT hr = session.GetServices()->GetObject(
        _bstr_t(className.c_str()),
        0,
        NULL,
        &pClass,
        NULL
    );
    
    if (FAILED(hr) || !pClass) return nullptr;
    
    IWbemClassObject* pInParamsDefinition = nullptr;
    hr = pClass->GetMethod(methodName.c_str(), 0, &pInParamsDefinition, NULL);
    pClass->Release();
    
    if (FAILED(hr) || !pInParamsDefinition) return nullptr;
    
    IWbemClassObject* pInParams = nullptr;
    hr = pInParamsDefinition->SpawnInstance(0, &pInParams);
    pInParamsDefinition->Release();
    
    if (FAILED(hr)) return nullptr;
    
    return pInParams;
}

// 设置参数（字符串）
void WmiHelper::SetParam(IWbemClassObject* pParams, const std::wstring& name, const std::wstring& value) {
    if (!pParams) return;
    
    VARIANT vtProp;
    VariantInit(&vtProp);
    vtProp.vt = VT_BSTR;
    vtProp.bstrVal = SysAllocString(value.c_str());
    
    pParams->Put(name.c_str(), 0, &vtProp, 0);
    VariantClear(&vtProp);
}

// 设置参数（整数 - int/uint32）
void WmiHelper::SetParam(IWbemClassObject* pParams, const std::wstring& name, int value) {
    if (!pParams) return;
    
    VARIANT vtProp;
    VariantInit(&vtProp);
    vtProp.vt = VT_I4; // 使用 VT_I4 兼容 uint16/uint32
    vtProp.lVal = value;
    
    pParams->Put(name.c_str(), 0, &vtProp, 0);
    VariantClear(&vtProp);
}

// 设置参数（整数 - uint64）
void WmiHelper::SetParam(IWbemClassObject* pParams, const std::wstring& name, uint64_t value) {
    if (!pParams) return;
    
    VARIANT vtProp;
    VariantInit(&vtProp);
    vtProp.vt = VT_UI8;
    vtProp.ullVal = value;
    
    pParams->Put(name.c_str(), 0, &vtProp, 0);
    VariantClear(&vtProp);
}

// 设置参数（布尔）
void WmiHelper::SetParam(IWbemClassObject* pParams, const std::wstring& name, bool value) {
    if (!pParams) return;
    
    VARIANT vtProp;
    VariantInit(&vtProp);
    vtProp.vt = VT_BOOL;
    vtProp.boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
    
    pParams->Put(name.c_str(), 0, &vtProp, 0);
    VariantClear(&vtProp);
}

// 设置参数（字符串数组）
void WmiHelper::SetParam(IWbemClassObject* pParams, const std::wstring& name, const std::vector<std::wstring>& values) {
    if (!pParams) return;
    
    SAFEARRAY* psa = SafeArrayCreateVector(VT_BSTR, 0, static_cast<ULONG>(values.size()));
    if (!psa) return;
    
    for (LONG i = 0; i < (LONG)values.size(); i++) {
        BSTR bstr = SysAllocString(values[i].c_str());
        SafeArrayPutElement(psa, &i, bstr);
        SysFreeString(bstr);
    }
    
    VARIANT vtProp;
    VariantInit(&vtProp);
    vtProp.vt = VT_ARRAY | VT_BSTR;
    vtProp.parray = psa;
    
    pParams->Put(name.c_str(), 0, &vtProp, 0);
    VariantClear(&vtProp);
}

// 初始化COM
bool WmiHelper::InitializeCOM() {
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    return (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE);
}

// 反初始化COM
void WmiHelper::UninitializeCOM() {
    CoUninitialize();
}
