
#include "directory.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sstream>

#ifdef WIN32
#include <windows.h>
#else
#include <stddef.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <memory.h>
#include <stdlib.h>
#endif


using namespace std;

#ifdef SOLARIS
typedef struct mydirent 
{
	ino_t           d_ino;          /* "inode number" of entry */
	off_t           d_off;          /* offset of disk directory entry */
	unsigned short  d_reclen;       /* length of this record */
	char            d_name[PATH_MAX];      /* name of file */
} mydirent_t;
#endif

char getDirSeparator()
{
#ifdef WIN32
	return '\\';
#else
	return '/';
#endif
}

string extractFileDir( const string& fileName )
{
	size_t index = fileName.find_last_of('/');
	if( index != string::npos )
		return fileName.substr( 0, index+1 );
#ifdef WIN32
	index = fileName.find_last_of('\\');
	if( index != string::npos )
		return fileName.substr( 0, index+1 );
#endif

	return "";
}

bool fileExists( const string& fileName )
{
	bool fileExists = false;
#ifdef WIN32
	struct _stat64 buf;
	fileExists = _stat64( fileName.c_str(), &buf ) == 0;
#else	// LINUX
	struct stat64 buf;
	fileExists = stat64( fileName.c_str(), &buf ) == 0;
#endif

	return fileExists;
}

uint64_t getFileSize ( const std::string& fileName )
{
    bool res = false;
#ifdef WIN32
    struct _stat64 fileStat;
    res = _stat64( fileName.c_str(), &fileStat ) == 0;
#else	// LINUX
    struct stat64 fileStat;
    res = stat64( fileName.c_str(), &fileStat ) == 0;
#endif

    return res ? ( uint64_t )fileStat.st_size : 0;
}

bool createDir( 
	const std::string& dirName,
	bool createParentDirs ) 
{
	if( dirName.empty() )
		return false;

	if( createParentDirs )
	{
		for( string::size_type 
			separatorPos = dirName.find_first_not_of( getDirSeparator() ), dirEnd = string::npos;
			separatorPos != string::npos;
			dirEnd = separatorPos, separatorPos = dirName.find_first_not_of( getDirSeparator(), separatorPos ) )
		{
			separatorPos = dirName.find( getDirSeparator(), separatorPos );
			if( dirEnd != string::npos )
			{
				string parentDir = dirName.substr( 0, dirEnd );
#if defined(LINUX) || defined(SOLARIS) || defined(MAC)
				if( mkdir( parentDir.c_str(), S_IREAD | S_IWRITE | S_IEXEC ) == -1 )
				{
					if( errno != EEXIST )
						return false;
				}
#elif defined(WIN32)
				if (parentDir.size() == 0 || parentDir[parentDir.size()-1] == ':' || 
                    parentDir == string("\\\\.")  || parentDir == string("\\\\.\\") || // UNC patch prefix
                    (strStartWith(parentDir, "\\\\.\\") && parentDir[parentDir.size()-1] == '}')) // UNC patch prefix
					continue;
				if( CreateDirectory( parentDir.c_str(), 0 ) == 0 )
				{
					if( GetLastError() != ERROR_ALREADY_EXISTS )
						return false;
				}
#endif
			}
		}
	}

#if defined(LINUX) || defined(SOLARIS) || defined(MAC)
	return mkdir( dirName.c_str(),  S_IREAD | S_IWRITE | S_IEXEC ) == 0;
#elif defined(WIN32)
	return CreateDirectory( dirName.c_str(), 0 ) != 0;
#endif
}

bool deleteFile( const string& fileName )
{
#if defined(LINUX) || defined(SOLARIS) || defined(MAC)
	return unlink( fileName.c_str() ) == 0;
#else
	if( DeleteFile(fileName.c_str()) )
	{
		return true;
	}
	else
	{
		DWORD err = GetLastError();
		return false;
	}

	return DeleteFile( fileName.c_str() ) != 0;
#endif
}


//-----------------------------------------------------------------------------
#if defined(WIN32) 
/** windows implementation */
//-----------------------------------------------------------------------------
#include <windows.h>

bool findFiles( 
               const string& path, 
               const string& fileMask, 
               vector< string>* fileList, 
               bool savePaths )
{
    WIN32_FIND_DATA fileData;   // Data structure describes the file found
    HANDLE hSearch;             // Search handle returned by FindFirstFile

    hSearch = FindFirstFile( TEXT( string( path + '/' + fileMask ).c_str() ), &fileData);
    if( hSearch == INVALID_HANDLE_VALUE )
        return false;

    do
    {
        if( !(fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
        {
            string fileName( fileData.cFileName );

            fileList->push_back( savePaths ? (path + '/' + fileName) : fileName );
        }
    }
    while ( FindNextFile (hSearch, &fileData) );

    FindClose( hSearch );

    return true;
}

bool findDirs( 
              const string& path, 
              vector< string>* dirsList )
{
    WIN32_FIND_DATA fileData;   // Data structure describes the file found
    HANDLE hSearch;             // Search handle returned by FindFirstFile

    hSearch = FindFirstFile( TEXT( string( path + "*" ).c_str() ), &fileData);
    if( hSearch == INVALID_HANDLE_VALUE ) return false;

    do {
        if( !(fileData.dwFileAttributes ^ FILE_ATTRIBUTE_DIRECTORY ) ) {
            string dirName( fileData.cFileName );

            if( "." != dirName && ".." != dirName )
                dirsList->push_back( path + dirName + "/" );
        }

    } while ( FindNextFile (hSearch, &fileData) );

    FindClose( hSearch );

    return true;
}
#elif defined(LINUX) || defined(MAC)
#include <fnmatch.h>

bool findFiles( 
               const string& path, 
               const string& fileMask, 
               vector< string>* fileList, 
               bool savePaths )
{
    dirent **namelist;

    int n = scandir( path.c_str(), &namelist, 0, 0 ); //alphasort);

    if( n < 0 )
    {
        return false;
    }
    else
    {
        while( n-- )
        {
            if( namelist[ n ]->d_type == DT_REG )
            {
                string fileName( namelist[ n ]->d_name );

                if( 0 == fnmatch( fileMask.c_str(), fileName.c_str(), 0 ) )
                    fileList->push_back( savePaths ? path + fileName : fileName );

            }
            free(namelist[n]);
        }
        free(namelist);
    }

    return true;
}	

bool findDirs( 
              const string& path, 
              vector< string>* dirsList )
{
    dirent **namelist;

    int n = scandir( path.c_str(), &namelist, 0, 0 ); //alphasort);

    if( n < 0 ) 
    {
        return false;
    }
    else
    {
        while( n-- )
        {
            if( namelist[ n ]->d_type == DT_DIR )
            {
                string dirName( namelist[ n ]->d_name );

                //сохраняем только нормальные
                if( "." != dirName && ".." != dirName )
                    dirsList->push_back( path + dirName + "/" );

            }
            free(namelist[n]);
        }
        free(namelist);
    }

    return true;
}
#endif
//-----------------------------------------------------------------------------


bool findFilesRecursive(const string& path, const string& mask, vector<string>* const fileList )
{
    //finding files
    findFiles( path, mask, fileList, true );

    vector<string> dirList;
    findDirs( path, &dirList );

    for( unsigned int d_idx = 0; d_idx != dirList.size(); d_idx++ )
    findFilesRecursive( dirList[ d_idx ], mask, fileList );

    return true;
}
