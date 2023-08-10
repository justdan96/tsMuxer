#if !defined(_WIN32)

#include "../directory.h"
#include "../directory_priv.h"

#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

char getDirSeparator() { return '/'; }

string extractFileDir(const string& fileName)
{
    size_t index = fileName.find_last_of('/');
    if (index != string::npos)
        return fileName.substr(0, index + 1);
    return "";
}

bool fileExists(const string& fileName)
{
    struct stat buf;
    return stat(fileName.c_str(), &buf) == 0;
}

uint64_t getFileSize(const std::string& fileName)
{
    struct stat fileStat;
    auto res = stat(fileName.c_str(), &fileStat) == 0;
    return res ? static_cast<uint64_t>(fileStat.st_size) : 0;
}

bool createDir(const std::string& dirName, bool createParentDirs)
{
    auto ok = preCreateDir([](auto) { return false; },
                           [](auto&& parentDir) {
                               if (mkdir(parentDir.c_str(), S_IREAD | S_IWRITE | S_IEXEC) == -1)
                               {
                                   if (errno != EEXIST)
                                       return false;
                               }
                               return true;
                           },
                           getDirSeparator(), dirName, createParentDirs);

    return ok ? mkdir(dirName.c_str(), S_IREAD | S_IWRITE | S_IEXEC) == 0 : false;
}

bool deleteFile(const string& fileName) { return unlink(fileName.c_str()) == 0; }

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

bool findFilesRecursive(const string& path, const string& mask, vector<string>* const fileList)
{
    recurseDirectorySearch(findFiles, findDirs, path, mask, fileList);
    return true;
}

#endif