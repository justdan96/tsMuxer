
#include "directory.h"

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <sstream>

#include "file.h"

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
#ifdef _WIN32
    File f;
    return f.open(fileName.c_str(), File::ofRead | File::ofOpenExisting);
#else
    struct stat64 buf;
    return stat64(fileName.c_str(), &buf) == 0;
#endif
}

uint64_t getFileSize(const std::string& fileName)
{
#ifdef _WIN32
    File f;
    if (f.open(fileName.c_str(), File::ofRead | File::ofOpenExisting))
    {
        uint64_t rv;
        return f.size(&rv) ? rv : 0;
    }
    else
    {
        return 0;
    }
#else
    struct stat64 fileStat;
    auto res = stat64(fileName.c_str(), &fileStat) == 0;
    return res ? static_cast<uint64_t>(fileStat.st_size) : 0;
#endif
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
                if (CreateDirectory(toWide(parentDir).data(), 0) == 0)
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
    return CreateDirectory(toWide(dirName).data(), 0) != 0;
#endif
}

bool deleteFile(const string& fileName)
{
#if __linux__ == 1 || (defined(__APPLE__) && defined(__MACH__))
    return unlink(fileName.c_str()) == 0;
#else
    if (DeleteFile(toWide(fileName).data()))
    {
        return true;
    }
    else
    {
        DWORD err = GetLastError();
        return false;
    }

    return DeleteFile(toWide(fileName).data()) != 0;
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

    auto searchStr = toWide(path + '/' + fileMask);
    hSearch = FindFirstFile(searchStr.data(), &fileData);
    if (hSearch == INVALID_HANDLE_VALUE)
        return false;

    do
    {
        if (!(fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            auto fileName = toUtf8(fileData.cFileName);
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

    auto searchStr = toWide(path + "*");
    hSearch = FindFirstFile(searchStr.data(), &fileData);
    if (hSearch == INVALID_HANDLE_VALUE)
        return false;

    do
    {
        if (!(fileData.dwFileAttributes ^ FILE_ATTRIBUTE_DIRECTORY))
        {
            auto dirName = toUtf8(fileData.cFileName);

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
