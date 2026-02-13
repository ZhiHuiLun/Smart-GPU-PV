#include <windows.h>
#include <commctrl.h>  // 添加这行
#include "MainWindow.h"
#include "Utils.h"

// 程序入口点
int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nShowCmd);
    
    // 初始化公共控件
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);
    
    // 初始化COM库
    HRESULT hRes = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hRes)) {
        // 如果无法初始化COM，可能是一个问题，但我们可以尝试继续，
        // 或者记录错误。这里我们先忽略，因为后续WMI调用会再次检查或失败。
        // 但为了严谨，最好初始化。
    }

    // 检查是否以管理员权限运行
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    
    if (!isAdmin) {
        MessageBoxW(
            nullptr,
            L"本程序需要管理员权限才能运行。\n\n请右键点击程序，选择\"以管理员身份运行\"。",
            L"需要管理员权限",
            MB_OK | MB_ICONWARNING);
        return 1;
    }
    
    // 创建并显示主窗口
    MainWindow mainWindow;
    mainWindow.Show(hInstance);
    
    return 0;
}
