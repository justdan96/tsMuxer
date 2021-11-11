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

#endif
