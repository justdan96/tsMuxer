#include <io.h>
#include <windows.h>

#include <sstream>

#include "../directory.h"
#include "../file.h"

void throwFileError()
{
    char msgBuf[12 * 1024];
    memset(msgBuf, 0, sizeof(msgBuf));
    DWORD dw = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&msgBuf,
                  sizeof(msgBuf) >> 1, NULL);
    throw std::runtime_error(msgBuf);
}

void makeWin32OpenFlags(unsigned int oflag, DWORD* const dwDesiredAccess, DWORD* const dwCreationDisposition,
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

File::File() : AbstractOutputStream(), m_impl(INVALID_HANDLE_VALUE), m_name(""), m_pos(0) {}

File::File(const char* fName, unsigned int oflag, unsigned int systemDependentFlags) /* throw ( std::runtime_error ) */
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
    m_impl = CreateFile(toWide(fName).data(), dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition,
                        systemDependentFlags, NULL);
    if (m_impl == INVALID_HANDLE_VALUE)
    {
        throwFileError();
    }
    else
    {
        if (oflag & File::ofAppend)
        {
            long hiword = 0;
            DWORD newPointerLow = SetFilePointer(m_impl, 0, &hiword, FILE_END);
            if (newPointerLow == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
                throwFileError();
        }
    }
}

File::~File()
{
    if (isOpen())
        close();
}

bool File::open(const char* fName, unsigned int oflag, unsigned int systemDependentFlags)
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

    if ((oflag & File::ofOpenExisting) == 0)
    {
        createDir(extractFileDir(fName), true);
    }
    m_impl = CreateFile(toWide(fName).data(), dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition,
                        systemDependentFlags, NULL);
    if (m_impl == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    else
    {
        if (oflag & File::ofAppend)
        {
            long hiword = 0;
            DWORD newPointerLow = SetFilePointer(m_impl, 0, &hiword, FILE_END);
            if (newPointerLow == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
                throwFileError();
        }
    }

    return true;
}

bool File::close()
{
    // sync();
    BOOL res = CloseHandle(m_impl);
    m_impl = INVALID_HANDLE_VALUE;
    return res != 0;
}

int File::read(void* buffer, uint32_t count) const
{
    if (!isOpen())
        return -1;

    DWORD bytesRead = 0;
    BOOL res = ReadFile(m_impl, buffer, count, &bytesRead, NULL);
    if (!res)
        return -1;

    m_pos += bytesRead;

    return (int)bytesRead;
}

int File::write(const void* buffer, uint32_t count)
{
    if (!isOpen())
        return -1;

    DWORD bytesWritten = 0;
    BOOL res = WriteFile(m_impl, buffer, count, &bytesWritten, NULL);
    if (!res)
    {
        throwFileError();
    }
    if (!res)
        return -1;

    m_pos += bytesWritten;

    return (int)bytesWritten;
}

void File::sync() { FlushFileBuffers(m_impl); }

bool File::isOpen() const { return m_impl != INVALID_HANDLE_VALUE; }

bool File::size(uint64_t* const fileSize) const
{
    DWORD highDw;
    DWORD lowDw = GetFileSize(m_impl, &highDw);
    if ((lowDw == INVALID_FILE_SIZE) && (GetLastError() != NO_ERROR))
        return false;
    *fileSize = highDw;
    *fileSize <<= 32;
    *fileSize |= lowDw;
    return true;
}

uint64_t File::seek(int64_t offset, SeekMethod whence)
{
    if (!isOpen())
        return (uint64_t)-1;

    DWORD moveMethod = 0;
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

    LONG distanceToMoveLow = (uint32_t)(offset & 0xffffffff);
    LONG distanceToMoveHigh = (uint32_t)((offset & 0xffffffff00000000ull) >> 32);

    DWORD newPointerLow = SetFilePointer(m_impl, distanceToMoveLow, &distanceToMoveHigh, moveMethod);
    if (newPointerLow == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
        return (uint64_t)-1;

    m_pos = newPointerLow | ((uint64_t)distanceToMoveHigh << 32);

    return m_pos;
}

bool File::truncate(uint64_t newFileSize)
{
    LONG distanceToMoveLow = (uint32_t)(newFileSize & 0xffffffff);
    LONG distanceToMoveHigh = (uint32_t)((newFileSize & 0xffffffff00000000ull) >> 32);
    DWORD newPointerLow = SetFilePointer(m_impl, distanceToMoveLow, &distanceToMoveHigh, FILE_BEGIN);
    int errCode = GetLastError();
    if ((newPointerLow == INVALID_SET_FILE_POINTER) && (errCode != NO_ERROR))
        // return false;
        throwFileError();

    return SetEndOfFile(m_impl) > 0;
}
