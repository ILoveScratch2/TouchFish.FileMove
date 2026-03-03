#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include <atomic>

// 文件操作类 - 负责文件移动、复制和符号链接创建
class FileOperations
{
public:
    // 进度回调函数类型: (当前进度, 总大小, 当前文件名)
    using ProgressCallback = std::function<void(ULONGLONG, ULONGLONG, const std::wstring&)>;
    
    // 操作结果结构
    struct OperationResult {
        bool success;
        std::wstring errorMessage;
        DWORD errorCode;
    };

    FileOperations();
    ~FileOperations();

    // 移动目录（异步操作）
    // sourcePath: 源路径
    // destPath: 目标路径
    // createSymlink: 是否在源位置创建符号链接
    // progressCallback: 进度回调函数
    OperationResult MoveDirectory(
        const std::wstring& sourcePath,
        const std::wstring& destPath,
        bool createSymlink,
        ProgressCallback progressCallback = nullptr
    );

    // 取消当前操作
    void CancelOperation();

    // 检查路径是否有效
    static bool IsValidPath(const std::wstring& path);

    // 检查是否有足够的权限
    static bool HasPermission(const std::wstring& path);

    // 创建符号链接（Junction Point）
    static OperationResult CreateSymbolicLink(
        const std::wstring& linkPath,
        const std::wstring& targetPath
    );

    // 获取目录大小
    static ULONGLONG GetDirectorySize(const std::wstring& path);

    // 检查是否在同一驱动器
    static bool IsSameDrive(const std::wstring& path1, const std::wstring& path2);

private:
    std::atomic<bool> m_cancelled;
    
    // 复制目录（递归）
    OperationResult CopyDirectoryRecursive(
        const std::wstring& source,
        const std::wstring& dest,
        ULONGLONG totalSize,
        ULONGLONG& processedSize,
        ProgressCallback progressCallback
    );

    // 复制单个文件
    OperationResult CopyFileTo(
        const std::wstring& source,
        const std::wstring& dest,
        ULONGLONG totalSize,
        ULONGLONG& processedSize,
        ProgressCallback progressCallback
    );

    // 删除目录（递归）
    static OperationResult DeleteDirectoryRecursive(const std::wstring& path);

    // 获取错误消息
    static std::wstring GetLastErrorMessage(DWORD errorCode);
};
