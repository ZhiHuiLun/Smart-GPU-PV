#include "MainWindow.h"
#include "resource.h"
#include "Utils.h"
#include "GPUPVConfigurator.h"
#include <commctrl.h>

// 构造函数
MainWindow::MainWindow() : m_hDlg(nullptr) {
}

// 析构函数
MainWindow::~MainWindow() {
}

// 显示主窗口
INT_PTR MainWindow::Show(HINSTANCE hInstance) {
    return DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_MAIN_DIALOG), nullptr, DialogProc, (LPARAM)this);
}

// 对话框过程
INT_PTR CALLBACK MainWindow::DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = nullptr;
    
    if (message == WM_INITDIALOG) {
        // 保存this指针
        SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
        pThis = reinterpret_cast<MainWindow*>(lParam);
        pThis->OnInitDialog(hDlg);
        return TRUE;
    }
    
    pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
    if (!pThis) return FALSE;
    
    switch (message) {
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case IDC_BUTTON_REFRESH:
                    pThis->OnRefresh();
                    return TRUE;
                    
                case IDC_BUTTON_CONFIGURE:
                    pThis->OnConfigure();
                    return TRUE;
                    
                case IDCANCEL:
                    EndDialog(hDlg, 0);
                    return TRUE;
            }
            break;
        }
        
        case WM_CLOSE:
            EndDialog(hDlg, 0);
            return TRUE;
    }
    
    return FALSE;
}

// 初始化对话框
void MainWindow::OnInitDialog(HWND hDlg) {
    m_hDlg = hDlg;

    // 设置默认显存值
    SetDlgItemText(m_hDlg, IDC_EDIT_VRAM, L"4096");

    // 显示欢迎信息
    AppendLog(L"欢迎使用 Smart GPU-PV 配置工具");
    AppendLog(L"本程序需要管理员权限运行");
    AppendLog(L"------------------------------------");

    // 自动加载虚拟机和GPU列表
    OnRefresh();
}

void MainWindow::OnRefresh() {
    AppendLog(L"正在刷新虚拟机和GPU列表...");

    // 获取虚拟机列表
    m_vms = VMManager::GetAllVMs();
    if (m_vms.empty()) {
        AppendLog(L"警告: 未找到任何虚拟机");
    } else {
        AppendLog(L"找到 " + std::to_wstring(m_vms.size()) + L" 个虚拟机");
    }
    PopulateVMComboBox();

    // 获取GPU列表
    m_gpus = GPUManager::GetPartitionableGPUs();
    if (m_gpus.empty()) {
        AppendLog(L"警告: 未找到支持分区的GPU");
        AppendLog(L"请确保系统支持GPU-PV功能");
    } else {
        AppendLog(L"找到 " + std::to_wstring(m_gpus.size()) + L" 个可分区GPU");
    }
    PopulateGPUComboBox();

    AppendLog(L"刷新完成");
    AppendLog(L"------------------------------------");
}

void MainWindow::OnConfigure() {
    // 获取选中的虚拟机
    VMInfo vm = GetSelectedVM();
    if (vm.strName.empty()) {
        Utils::ShowError(m_hDlg, L"请选择一个虚拟机");
        return;
    }

    // 获取选中的GPU
    GPUInfo gpu = GetSelectedGPU();
    if (gpu.strFriendlyName.empty()) {
        Utils::ShowError(m_hDlg, L"请选择一个GPU");
        return;
    }

    // 获取显存大小
    int vramMB = GetVRAMSize();
    // 允许0MB，视为关闭GPU-PV
    if (vramMB < 0) {
        Utils::ShowError(m_hDlg, L"请输入有效的显存大小（非负整数）");
        return;
    }

    // 检查1：显存不能超过物理显存的90%
    if (vramMB >= 64) {
        uint64_t vramBytes = static_cast<uint64_t>(vramMB) * 1024 * 1024;
        uint64_t maxAllowed = static_cast<uint64_t>(gpu.ui64VramBytes * 0.9);
        
        if (vramBytes > maxAllowed) {
            std::wstring msg = L"设置的显存大小 (" + std::to_wstring(vramMB) + L" MB) 超过了物理显存的 90% (" + 
                              Utils::StringToWString(Utils::FormatVRAMSize(maxAllowed)) + LR"()。\n)" +
                              LR"(为了保证宿主机的稳定性，建议减少分配的显存。\n\n)" +
                              LR"(是否仍要继续？)";
            if (MessageBoxW(m_hDlg, msg.c_str(), L"显存警告", MB_YESNO | MB_ICONWARNING) != IDYES) {
                return;
            }
        }
    }

    // 检查2：重复设置检查
    if (vm.strGPUStatus == "On" && vramMB >= 64) {
        // 将当前VRAM转换为MB进行比较 (允许少量误差)
        int currentMB = static_cast<int>(vm.ui64VramBytes / (1024 * 1024));
        
        // 检查显卡是否一致（通过InstancePath）
        // 注意：这里简化比较，如果GPU路径包含关系成立且显存一致
        bool sameGPU = !vm.strGPUInstancePath.empty() &&
                      (vm.strGPUInstancePath.find(gpu.strInstancePath) != std::string::npos ||
                       gpu.strInstancePath.find(vm.strGPUInstancePath) != std::string::npos);
        
        if (sameGPU && std::abs(currentMB - vramMB) < 4) { // 4MB误差
            Utils::ShowInfo(m_hDlg, LR"(虚拟机已配置了相同的 GPU 和显存大小。\n无需重复设置。)");
            return;
        }
    }

    // 检查3：无效关闭检查
    if (vramMB < 64 && vm.strGPUStatus != "On") {
        Utils::ShowInfo(m_hDlg, L"虚拟机未开启 GPU-PV 功能，无需执行关闭操作。");
        return;
    }

    // 确认操作
    std::wstring confirmMsg;
    if (vramMB < 64) {
        confirmMsg = L"即将为虚拟机关闭 GPU-PV 功能：\n\n";
        confirmMsg += L"虚拟机: " + Utils::StringToWString(vm.strName) + L"\n";
        confirmMsg += L"显存设置: " + std::to_wstring(vramMB) + L" MB (小于64MB视为关闭)\n\n";
        confirmMsg += L"此操作将停止虚拟机并移除 GPU 分区适配器。\n";
        confirmMsg += L"是否继续？";
    } else {
        confirmMsg = L"即将为虚拟机配置GPU-PV：\n\n";
        confirmMsg += L"虚拟机: " + Utils::StringToWString(vm.strName) + L"\n";
        confirmMsg += L"GPU: " + Utils::StringToWString(gpu.strFriendlyName) + L"\n";
        confirmMsg += L"显存: " + std::to_wstring(vramMB) + L" MB\n\n";
        confirmMsg += L"此操作将停止虚拟机并修改其配置。\n";
        confirmMsg += L"是否继续？";
    }

    if (MessageBoxW(m_hDlg, confirmMsg.c_str(), L"确认操作", MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return;
    }

    // 禁用配置按钮
    EnableWindow(GetControl(IDC_BUTTON_CONFIGURE), FALSE);

    AppendLog(L"====================================");
    if (vramMB < 64) {
        AppendLog(L"开始关闭 GPU-PV...");
    } else {
        AppendLog(L"开始配置 GPU-PV...");
    }
    AppendLog(L"虚拟机: " + Utils::StringToWString(vm.strName));
    AppendLog(L"GPU: " + Utils::StringToWString(gpu.strFriendlyName));
    AppendLog(L"显存: " + std::to_wstring(vramMB) + L" MB");
    AppendLog(L"====================================");

    // 创建进度回调
    auto callback = [this](const std::string& message) {
        // 如果消息末尾有换行符，先移除，因为AppendLog会自动添加
        // 但这里我们为了兼容性，统一移除末尾换行，然后由AppendLog添加
        std::wstring wmsg = Utils::StringToWString(message);
        if (!wmsg.empty() && wmsg.back() == L'\n') {
            wmsg.pop_back();
        }
        AppendLog(wmsg);
    };

    // 执行配置
    bool success = GPUPVConfigurator::ConfigureGPUPV(
        vm.strName,
        gpu.strFriendlyName,
        gpu.strInstancePath,
        gpu.strDriverPath,
        vramMB,
        callback
    );

    // 重新启用按钮
    EnableWindow(GetControl(IDC_BUTTON_CONFIGURE), TRUE);

    if (success) {
        AppendLog(L"====================================");
        if (vramMB < 64) {
             AppendLog(L"GPU-PV 关闭操作完成！");
        } else {
             AppendLog(L"GPU-PV 配置成功完成！");
        }
        AppendLog(L"====================================");
        
        if (vramMB < 64) {
             Utils::ShowInfo(m_hDlg, L"GPU-PV已成功关闭！\n\n虚拟机已恢复到未配置GPU-PV的状态。");
        } else {
             Utils::ShowInfo(m_hDlg, L"GPU-PV配置成功！\n\n现在可以启动虚拟机并使用GPU加速功能。");
        }
    } else {
        AppendLog(L"====================================");
        AppendLog(L"GPU-PV 配置失败");
        AppendLog(L"====================================");
        Utils::ShowError(m_hDlg, L"GPU-PV配置失败，请查看日志了解详情。");
    }
}

// 填充虚拟机下拉框
void MainWindow::PopulateVMComboBox() {
    HWND hCombo = GetControl(IDC_COMBO_VM);
    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
    
    for (const auto& vm : m_vms) {
        std::wstring displayText = Utils::StringToWString(vm.strDisplayText);
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)displayText.c_str());
    }
    
    // 选择第一项
    if (!m_vms.empty()) {
        SendMessage(hCombo, CB_SETCURSEL, 0, 0);
    }
}

// 填充GPU下拉框
void MainWindow::PopulateGPUComboBox() {
    HWND hCombo = GetControl(IDC_COMBO_GPU);
    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
    
    for (const auto& gpu : m_gpus) {
        std::wstring displayText = Utils::StringToWString(gpu.strDisplayText);
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)displayText.c_str());
    }
    
    // 选择第一项
    if (!m_gpus.empty()) {
        SendMessage(hCombo, CB_SETCURSEL, 0, 0);
    }
}

// 追加日志
void MainWindow::AppendLog(const std::wstring& message) {
    HWND hLog = GetControl(IDC_EDIT_LOG);
    Utils::AppendLog(hLog, message);
}

// 获取控件句柄
HWND MainWindow::GetControl(int controlId) {
    return GetDlgItem(m_hDlg, controlId);
}

// 获取选中的虚拟机
VMInfo MainWindow::GetSelectedVM() {
    HWND hCombo = GetControl(IDC_COMBO_VM);
    int index = static_cast<int>(SendMessage(hCombo, CB_GETCURSEL, 0, 0));
    
    if (index >= 0 && index < static_cast<int>(m_vms.size())) {
        return m_vms[index];
    }
    
    return VMInfo();
}

// 获取选中的GPU
GPUInfo MainWindow::GetSelectedGPU() {
    HWND hCombo = GetControl(IDC_COMBO_GPU);
    int index = static_cast<int>(SendMessage(hCombo, CB_GETCURSEL, 0, 0));
    
    if (index >= 0 && index < static_cast<int>(m_gpus.size())) {
        return m_gpus[index];
    }
    
    return GPUInfo();
}

// 获取显存大小
int MainWindow::GetVRAMSize() {
    wchar_t buffer[256];
    GetDlgItemText(m_hDlg, IDC_EDIT_VRAM, buffer, 256);
    
    try {
        return std::stoi(buffer);
    } catch (...) {
        return 0;
    }
}