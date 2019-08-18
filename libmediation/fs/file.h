#ifndef LIBMEDIATION_FILE_H
#define LIBMEDIATION_FILE_H

#include <stdexcept>
#include <fcntl.h>
#include "../types/types.h"

#ifdef WIN32
#pragma warning( disable : 4290 )
#endif

class AbstractStream
{
public:
    AbstractStream() {}
    virtual ~AbstractStream() {}

    static const unsigned int ofRead = 1;
    static const unsigned int ofWrite = 2;
    static const unsigned int ofAppend = 4;			// file can be exist
    static const unsigned int ofOpenExisting = 8;	// do not create file if absent
    static const unsigned int ofCreateNew = 16;		// create new file. Return error If file exist 
    static const unsigned int ofNoTruncate = 32;	// keep file data while opening

    virtual bool open(const char* fName, unsigned int oflag, unsigned int systemDependentFlags = 0 ) = 0;
    virtual bool close() = 0;
    virtual int64_t size() const = 0;
};

class AbstractOutputStream: public AbstractStream
{
public:
    AbstractOutputStream(): AbstractStream() {}
    virtual int write( const void* buffer, uint32_t count ) = 0;
    virtual void sync() = 0;
};


//! �����, ��������������� ��������� ��� ������ � �������
class  File: public AbstractOutputStream
{
public:
	enum SeekMethod
	{
		smBegin,
		smCurrent,
		smEnd
	};

	File();
	//! �����������
	/*!
		������ ������ � ��������� ����. ���� ���� ������� �� �������, �� �������� ����������.
		\param fName ��� �����
		\param oflag ������� ����� ���������� ������������ �����
		\param systemDependentFlags ��������-��������� ����� ��� �������� �����. 
			� ���������� ��� win32 �������� ����� ��������� ��������� � �������� dwFlagsAndAttributes ������� CreateFile,
			� ���������� ��� unix - �� ������ ��������(oflag) ������� open.
	*/
	File( 
		const char* fName, 
		unsigned int oflag,
		unsigned int systemDependentFlags = 0 ) /* throw ( std::runtime_error ) */;
	virtual ~File();

	//! �������� �����
	/*!
		���� ���� ��� ������, �� �� �����������
		\param fName ��� �����
		\param oflag ������� ����� ���������� ������������ �����
		\param systemDependentFlags ��������-��������� ����� ��� �������� �����. 
			� ���������� ��� win32 �������� ����� ��������� ��������� � �������� dwFlagsAndAttributes ������� CreateFile,
			� ���������� ��� unix - �� ������ ��������(oflag) ������� open.
		\return true, ���� ���� ������ ������, false � ��������� ������
	*/
	virtual bool open( 
		const char* fName, 
		unsigned int oflag,
		unsigned int systemDependentFlags = 0 ) override;
	//! ������� ����
	/*!
		\return true, ���� ���� ������, false � ������ ������
	*/
	virtual bool close() override;

	//! ������ �����
	/*!
		������ � ����� �������� count ���� �� �����. ���������� ����� ��������� �� ����� ����.
		\return ����� ��������� �� ����� ����, 0 ��� ������� ������ �� ����� �����, -1 � ������ ������ ������.
	*/
	int read( void* buffer, uint32_t count ) const;
	//! ������ � ����
	/*!
		\return ����� ����, ���������� � ����. -1 � ������ ������ (��������, ������������ ����� �� �����).
	*/
	virtual int write( const void* buffer, uint32_t count ) override;
	//!�������� ��������� �� ����
	/*!
		����� ��������� �� ����
	*/
	virtual void sync() override;

	//! ��������, ������ �� ����
	/*!
		\return true, ���� ���� ������, false � ��������� ������.
	*/
	bool isOpen() const;

	//!��������� ������� �����
	/*!
		\return ������� ������ �����
	*/
	bool size( uint64_t* const fileSize ) const;

    virtual int64_t size() const override {
        uint64_t result;
        if (size(&result))
            return (int64_t) result;
        else
            return -1;
    }

	//!����������� ������� �� �����
	/*!
		\param offset 
		\param whence
		\return ��������� ��������� ����� �����������, ��� uint64_t(-1) � ������ ������.
	*/
	uint64_t seek( int64_t offset, SeekMethod whence = smBegin);

	//!��������� ������� �����
	/*!
		��������� ��������� ������� ����� ������ ���� ������� �� ����������.
		\param newFileSize ����� ������ �����. ��� ������� ����� ��� ��������� ������ �����, ��� � ��������� ���.
	*/
	bool truncate( uint64_t newFileSize);
	
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

#endif	//LIBMEDIATION_FILE_H
