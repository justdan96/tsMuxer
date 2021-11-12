#ifndef libmediation_directory_priv_h
#define libmediation_directory_priv_h

#include <string>
#include <vector>

template <typename F, typename D>
void recurseDirectorySearch(F&& findFilesFn, D&& findDirsFn, const std::string& path, const std::string& mask,
                            std::vector<std::string>* const fileList)
{
    // finding files
    findFilesFn(path, mask, fileList, true);

    std::vector<std::string> dirList;
    findDirsFn(path, &dirList);

    for (auto&& dir : dirList)
    {
        recurseDirectorySearch(findFilesFn, findDirsFn, dir, mask, fileList);
    }
}

template <typename SkipParentDirFn, typename CreateParentDirFn>
bool preCreateDir(SkipParentDirFn&& skipParentFn, CreateParentDirFn&& createParentFn, char dirSeparator,
                  const std::string& dirName, bool createParentDirs)
{
    if (dirName.empty())
        return false;

    if (createParentDirs)
    {
        for (auto separatorPos = dirName.find_first_not_of(dirSeparator), dirEnd = std::string::npos;
             separatorPos != std::string::npos;
             dirEnd = separatorPos, separatorPos = dirName.find_first_not_of(dirSeparator, separatorPos))
        {
            separatorPos = dirName.find(dirSeparator, separatorPos);
            if (dirEnd != std::string::npos)
            {
                auto parentDir = dirName.substr(0, dirEnd);
                if (skipParentFn(parentDir))
                {
                    continue;
                }
                if (!createParentFn(parentDir))
                {
                    return false;
                }
            }
        }
    }

    return true;
}

#endif
