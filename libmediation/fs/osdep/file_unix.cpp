/***********************************************************************
 *	File: file.cpp
 *	Author: Andrey Kolesnikov
 *	Date: 13 oct 2006
 ***********************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sstream>

#include "../directory.h"
#include "../file.h"

namespace
{
int to_fd(void* impl) { return static_cast<int>(reinterpret_cast<std::intptr_t>(impl)); }
void* from_fd(int fd) { return reinterpret_cast<void*>(static_cast<std::intptr_t>(fd)); }

int makeUnixOpenFlags(unsigned int oflag)
{
    int sysFlags = 0;
    if (oflag & File::ofRead)
        sysFlags = O_RDONLY;
    if (oflag & File::ofWrite)
    {
        if (oflag & File::ofRead)
            sysFlags = O_RDWR | O_TRUNC;
        else
            sysFlags = O_WRONLY | O_TRUNC;
        if (!(oflag & File::ofOpenExisting))
            sysFlags |= O_CREAT;
        if (oflag & File::ofNoTruncate)
        {
            sysFlags &= ~O_TRUNC;
        }
        if (oflag & File::ofAppend)
        {
            sysFlags |= O_APPEND;
            sysFlags &= ~O_TRUNC;
        }
    }
    if (oflag & File::ofCreateNew)
        sysFlags |= O_CREAT | O_EXCL;
    return sysFlags;
}
}  // namespace

File::File() : m_impl(from_fd(-1)), m_pos(0) {}

File::File(const char* fName, unsigned int oflag, unsigned int systemDependentFlags) : m_name(fName), m_pos(0)
{
    int sysFlags = makeUnixOpenFlags(oflag);
    auto fd = ::open(fName, sysFlags | systemDependentFlags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd == -1)
    {
        std::ostringstream ss;
        ss << "Error opening file " << fName << ": " << strerror(errno) << "(" << errno << ")";
        throw std::runtime_error(ss.str());
    }
    m_impl = from_fd(fd);
}

File::~File()
{
    if (isOpen())
        close();
}

bool File::open(const char* fName, unsigned int oflag, unsigned int systemDependentFlags)
{
    m_name = fName;

    if (isOpen())
        close();

    int sysFlags = makeUnixOpenFlags(oflag);
    createDir(extractFileDir(fName), true);
    auto fd = ::open(fName, sysFlags | systemDependentFlags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    m_impl = from_fd(fd);
    return fd != -1;
}

bool File::close()
{
    if (::close(to_fd(m_impl)) == 0)
    {
        m_impl = from_fd(-1);
        return true;
    }

    return false;
}

int File::read(void* buffer, uint32_t count) const
{
    if (!isOpen())
        return -1;
    m_pos += count;
    return ::read(to_fd(m_impl), buffer, count);
}

int File::write(const void* buffer, uint32_t count)
{
    if (!isOpen())
        return -1;
    m_pos += count;
    return ::write(to_fd(m_impl), buffer, count);
}

bool File::isOpen() const { return to_fd(m_impl) != -1; }

bool File::size(uint64_t* const fileSize) const
{
    bool res = false;

    struct stat buf;

    if (isOpen() && (fstat(to_fd(m_impl), &buf) == 0))
    {
        *fileSize = buf.st_size;
        res = true;
    }

    return res;
}

uint64_t File::seek(int64_t offset, SeekMethod whence)
{
    if (!isOpen())
        return UINT64_C(-1);

    int sWhence = 0;
    switch (whence)
    {
    case smBegin:
        sWhence = SEEK_SET;
        break;
    case smCurrent:
        sWhence = SEEK_CUR;
        break;
    case smEnd:
        sWhence = SEEK_END;
        break;
    }
    m_pos = offset;
    return lseek(to_fd(m_impl), offset, sWhence);
}

bool File::truncate(uint64_t newFileSize) { return ftruncate(to_fd(m_impl), newFileSize) == 0; }

void File::sync() { ::sync(); }
