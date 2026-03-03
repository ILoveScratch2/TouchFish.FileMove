#pragma once
#include <windows.h>
#include <string>
#include <atomic>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

// 自定义消息
#define WM_UPDATE_PROGRESS (WM_USER + 1)

// 进度信息结构
struct ProgressInfo {
    ULONGLONG current;
    ULONGLONG total;
    std::wstring* message;
};

// 进度对话框类 - 模态对话框
class ProgressDialog
{
public:
    ProgressDialog();
    ~ProgressDialog();

    // 创建进度对话框（在当前线程）
    bool Create(HWND parent);

    // 关闭进度对话框
    void Close();

    // 更新进度（线程安全）
    void UpdateProgress(ULONGLONG current, ULONGLONG total, const std::wstring& message);

    // 检查是否被取消
    bool IsCancelled() const { return m_cancelled; }

    // 获取对话框句柄
    HWND GetHandle() const { return m_hDlg; }

    // 处理消息（需要在工作线程中定期调用）
    void ProcessMessages();

private:
    HWND m_hDlg;
    HWND m_hParent;
    HWND m_hProgressBar;
    HWND m_hStatusText;
    HWND m_hDetailText;
    HWND m_hCancelButton;
    
    std::atomic<bool> m_cancelled;
    std::atomic<bool> m_created;
    
    // 窗口过程
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    
    // 初始化对话框
    void InitDialog();
    
    // 格式化文件大小
    static std::wstring FormatFileSize(ULONGLONG size);
};
