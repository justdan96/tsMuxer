#ifndef LIBMEDIATION_FILE_H
#define LIBMEDIATION_FILE_H

#include <fcntl.h>

#include <stdexcept>

#include "../types/types.h"

#ifdef _WIN32
#pragma warning(disable : 4290)
#endif

class AbstractStream
{
   public:
    AbstractStream() {}
    virtual ~AbstractStream() {}

    static const unsigned int ofRead = 1;
    static const unsigned int ofWrite = 2;
    static const unsigned int ofAppend = 4;        // file can be exist
    static const unsigned int ofOpenExisting = 8;  // do not create file if absent
    static const unsigned int ofCreateNew = 16;    // create new file. Return error If file exist
    static const unsigned int ofNoTruncate = 32;   // keep file data while opening

    virtual bool open(const char* fName, unsigned int oflag, unsigned int systemDependentFlags = 0) = 0;
    virtual bool close() = 0;
    virtual int64_t size() const = 0;
};

class AbstractOutputStream : public AbstractStream
{
   public:
    AbstractOutputStream() : AbstractStream() {}
    virtual int write(const void* buffer, uint32_t count) = 0;
    virtual void sync() = 0;
};

//! A class which represents an interface for working with files.
class File : public AbstractOutputStream
{
   public:
    enum SeekMethod
    {
        smBegin,
        smCurrent,
        smEnd
    };

    File();
    //! Constructor
    /*!
            Creates the object and opens the file. If opening was unsuccessful, throws an exception.
            \param fName Name of the file.
            \param oflag A bitmask of the opened file's parameters.
            \param systemDependentFlags System-dependent flags for opening the file.
                    In the win32 implementation, this is the dwFlagsAndAttributes parameter to the CreateFile function,
                    In the unix implementation, this is the second parameter to the open function.
    */
    File(const char* fName, unsigned int oflag,
         unsigned int systemDependentFlags = 0) /* throw ( std::runtime_error ) */;
    ~File() override;

    //! Open the file
    /*!
            If a file is already open, it will be closed.
    \param oflag A bitmask of the opened file's parameters.
            \param systemDependentFlags System-dependent flags for opening the file.
                    In the win32 implementation, this is the dwFlagsAndAttributes parameter to the CreateFile function,
                    In the unix implementation, this is the second parameter to the open function.
            \return true if the file was opened successfully, false otherwise
    */
    bool open(const char* fName, unsigned int oflag, unsigned int systemDependentFlags = 0) override;
    //! Close the file
    /*!
            \return true, if the file was closed, false in case of an error
    */
    bool close() override;

    //! Read the file
    /*!
            Reads at most count bytes from the file. Returns the number of bytes actually read.
            \return Number of bytes actually read, 0 if the end of the file was reached, -1 in case of an error.
    */
    int read(void* buffer, uint32_t count) const;
    //! Write to the file
    /*!
            \return The number of bytes written into the file. -1 in case of an error (for example, if the disk is
       full).
    */
    int write(const void* buffer, uint32_t count) override;
    //! Write changes into the disk.
    /*!
            Write changes into the disk
    */
    void sync() override;

    //! Check if the file is open.
    /*!
            \return true if the file is open, false otherwise.
    */
    bool isOpen() const;

    //! Get the size of the file
    /*!
            \return Current size of the file
    */
    bool size(uint64_t* const fileSize) const;

    int64_t size() const override
    {
        uint64_t result;
        if (size(&result))
            return (int64_t)result;
        else
            return -1;
    }

    //! Relocate the file cursor
    /*!
            \param offset
            \param whence
            \return Location of the cursor after relocating it, or uint64_t(-1) in case of an error.
    */
    uint64_t seek(int64_t offset, SeekMethod whence = smBegin);

    //! Change the size of the file
    /*!
            The location of the file cursor after calling this function is undefined.
            \param newFileSize New size of the file. This function can both enlarge, as well as reduce the file size.
    */
    bool truncate(uint64_t newFileSize);

    std::string getName() { return m_name; }

    uint64_t pos() const { return m_pos; }

   private:
    void* m_impl;

    friend class MemoryMappedFile;

    std::string m_name;
    mutable uint64_t m_pos;
};

class FileFactory
{
   public:
    virtual ~FileFactory() {}
    virtual AbstractOutputStream* createFile() = 0;
    virtual bool isVirtualFS() const = 0;
};

#endif  // LIBMEDIATION_FILE_H
