/***********************************************************************
*	File: file.cpp
*	Author: Andrey Kolesnikov
*	Date: 13 oct 2006
***********************************************************************/

#include "../file.h"
#include "../directory.h"

#include <sstream>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifdef MAC
#define O_LARGEFILE 0
#endif


void makeUnixOpenFlags( 
	unsigned int oflag, 
	int* const unixOflag )
{
	int sysFlags = 0;
	if( oflag & File::ofRead )
		sysFlags = O_RDONLY;
	if( oflag & File::ofWrite )
	{
		if( oflag & File::ofRead)
			sysFlags = O_RDWR | O_TRUNC;
		else
			sysFlags = O_WRONLY | O_TRUNC;
		if( !(oflag & File::ofOpenExisting) )
			sysFlags |= O_CREAT;
		if( oflag & File::ofNoTruncate)
		{
			sysFlags &= ~O_TRUNC;
		}
		if( oflag & File::ofAppend )
		{
			sysFlags |= O_APPEND;
			sysFlags &= ~O_TRUNC;
		}

	}
	if( oflag & File::ofCreateNew )
		sysFlags |= O_CREAT | O_EXCL;
	*unixOflag = sysFlags;
}

File::File():
	m_impl( (void*)0xffffffff ),
	m_name ( "" ),
    m_pos(0)
{
}

File::File ( const char* fName, unsigned int oflag, unsigned int systemDependentFlags ) /* throw ( std::runtime_error ) */:
    m_name ( fName ),
    m_pos(0)
{
	int sysFlags = 0;
	makeUnixOpenFlags( oflag, &sysFlags );
	m_impl = (void*)::open( fName, sysFlags | O_LARGEFILE | systemDependentFlags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );
	if( (long)m_impl == -1 )
	{
		std::ostringstream ss;
		ss<<"Error opening file "<<fName<<": "<<strerror(errno)<<"("<<errno<<")";
		throw std::runtime_error( ss.str() );
	}
}

File::~File()
{
	if( isOpen() )
		close();
}

bool File::open ( const char* fName, unsigned int oflag, unsigned int systemDependentFlags )
{
	m_name = fName;
	
	if( isOpen() )
		close();

	int sysFlags = 0;
	makeUnixOpenFlags( oflag, &sysFlags );
    createDir ( extractFileDir ( fName ), true );
	m_impl = (void*)::open( fName, sysFlags | O_LARGEFILE | systemDependentFlags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );

	return (long)m_impl != -1;
}

bool File::close()
{
	if( ::close( (long)m_impl ) == 0 )
	{
		m_impl = (void*)0xffffffff;
		return true;
	}

	return false;
}

int File::read( void* buffer, uint32_t count ) const
{
	if( !isOpen() )
		return -1;
    m_pos += count;
	return ::read( (long)m_impl, buffer, count );
}

int File::write( const void* buffer, uint32_t count )
{
	if( !isOpen() )
		return -1;
    m_pos += count;
	return ::write( (long)m_impl, buffer, count );
}

bool File::isOpen() const
{
	return m_impl != (void*)0xffffffff;
}

bool File::size( uint64_t* const fileSize ) const
{
    bool res = false;

    struct stat64 buf;

    if( isOpen() &&  ( fstat64( (long)m_impl, &buf ) == 0 ) )
    {
    	*fileSize = buf.st_size;
    	res = true;
    }

	return res;
}

uint64_t File::seek( int64_t offset, SeekMethod whence )
{
	if( !isOpen() )
		return (uint64_t)-1;

	int sWhence = 0;
	switch( whence )
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
#ifdef MAC  
	return lseek( (long)m_impl, offset, sWhence );
#else
	return lseek64( (long)m_impl, offset, sWhence );
#endif	
}

bool File::truncate( uint64_t newFileSize )
{
#ifdef MAC  
	return ftruncate( (long)m_impl, newFileSize ) == 0;
#else
	return ftruncate64( (long)m_impl, newFileSize ) == 0;
#endif	
}

void File::sync()
{
	::sync();
}

