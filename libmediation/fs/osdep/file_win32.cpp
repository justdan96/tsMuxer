#include <io.h>
#include <windows.h>

#include "../directory.h"
#include "../file.h"

[[noreturn]] void throwFileError()
{
    LPVOID msgBuf = nullptr;
    const DWORD dw = GetLastError();

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, dw,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPTSTR>(&msgBuf), 0, nullptr);

    const std::string str(static_cast<char*>(msgBuf));
    throw std::runtime_error(str);
}

void makeWin32OpenFlags(const unsigned int oflag, DWORD* const dwDesiredAccess, DWORD* const dwCreationDisposition,
                        DWORD* const dwShareMode)
{
    *dwDesiredAccess = 0;
    *dwCreationDisposition = CREATE_ALWAYS;
    if (oflag & File::ofRead)
    {
        *dwDesiredAccess = GENERIC_READ & ~SYNCHRONIZE;
        *dwCreationDisposition = OPEN_EXISTING;
        *dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
    }
    if (oflag & File::ofWrite)
    {
        *dwDesiredAccess |= GENERIC_WRITE & ~SYNCHRONIZE;
        *dwCreationDisposition = CREATE_ALWAYS;
        *dwShareMode = FILE_SHARE_READ;
    }
    if (oflag & File::ofAppend)
        *dwCreationDisposition = OPEN_ALWAYS;
    if (oflag & File::ofNoTruncate)
        *dwCreationDisposition = OPEN_ALWAYS;
    if (oflag & File::ofOpenExisting)
        *dwCreationDisposition = OPEN_EXISTING;
    if (oflag & File::ofCreateNew)
        *dwCreationDisposition = CREATE_NEW;
}

File::File() : AbstractOutputStream(), m_impl(INVALID_HANDLE_VALUE), m_pos(0) {}

File::File(const char* fName, const unsigned int oflag,
           unsigned int systemDependentFlags) /* throw ( std::runtime_error ) */
    : AbstractOutputStream(), m_impl(INVALID_HANDLE_VALUE), m_name(fName), m_pos(0)
{
    DWORD dwDesiredAccess = 0;
    DWORD dwCreationDisposition = CREATE_ALWAYS;
    DWORD dwShareMode = 0;
    makeWin32OpenFlags(oflag, &dwDesiredAccess, &dwCreationDisposition, &dwShareMode);
    if (!systemDependentFlags)
    {
        if ((oflag & ofRead) && !(oflag & ofWrite))
            systemDependentFlags = FILE_FLAG_SEQUENTIAL_SCAN;
        else
            systemDependentFlags = FILE_FLAG_RANDOM_ACCESS;
    }
    m_impl = CreateFile(toWide(fName).data(), dwDesiredAccess, dwShareMode, nullptr, dwCreationDisposition,
                        systemDependentFlags, nullptr);
    if (m_impl == INVALID_HANDLE_VALUE)
    {
        throwFileError();
    }
    else
    {
        if (oflag & ofAppend)
        {
            long hiword = 0;
            const DWORD newPointerLow = SetFilePointer(m_impl, 0, &hiword, FILE_END);
            if (newPointerLow == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
                throwFileError();
        }
    }
}

File::~File()
{
    // TODO: fix use of virtual fucntion inside destructor
    if (isOpen())
        close();
}

bool File::open(const char* fName, const unsigned int oflag, unsigned int systemDependentFlags)
{
    m_name = fName;
    m_pos = 0;

    DWORD dwDesiredAccess = 0;
    DWORD dwCreationDisposition = CREATE_ALWAYS;
    DWORD dwShareMode = 0;
    makeWin32OpenFlags(oflag, &dwDesiredAccess, &dwCreationDisposition, &dwShareMode);
    if (!systemDependentFlags)
    {
        if ((oflag & ofRead) && !(oflag & ofWrite))
            systemDependentFlags = FILE_FLAG_SEQUENTIAL_SCAN;
        else
            systemDependentFlags = FILE_FLAG_RANDOM_ACCESS;
    }

    if ((oflag & ofOpenExisting) == 0)
    {
        createDir(extractFileDir(fName), true);
    }
    m_impl = CreateFile(toWide(fName).data(), dwDesiredAccess, dwShareMode, nullptr, dwCreationDisposition,
                        systemDependentFlags, nullptr);
    if (m_impl == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    if (oflag & ofAppend)
    {
        long hiword = 0;
        const DWORD newPointerLow = SetFilePointer(m_impl, 0, &hiword, FILE_END);
        if (newPointerLow == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
            throwFileError();
    }

    return true;
}

bool File::close()
{
    // sync();
    const BOOL res = CloseHandle(m_impl);
    m_impl = INVALID_HANDLE_VALUE;
    return res != 0;
}

int File::read(void* buffer, const uint32_t count) const
{
    if (!isOpen())
        return -1;

    DWORD bytesRead = 0;
    const BOOL res = ReadFile(m_impl, buffer, count, &bytesRead, nullptr);
    if (!res)
        return -1;

    m_pos += bytesRead;

    return static_cast<int>(bytesRead);
}

int File::write(const void* buffer, const uint32_t count)
{
    if (!isOpen())
        return -1;

    DWORD bytesWritten = 0;
    const BOOL res = WriteFile(m_impl, buffer, count, &bytesWritten, nullptr);
    if (!res)
    {
        throwFileError();
    }
    if (!res)
        return -1;

    m_pos += bytesWritten;

    return static_cast<int>(bytesWritten);
}

void File::sync() { FlushFileBuffers(m_impl); }

bool File::isOpen() const { return m_impl != INVALID_HANDLE_VALUE; }

bool File::size(int64_t* const fileSize) const
{
    DWORD highDw;
    const DWORD lowDw = GetFileSize(m_impl, &highDw);
    if ((lowDw == INVALID_FILE_SIZE) && (GetLastError() != NO_ERROR))
        return false;
    *fileSize = highDw;
    *fileSize <<= 32;
    *fileSize |= lowDw;
    return true;
}

int64_t File::seek(const int64_t offset, const SeekMethod whence) const
{
    if (!isOpen())
        return -1;

    DWORD moveMethod = FILE_BEGIN;
    switch (whence)
    {
    case SeekMethod::smBegin:
        moveMethod = FILE_BEGIN;
        break;
    case SeekMethod::smCurrent:
        moveMethod = FILE_CURRENT;
        break;
    case SeekMethod::smEnd:
        moveMethod = FILE_END;
        break;
    }

    const LONG distanceToMoveLow = static_cast<LONG>(offset);
    LONG distanceToMoveHigh = static_cast<LONG>(offset >> 32);

    const DWORD newPointerLow = SetFilePointer(m_impl, distanceToMoveLow, &distanceToMoveHigh, moveMethod);
    if (newPointerLow == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
        return -1;

    m_pos = static_cast<int64_t>(distanceToMoveHigh) << 32 | newPointerLow;

    return m_pos;
}

bool File::truncate(const uint64_t newFileSize) const
{
    const LONG distanceToMoveLow = static_cast<LONG>(newFileSize);
    LONG distanceToMoveHigh = static_cast<LONG>(newFileSize >> 32);
    const DWORD newPointerLow = SetFilePointer(m_impl, distanceToMoveLow, &distanceToMoveHigh, FILE_BEGIN);
    const DWORD errCode = GetLastError();
    if (newPointerLow == INVALID_SET_FILE_POINTER && errCode != NO_ERROR)
        throwFileError();

    return SetEndOfFile(m_impl) > 0;
}
