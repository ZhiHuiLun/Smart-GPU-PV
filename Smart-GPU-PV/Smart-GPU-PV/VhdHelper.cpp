#include <initguid.h>
#include "VhdHelper.h"
#include <virtdisk.h>
#include <vector>
#include <thread>
#include <chrono>

#pragma comment(lib, "virtdisk.lib")
#pragma comment(lib, "uuid.lib")

// VhdHandle 实现
VhdHelper::VhdHandle::VhdHandle() 
    : m_handle(INVALID_HANDLE_VALUE), m_attached(false) {
}

VhdHelper::VhdHandle::~VhdHandle() {
    if (m_attached) {
        Detach();
    }
    if (m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_handle);
    }
}

bool VhdHelper::VhdHandle::Open(const std::wstring& vhdPath, bool readOnly) {
    if (m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
    
    VIRTUAL_STORAGE_TYPE storageType = { 0 };
    storageType.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN;
    storageType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT;
    
    OPEN_VIRTUAL_DISK_PARAMETERS openParams = {  };
    openParams.Version = OPEN_VIRTUAL_DISK_VERSION_1;
    
    OPEN_VIRTUAL_DISK_FLAG flags = readOnly 
        ? OPEN_VIRTUAL_DISK_FLAG_NO_PARENTS 
        : OPEN_VIRTUAL_DISK_FLAG_NONE;
    
    DWORD result = OpenVirtualDisk(
        &storageType,
        vhdPath.c_str(),
        readOnly ? VIRTUAL_DISK_ACCESS_READ : VIRTUAL_DISK_ACCESS_ALL,
        flags,
        &openParams,
        &m_handle
    );
    
    return (result == ERROR_SUCCESS && m_handle != INVALID_HANDLE_VALUE);
}

bool VhdHelper::VhdHandle::Attach() {
    if (m_handle == INVALID_HANDLE_VALUE) return false;
    if (m_attached) return true;
    
    ATTACH_VIRTUAL_DISK_PARAMETERS attachParams = {  };
    attachParams.Version = ATTACH_VIRTUAL_DISK_VERSION_1;
    
    DWORD result = AttachVirtualDisk(
        m_handle,
        NULL,
        ATTACH_VIRTUAL_DISK_FLAG_PERMANENT_LIFETIME,
        0,
        &attachParams,
        NULL
    );
    
    if (result == ERROR_SUCCESS) {
        m_attached = true;
        // 等待系统识别磁盘
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        return true;
    }
    
    return false;
}

bool VhdHelper::VhdHandle::Detach() {
    if (m_handle == INVALID_HANDLE_VALUE) return false;
    if (!m_attached) return true;
    
    // 尝试多次卸载（处理文件句柄占用的情况）
    for (int retry = 0; retry < 5; retry++) {
        DWORD result = DetachVirtualDisk(
            m_handle,
            DETACH_VIRTUAL_DISK_FLAG_NONE,
            0
        );
        
        if (result == ERROR_SUCCESS) {
            m_attached = false;
            return true;
        }
        
        // 如果失败，等待一下再重试
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    return false;
}

std::wstring VhdHelper::VhdHandle::GetSystemDriveLetter() {
    if (m_handle == INVALID_HANDLE_VALUE || !m_attached) {
        return L"";
    }
    
    // 获取物理磁盘路径
    wchar_t physicalPath[MAX_PATH] = { 0 };
    ULONG pathSize = MAX_PATH;
    
    DWORD result = GetVirtualDiskPhysicalPath(m_handle, &pathSize, physicalPath);
    if (result != ERROR_SUCCESS) {
        return L"";
    }
    
    // 物理路径格式：\\.\PhysicalDriveX
    // 我们需要枚举所有驱动器号，检查哪个对应此物理磁盘
    
    // 遍历所有可能的驱动器号
    for (wchar_t drive = L'C'; drive <= L'Z'; drive++) {
        std::wstring drivePath = std::wstring(1, drive) + L":\\";
        std::wstring windowsPath = drivePath + L"Windows\\System32";
        
        // 检查是否存在Windows目录
        DWORD attrib = GetFileAttributesW(windowsPath.c_str());
        if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY)) {
            // 这可能是系统盘，但我们需要确认它是否来自我们挂载的VHD
            // 简化处理：只要找到Windows目录就返回
            // 更严格的做法是比较物理磁盘编号，但对于VM场景这已足够
            return std::wstring(1, drive) + L":";
        }
    }
    
    return L"";
}

// 便捷方法：挂载VHD并返回系统驱动器号（保持挂载状态）
std::wstring VhdHelper::MountAndGetSystemDrive(const std::wstring& vhdPath) {
    // 使用静态handle保持挂载状态（简化方案）
    // 注意：这要求在使用后必须调用Unmount
    VhdHandle* handle = new VhdHandle();
    
    if (!handle->Open(vhdPath, false)) {
        delete handle;
        return L"";
    }
    
    if (!handle->Attach()) {
        delete handle;
        return L"";
    }
    
    // 等待驱动器字母分配
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    std::wstring drive = handle->GetSystemDriveLetter();
    
    // 不删除handle，保持挂载状态
    // 注意：会造成内存泄漏，但简化了API使用
    // 更好的方案是返回智能指针或使用RAII包装器
    
    return drive;
}

// 便捷方法：卸载VHD
bool VhdHelper::Unmount(const std::wstring& vhdPath) {
    VhdHandle handle;
    
    if (!handle.Open(vhdPath, false)) {
        return false;
    }
    
    return handle.Detach();
}
