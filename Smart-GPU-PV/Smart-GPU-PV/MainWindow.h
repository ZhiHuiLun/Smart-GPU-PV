#pragma once
#include <windows.h>
#include <vector>
#include "VMManager.h"
#include "GPUManager.h"

// 主窗口类
class MainWindow {
public:
    MainWindow();
    ~MainWindow();
    
    // 显示主窗口对话框
    INT_PTR Show(HINSTANCE hInstance);
    
private:
    // 对话框句柄
    HWND m_hDlg;
    
    // 虚拟机和GPU列表
    std::vector<VMInfo> m_vms;
    std::vector<GPUInfo> m_gpus;
    
    // 对话框过程
    static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
    
    // 初始化对话框
    void OnInitDialog(HWND hDlg);
    
    // 刷新虚拟机和GPU列表
    void OnRefresh();
    
    // 配置GPU-PV按钮点击
    void OnConfigure();
    
    // 填充虚拟机下拉框
    void PopulateVMComboBox();
    
    // 填充GPU下拉框
    void PopulateGPUComboBox();
    
    // 追加日志
    void AppendLog(const std::wstring& message);
    
    // 获取控件句柄
    HWND GetControl(int controlId);
    
    // 获取选中的虚拟机
    VMInfo GetSelectedVM();
    
    // 获取选中的GPU
    GPUInfo GetSelectedGPU();
    
    // 获取输入的显存大小
    int GetVRAMSize();
};
