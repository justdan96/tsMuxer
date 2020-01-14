
#include "directory.h"

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <memory.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#endif

using namespace std;

char getDirSeparator()
{
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

string extractFileDir(const string& fileName)
{
    size_t index = fileName.find_last_of('/');
    if (index != string::npos)
        return fileName.substr(0, index + 1);
#ifdef _WIN32
    index = fileName.find_last_of('\\');
    if (index != string::npos)
        return fileName.substr(0, index + 1);
#endif

    return "";
}

bool fileExists(const string& fileName)
{
    bool fileExists = false;
#ifdef _WIN32
    struct _stat64 buf;
    fileExists = _stat64(fileName.c_str(), &buf) == 0;
#else
    struct stat64 buf;
    fileExists = stat64(fileName.c_str(), &buf) == 0;
#endif

    return fileExists;
}

uint64_t getFileSize(const std::string& fileName)
{
    bool res = false;
#ifdef _WIN32
    struct _stat64 fileStat;
    res = _stat64(fileName.c_str(), &fileStat) == 0;
#else
    struct stat64 fileStat;
    res = stat64(fileName.c_str(), &fileStat) == 0;
#endif

    return res ? (uint64_t)fileStat.st_size : 0;
}

bool createDir(const std::string& dirName, bool createParentDirs)
{
    if (dirName.empty())
        return false;

    if (createParentDirs)
    {
        for (string::size_type separatorPos = dirName.find_first_not_of(getDirSeparator()), dirEnd = string::npos;
             separatorPos != string::npos;
             dirEnd = separatorPos, separatorPos = dirName.find_first_not_of(getDirSeparator(), separatorPos))
        {
            separatorPos = dirName.find(getDirSeparator(), separatorPos);
            if (dirEnd != string::npos)
            {
                string parentDir = dirName.substr(0, dirEnd);
#if __linux__ == 1 || (defined(__APPLE__) && defined(__MACH__))
                if (mkdir(parentDir.c_str(), S_IREAD | S_IWRITE | S_IEXEC) == -1)
                {
                    if (errno != EEXIST)
                        return false;
                }
#elif defined(_WIN32)
                if (parentDir.size() == 0 || parentDir[parentDir.size() - 1] == ':' || parentDir == string("\\\\.") ||
                    parentDir == string("\\\\.\\") ||                                                // UNC patch prefix
                    (strStartWith(parentDir, "\\\\.\\") && parentDir[parentDir.size() - 1] == '}'))  // UNC patch prefix
                    continue;
                if (CreateDirectory(parentDir.c_str(), 0) == 0)
                {
                    if (GetLastError() != ERROR_ALREADY_EXISTS)
                        return false;
                }
#endif
            }
        }
    }

#if __linux__ == 1 || (defined(__APPLE__) && defined(__MACH__))
    return mkdir(dirName.c_str(), S_IREAD | S_IWRITE | S_IEXEC) == 0;
#elif defined(_WIN32)
    return CreateDirectory(dirName.c_str(), 0) != 0;
#endif
}

bool deleteFile(const string& fileName)
{
#if __linux__ == 1 || (defined(__APPLE__) && defined(__MACH__))
    return unlink(fileName.c_str()) == 0;
#else
    if (DeleteFile(fileName.c_str()))
    {
        return true;
    }
    else
    {
        DWORD err = GetLastError();
        return false;
    }

    return DeleteFile(fileName.c_str()) != 0;
#endif
}

//-----------------------------------------------------------------------------
#if defined(_WIN32)
/** windows implementation */
//-----------------------------------------------------------------------------
#include <windows.h>

bool findFiles(const string& path, const string& fileMask, vector<string>* fileList, bool savePaths)
{
    WIN32_FIND_DATA fileData;  // Data structure describes the file found
    HANDLE hSearch;            // Search handle returned by FindFirstFile

    hSearch = FindFirstFile(TEXT(string(path + '/' + fileMask).c_str()), &fileData);
    if (hSearch == INVALID_HANDLE_VALUE)
        return false;

    do
    {
        if (!(fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            string fileName(fileData.cFileName);

            fileList->push_back(savePaths ? (path + '/' + fileName) : fileName);
        }
    } while (FindNextFile(hSearch, &fileData));

    FindClose(hSearch);

    return true;
}

bool findDirs(const string& path, vector<string>* dirsList)
{
    WIN32_FIND_DATA fileData;  // Data structure describes the file found
    HANDLE hSearch;            // Search handle returned by FindFirstFile

    hSearch = FindFirstFile(TEXT(string(path + "*").c_str()), &fileData);
    if (hSearch == INVALID_HANDLE_VALUE)
        return false;

    do
    {
        if (!(fileData.dwFileAttributes ^ FILE_ATTRIBUTE_DIRECTORY))
        {
            string dirName(fileData.cFileName);

            if ("." != dirName && ".." != dirName)
                dirsList->push_back(path + dirName + "/");
        }

    } while (FindNextFile(hSearch, &fileData));

    FindClose(hSearch);

    return true;
}
#elif __linux__ == 1 || (defined(__APPLE__) && defined(__MACH__))
#include <fnmatch.h>

bool findFiles(const string& path, const string& fileMask, vector<string>* fileList, bool savePaths)
{
    dirent** namelist;

    int n = scandir(path.c_str(), &namelist, 0, 0);  // alphasort);

    if (n < 0)
    {
        return false;
    }
    else
    {
        while (n--)
        {
            if (namelist[n]->d_type == DT_REG)
            {
                string fileName(namelist[n]->d_name);

                if (0 == fnmatch(fileMask.c_str(), fileName.c_str(), 0))
                    fileList->push_back(savePaths ? path + fileName : fileName);
            }
            free(namelist[n]);
        }
        free(namelist);
    }

    return true;
}

bool findDirs(const string& path, vector<string>* dirsList)
{
    dirent** namelist;

    int n = scandir(path.c_str(), &namelist, 0, 0);  // alphasort);

    if (n < 0)
    {
        return false;
    }
    else
    {
        while (n--)
        {
            if (namelist[n]->d_type == DT_DIR)
            {
                string dirName(namelist[n]->d_name);

                // we save only normal ones
                if ("." != dirName && ".." != dirName)
                    dirsList->push_back(path + dirName + "/");
            }
            free(namelist[n]);
        }
        free(namelist);
    }

    return true;
}
#endif
//-----------------------------------------------------------------------------

bool findFilesRecursive(const string& path, const string& mask, vector<string>* const fileList)
{
    // finding files
    findFiles(path, mask, fileList, true);

    vector<string> dirList;
    findDirs(path, &dirList);

    for (unsigned int d_idx = 0; d_idx != dirList.size(); d_idx++) findFilesRecursive(dirList[d_idx], mask, fileList);

    return true;
}
