#pragma once
#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

// 进度对话框类 - 在独立线程中显示进度
class ProgressDialog
{
public:
    ProgressDialog();
    ~ProgressDialog();

    // 显示进度对话框（非阻塞）
    void Show(HWND parent);

    // 关闭进度对话框
    void Close();

    // 更新进度
    void UpdateProgress(ULONGLONG current, ULONGLONG total, const std::wstring& message);

    // 检查是否被取消
    bool IsCancelled() const { return m_cancelled; }

    // 获取对话框句柄
    HWND GetHandle() const { return m_hDlg; }

private:
    HWND m_hDlg;
    HWND m_hParent;
    HWND m_hProgressBar;
    HWND m_hStatusText;
    HWND m_hDetailText;
    HWND m_hCancelButton;
    
    std::thread m_thread;
    std::atomic<bool> m_cancelled;
    std::atomic<bool> m_shouldClose;
    
    // 对话框线程函数
    void DialogThread();
    
    // 对话框过程
    static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
    
    // 初始化对话框
    void InitDialog();
    
    // 格式化文件大小
    static std::wstring FormatFileSize(ULONGLONG size);
};
