#include <windows.h>

#include "../directory.h"
#include "../directory_priv.h"

#include "../file.h"

using namespace std;

char getDirSeparator() { return '\\'; }

string extractFileDir(const string& fileName)
{
    const size_t index = fileName.find_last_of('\\');
    if (index != string::npos)
        return fileName.substr(0, index + 1);

    return "";
}

bool fileExists(const string& fileName)
{
    File f;
    return f.open(fileName.c_str(), File::ofRead | File::ofOpenExisting);
}

uint64_t getFileSize(const std::string& fileName)
{
    File f;
    if (f.open(fileName.c_str(), File::ofRead | File::ofOpenExisting))
    {
        uint64_t rv;
        return f.size(&rv) ? rv : 0;
    }
    return 0;
}

bool createDir(const std::string& dirName, const bool createParentDirs)
{
    const bool ok = preCreateDir(
        [](auto&& parentDir) {
            return parentDir.empty() || parentDir[parentDir.size() - 1] == ':' || parentDir == "\\\\." ||
                   parentDir == R"(\\.\)" ||                                                        // UNC patch prefix
                   (strStartWith(parentDir, R"(\\.\)") && parentDir[parentDir.size() - 1] == '}');  // UNC patch prefix
        },
        [](auto&& parentDir) {
            if (CreateDirectory(toWide(parentDir).data(), nullptr) == 0)
            {
                if (GetLastError() != ERROR_ALREADY_EXISTS)
                    return false;
            }
            return true;
        },
        getDirSeparator(), dirName, createParentDirs);
    return ok ? CreateDirectory(toWide(dirName).data(), nullptr) != 0 : false;
}

bool deleteFile(const string& fileName)
{
    if (DeleteFile(toWide(fileName).data()))
    {
        return true;
    }
    DWORD err = GetLastError();
    return false;
}

bool findFiles(const string& path, const string& fileMask, vector<string>* fileList, const bool savePaths)
{
    WIN32_FIND_DATA fileData;  // Data structure describes the file found
    // Search handle returned by FindFirstFile

    const auto searchStr = toWide(path + '/' + fileMask);
    const HANDLE hSearch = FindFirstFile(searchStr.data(), &fileData);
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
    // Search handle returned by FindFirstFile

    const auto searchStr = toWide(path + "*");
    const HANDLE hSearch = FindFirstFile(searchStr.data(), &fileData);
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

bool findFilesRecursive(const string& path, const string& mask, vector<string>* const fileList)
{
    recurseDirectorySearch(findFiles, findDirs, path, mask, fileList);
    return true;
}
