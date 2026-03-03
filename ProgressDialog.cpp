#include "framework.h"
#include "ProgressDialog.h"
#include <sstream>
#include <iomanip>

#define IDC_PROGRESS_BAR    1001
#define IDC_STATUS_TEXT     1002
#define IDC_DETAIL_TEXT     1003
#define IDC_CANCEL_BUTTON   1004

ProgressDialog::ProgressDialog()
    : m_hDlg(nullptr)
    , m_hParent(nullptr)
    , m_hProgressBar(nullptr)
    , m_hStatusText(nullptr)
    , m_hDetailText(nullptr)
    , m_hCancelButton(nullptr)
    , m_cancelled(false)
    , m_shouldClose(false)
{
}

ProgressDialog::~ProgressDialog()
{
    Close();
}

void ProgressDialog::Show(HWND parent)
{
    m_hParent = parent;
    m_cancelled = false;
    m_shouldClose = false;
    
    // 在新线程中创建对话框
    m_thread = std::thread(&ProgressDialog::DialogThread, this);
}

void ProgressDialog::Close()
{
    m_shouldClose = true;
    
    // 等待对话框线程结束
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void ProgressDialog::DialogThread()
{
    // 初始化通用控件
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);
    
    // 创建对话框窗口
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = DialogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"ProgressDialogClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    RegisterClassExW(&wc);
    
    // 创建窗口
    m_hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"ProgressDialogClass",
        L"正在处理...",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        0, 0, 450, 200,
        m_hParent,
        NULL,
        GetModuleHandle(NULL),
        this
    );
    
    if (!m_hDlg) {
        return;
    }
    
    // 居中显示
    RECT rc;
    GetWindowRect(m_hDlg, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(m_hDlg, NULL,
        (screenWidth - width) / 2,
        (screenHeight - height) / 2,
        0, 0, SWP_NOSIZE | SWP_NOZORDER);
    
    InitDialog();
    ShowWindow(m_hDlg, SW_SHOW);
    UpdateWindow(m_hDlg);
    
    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) && !m_shouldClose) {
        if (!IsDialogMessage(m_hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    if (m_hDlg) {
        DestroyWindow(m_hDlg);
        m_hDlg = nullptr;
    }
}

void ProgressDialog::InitDialog()
{
    // 创建状态文本
    m_hStatusText = CreateWindowExW(
        0, L"STATIC", L"准备中...",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 20, 410, 20,
        m_hDlg, (HMENU)IDC_STATUS_TEXT,
        GetModuleHandle(NULL), NULL
    );
    
    // 创建进度条
    m_hProgressBar = CreateWindowExW(
        0, PROGRESS_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        20, 50, 410, 25,
        m_hDlg, (HMENU)IDC_PROGRESS_BAR,
        GetModuleHandle(NULL), NULL
    );
    
    SendMessage(m_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessage(m_hProgressBar, PBM_SETPOS, 0, 0);
    
    // 创建详细信息文本
    m_hDetailText = CreateWindowExW(
        0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_PATHELLIPSIS,
        20, 85, 410, 30,
        m_hDlg, (HMENU)IDC_DETAIL_TEXT,
        GetModuleHandle(NULL), NULL
    );
    
    // 创建取消按钮
    m_hCancelButton = CreateWindowExW(
        0, L"BUTTON", L"取消",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        175, 130, 100, 30,
        m_hDlg, (HMENU)IDC_CANCEL_BUTTON,
        GetModuleHandle(NULL), NULL
    );
    
    // 设置字体
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(m_hStatusText, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_hDetailText, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_hCancelButton, WM_SETFONT, (WPARAM)hFont, TRUE);
}

void ProgressDialog::UpdateProgress(ULONGLONG current, ULONGLONG total, const std::wstring& message)
{
    if (!m_hDlg) return;
    
    // 计算进度百分比
    int percentage = total > 0 ? (int)((current * 100) / total) : 0;
    
    // 更新进度条
    if (m_hProgressBar) {
        SendMessage(m_hProgressBar, PBM_SETPOS, percentage, 0);
    }
    
    // 更新状态文本
    if (m_hStatusText) {
        std::wstring statusText = FormatFileSize(current) + L" / " + 
                                  FormatFileSize(total) + L" (" + 
                                  std::to_wstring(percentage) + L"%)";
        SetWindowTextW(m_hStatusText, statusText.c_str());
    }
    
    // 更新详细信息
    if (m_hDetailText && !message.empty()) {
        SetWindowTextW(m_hDetailText, message.c_str());
    }
}

INT_PTR CALLBACK ProgressDialog::DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    ProgressDialog* pThis = nullptr;
    
    if (message == WM_CREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (ProgressDialog*)pCreate->lpCreateParams;
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (ProgressDialog*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
    }
    
    switch (message) {
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_CANCEL_BUTTON) {
                if (pThis) {
                    int result = MessageBoxW(hDlg, L"\u786e\u5b9a\u8981\u53d6\u6d88\u5f53\u524d\u64cd\u4f5c\u5417\uff1f", L"\u786e\u8ba4\u53d6\u6d88",
                        MB_YESNO | MB_ICONQUESTION);
                    if (result == IDYES) {
                        pThis->m_cancelled = true;
                        EnableWindow(pThis->m_hCancelButton, FALSE);
                        SetWindowTextW(pThis->m_hStatusText, L"\u6b63\u5728\u53d6\u6d88...");
                    }
                }
                return TRUE;
            }
            break;
            
        case WM_CLOSE:
            if (pThis && !pThis->m_shouldClose) {
                // 防止用户直接关闭对话框
                int result = MessageBoxW(hDlg, L"\u786e\u5b9a\u8981\u53d6\u6d88\u5f53\u524d\u64cd\u4f5c\u5417\uff1f", L"\u786e\u8ba4\u53d6\u6d88",
                    MB_YESNO | MB_ICONQUESTION);
                if (result == IDYES) {
                    pThis->m_cancelled = true;
                    pThis->m_shouldClose = true;
                }
                return TRUE;
            }
            break;
    }
    
    return FALSE;
}

std::wstring ProgressDialog::FormatFileSize(ULONGLONG size)
{
    const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
    int unitIndex = 0;
    double displaySize = (double)size;
    
    while (displaySize >= 1024.0 && unitIndex < 4) {
        displaySize /= 1024.0;
        unitIndex++;
    }
    
    std::wostringstream oss;
    oss << std::fixed << std::setprecision(2) << displaySize << L" " << units[unitIndex];
    return oss.str();
}
