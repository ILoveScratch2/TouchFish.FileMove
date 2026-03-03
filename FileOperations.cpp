#include "framework.h"
#include "FileOperations.h"
#include <shlwapi.h>
#include <vector>
#include <sstream>

#pragma comment(lib, "shlwapi.lib")

FileOperations::FileOperations() : m_cancelled(false)
{
}

FileOperations::~FileOperations()
{
}

void FileOperations::CancelOperation()
{
    m_cancelled = true;
}

bool FileOperations::IsValidPath(const std::wstring& path)
{
    if (path.empty()) return false;
    
    // 检查路径格式
    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool FileOperations::HasPermission(const std::wstring& path)
{
    // 尝试创建一个测试文件来检查权限
    std::wstring testPath = path + L"\\~test_permission.tmp";
    HANDLE hFile = CreateFileW(
        testPath.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    CloseHandle(hFile);
    return true;
}

ULONGLONG FileOperations::GetDirectorySize(const std::wstring& path)
{
    ULONGLONG totalSize = 0;
    std::wstring searchPath = path + L"\\*";
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    do {
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }
        
        std::wstring fullPath = path + L"\\" + findData.cFileName;
        
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            totalSize += GetDirectorySize(fullPath);
        } else {
            ULARGE_INTEGER fileSize;
            fileSize.LowPart = findData.nFileSizeLow;
            fileSize.HighPart = findData.nFileSizeHigh;
            totalSize += fileSize.QuadPart;
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
    return totalSize;
}

bool FileOperations::IsSameDrive(const std::wstring& path1, const std::wstring& path2)
{
    if (path1.length() < 2 || path2.length() < 2) return false;
    
    wchar_t drive1 = towupper(path1[0]);
    wchar_t drive2 = towupper(path2[0]);
    
    return drive1 == drive2;
}

std::wstring FileOperations::GetDirectoryName(const std::wstring& path)
{
    // 移除末尾的反斜杠
    std::wstring cleanPath = path;
    while (!cleanPath.empty() && (cleanPath.back() == L'\\' || cleanPath.back() == L'/')) {
        cleanPath.pop_back();
    }
    
    // 查找最后一个路径分隔符
    size_t pos = cleanPath.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return cleanPath;  // 没有路径分隔符，返回整个字符串
    }
    
    return cleanPath.substr(pos + 1);
}

std::wstring FileOperations::GetLastErrorMessage(DWORD errorCode)
{
    LPWSTR buffer = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&buffer,
        0,
        NULL
    );
    
    std::wstring message = buffer ? buffer : L"\u672a\u77e5\u9519\u8bef";
    if (buffer) LocalFree(buffer);
    
    return message;
}

FileOperations::OperationResult FileOperations::CreateSymbolicLink(
    const std::wstring& linkPath,
    const std::wstring& targetPath)
{
    OperationResult result;
    result.success = false;
    result.errorCode = 0;
    
    // 使用CreateSymbolicLinkW创建目录符号链接
    // 在Windows Vista及更高版本中，需要管理员权限
    DWORD flags = SYMBOLIC_LINK_FLAG_DIRECTORY;
    
    if (::CreateSymbolicLinkW(linkPath.c_str(), targetPath.c_str(), flags)) {
        result.success = true;
    } else {
        result.errorCode = GetLastError();
        std::wstring errMsg = L"创建符号链接失败: ";
        result.errorMessage = errMsg + GetLastErrorMessage(result.errorCode);
    }
    
    return result;
}

FileOperations::OperationResult FileOperations::MoveDirectory(
    const std::wstring& sourcePath,
    const std::wstring& destPath,
    bool createSymlink,
    ProgressCallback progressCallback)
{
    OperationResult result;
    result.success = false;
    result.errorCode = 0;
    m_cancelled = false;
    
    // 验证源路径
    if (!IsValidPath(sourcePath)) {
        result.errorMessage = L"\u6e90\u8def\u5f84\u65e0\u6548\u6216\u4e0d\u5b58\u5728";
        return result;
    }
    
    // 处理目标路径
    std::wstring finalDestPath = destPath;
    
    // 如果目标路径已存在，在其下创建源目录同名的子目录
    if (IsValidPath(destPath)) {
        std::wstring sourceDirName = GetDirectoryName(sourcePath);
        finalDestPath = destPath;
        if (!finalDestPath.empty() && finalDestPath.back() != L'\\' && finalDestPath.back() != L'/') {
            finalDestPath += L"\\";
        }
        finalDestPath += sourceDirName;
        
        // 检查新的目标路径是否也存在
        if (IsValidPath(finalDestPath)) {
            result.errorMessage = L"\u76ee\u6807\u8def\u5f84\u5df2\u5b58\u5728: " + finalDestPath;
            return result;
        }
    }
    
    // 获取总大小用于进度显示
    ULONGLONG totalSize = GetDirectorySize(sourcePath);
    ULONGLONG processedSize = 0;
    
    if (progressCallback) {
        progressCallback(0, totalSize, L"\u5f00\u59cb\u79fb\u52a8...");
    }
    
    // 检查是否在同一驱动器
    bool sameDrive = IsSameDrive(sourcePath, finalDestPath);
    
    if (sameDrive) {
        // 同一驱动器，直接移动
        if (MoveFileW(sourcePath.c_str(), finalDestPath.c_str())) {
            result.success = true;
            
            if (progressCallback) {
                progressCallback(totalSize, totalSize, L"\u79fb\u52a8\u5b8c\u6210");
            }
        } else {
            result.errorCode = GetLastError();
            std::wstring errMsg = L"\u79fb\u52a8\u5931\u8d25: ";
            result.errorMessage = errMsg + GetLastErrorMessage(result.errorCode);
            return result;
        }
    } else {
        // 不同驱动器，需要复制后删除
        // 1. 复制文件
        result = CopyDirectoryRecursive(sourcePath, finalDestPath, totalSize, processedSize, progressCallback);
        
        if (!result.success || m_cancelled) {
            // 复制失败或被取消，清理已复制的文件
            DeleteDirectoryRecursive(finalDestPath);
            if (m_cancelled) {
                result.errorMessage = L"\u64cd\u4f5c\u5df2\u53d6\u6d88";
            }
            return result;
        }
        
        // 2. 删除源目录
        result = DeleteDirectoryRecursive(sourcePath);
        if (!result.success || m_cancelled) {
            // 删除源目录失败或被取消，需要回滚
            std::wstring originalError = result.errorMessage;
            
            if (progressCallback) {
                progressCallback(0, 100, L"删除源目录失败，正在回滚...");
            }
            
            // 执行回滚
            auto rollbackResult = RollbackMove(sourcePath, finalDestPath, progressCallback);
            
            if (!rollbackResult.success) {
                // 回滚失败
                result.rollbackFailed = true;
                result.rollbackErrorMessage = rollbackResult.errorMessage;
                std::wstring errMsg = L"删除源目录失败: " + originalError + 
                                     L"\n\n尝试回滚也失败: " + rollbackResult.errorMessage +
                                     L"\n\n警告：部分文件可能已复制到目标位置，请手动检查和处理！";
                result.errorMessage = errMsg;
            } else {
                // 回滚成功
                if (m_cancelled) {
                    result.errorMessage = L"操作已取消，文件已回滚到原位置";
                } else {
                    result.errorMessage = L"删除源目录失败: " + originalError + 
                                         L"\n文件已回滚到原位置";
                }
            }
            return result;
        }
        
        if (progressCallback) {
            progressCallback(totalSize, totalSize, L"\u79fb\u52a8\u5b8c\u6210");
        }
    }
    
    // 3. 创建符号链接（如果需要）
    if (result.success && createSymlink) {
        auto symlinkResult = CreateSymbolicLink(sourcePath, finalDestPath);
        if (!symlinkResult.success) {
            // 符号链接创建失败，但文件已移动成功
            // 询问是否需要回滚
            std::wstring errMsg = L"文件移动成功，但创建符号链接失败: " + symlinkResult.errorMessage;
            
            // 这里不自动回滚，而是让上层决定
            result.errorMessage = errMsg;
            result.success = false;  // 标记为失败，让上层处理
            result.errorCode = symlinkResult.errorCode;
            return result;
        }
    }
    
    return result;
}

FileOperations::OperationResult FileOperations::CopyDirectoryRecursive(
    const std::wstring& source,
    const std::wstring& dest,
    ULONGLONG totalSize,
    ULONGLONG& processedSize,
    ProgressCallback progressCallback)
{
    OperationResult result;
    result.success = true;
    
    // 创建目标目录
    if (!CreateDirectoryW(dest.c_str(), NULL)) {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
            result.success = false;
            result.errorCode = error;
            std::wstring errMsg = L"创建目录失败: ";
            result.errorMessage = errMsg + GetLastErrorMessage(error);
            return result;
        }
    }
    
    // 遍历源目录
    std::wstring searchPath = source + L"\\*";
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        result.success = false;
        result.errorCode = GetLastError();
        result.errorMessage = L"无法访问源目录";
        return result;
    }
    
    do {
        if (m_cancelled) {
            result.success = false;
            result.errorMessage = L"操作已取消";
            FindClose(hFind);
            return result;
        }
        
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }
        
        std::wstring srcPath = source + L"\\" + findData.cFileName;
        std::wstring dstPath = dest + L"\\" + findData.cFileName;
        
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // 递归复制子目录
            result = CopyDirectoryRecursive(srcPath, dstPath, totalSize, processedSize, progressCallback);
            if (!result.success) {
                FindClose(hFind);
                return result;
            }
        } else {
            // 复制文件
            result = CopyFileTo(srcPath, dstPath, totalSize, processedSize, progressCallback);
            if (!result.success) {
                FindClose(hFind);
                return result;
            }
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
    return result;
}

FileOperations::OperationResult FileOperations::CopyFileTo(
    const std::wstring& source,
    const std::wstring& dest,
    ULONGLONG totalSize,
    ULONGLONG& processedSize,
    ProgressCallback progressCallback)
{
    OperationResult result;
    result.success = false;
    
    // 获取文件大小
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (!GetFileAttributesExW(source.c_str(), GetFileExInfoStandard, &fileInfo)) {
        result.errorCode = GetLastError();
        result.errorMessage = L"无法获取文件信息";
        return result;
    }
    
    ULARGE_INTEGER fileSize;
    fileSize.LowPart = fileInfo.nFileSizeLow;
    fileSize.HighPart = fileInfo.nFileSizeHigh;
    
    // 报告进度 - 使用totalSize作为总大小
    if (progressCallback) {
        progressCallback(processedSize, totalSize, source);
    }
    
    // 复制文件
    if (::CopyFileW(source.c_str(), dest.c_str(), FALSE)) {
        result.success = true;
        processedSize += fileSize.QuadPart;
    } else {
        result.errorCode = GetLastError();
        std::wstring errMsg = L"复制文件失败: ";
        result.errorMessage = errMsg + GetLastErrorMessage(result.errorCode);
    }
    
    return result;
}

// 回滚操作：将目标目录的内容移回源目录
FileOperations::OperationResult FileOperations::RollbackMove(
    const std::wstring& sourcePath,
    const std::wstring& destPath,
    ProgressCallback progressCallback)
{
    OperationResult result;
    result.success = false;
    
    if (progressCallback) {
        progressCallback(0, 100, L"正在回滚操作...");
    }
    
    // 检查目标路径是否存在
    if (!IsValidPath(destPath)) {
        result.errorMessage = L"目标路径不存在，无需回滚";
        result.success = true;  // 不算失败，因为没有需要回滚的内容
        return result;
    }
    
    // 尝试将目标目录移回源目录
    bool sameDrive = IsSameDrive(sourcePath, destPath);
    
    if (sameDrive) {
        // 同一驱动器，直接移动回去
        if (MoveFileW(destPath.c_str(), sourcePath.c_str())) {
            result.success = true;
            if (progressCallback) {
                progressCallback(100, 100, L"回滚完成");
            }
        } else {
            result.errorCode = GetLastError();
            std::wstring errMsg = L"回滚失败（无法移动目录）: ";
            result.errorMessage = errMsg + GetLastErrorMessage(result.errorCode);
        }
    } else {
        // 不同驱动器，需要复制回去然后删除目标
        ULONGLONG totalSize = GetDirectorySize(destPath);
        ULONGLONG processedSize = 0;
        
        // 复制回源目录
        result = CopyDirectoryRecursive(destPath, sourcePath, totalSize, processedSize, progressCallback);
        
        if (!result.success) {
            std::wstring errMsg = L"回滚失败（无法复制文件回源位置）: ";
            result.errorMessage = errMsg + result.errorMessage;
            return result;
        }
        
        // 删除目标目录
        auto deleteResult = DeleteDirectoryRecursive(destPath);
        if (!deleteResult.success) {
            // 复制回去成功但删除目标失败，这不算回滚失败
            result.success = true;
            result.errorMessage = L"文件已回滚到源位置，但目标位置的副本无法删除: " + deleteResult.errorMessage;
        } else {
            result.success = true;
            if (progressCallback) {
                progressCallback(100, 100, L"回滚完成");
            }
        }
    }
    
    return result;
}

FileOperations::OperationResult FileOperations::DeleteDirectoryRecursive(const std::wstring& path)
{
    OperationResult result;
    result.success = true;
    
    std::wstring searchPath = path + L"\\*";
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        result.success = false;
        result.errorCode = GetLastError();
        result.errorMessage = L"\u65e0\u6cd5\u8bbf\u95ee\u76ee\u5f55";
        return result;
    }
    
    do {
        // 检查是否被取消
        if (m_cancelled) {
            result.success = false;
            result.errorMessage = L"\u64cd\u4f5c\u5df2\u53d6\u6d88";
            FindClose(hFind);
            return result;
        }
        
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }
        
        std::wstring fullPath = path + L"\\" + findData.cFileName;
        
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            result = DeleteDirectoryRecursive(fullPath);
            if (!result.success) {
                FindClose(hFind);
                return result;
            }
        } else {
            if (!DeleteFileW(fullPath.c_str())) {
                result.success = false;
                result.errorCode = GetLastError();
                std::wstring errMsg = L"\u5220\u9664\u6587\u4ef6\u5931\u8d25: ";
                result.errorMessage = errMsg + GetLastErrorMessage(result.errorCode);
                FindClose(hFind);
                return result;
            }
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
    
    // 删除空目录
    if (!RemoveDirectoryW(path.c_str())) {
        result.success = false;
        result.errorCode = GetLastError();
        std::wstring errMsg = L"\u5220\u9664\u76ee\u5f55\u5931\u8d25: ";
        result.errorMessage = errMsg + GetLastErrorMessage(result.errorCode);
    }
    
    return result;
}
