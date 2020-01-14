#ifndef directory_h
#define directory_h

#include <string>

#include "types/types.h"

bool createDir(const std::string& dirName, bool createParentDirs = false);

std::string extractFileDir(const std::string& fileName);

char getDirSeparator();

bool fileExists(const std::string& fileName);

uint64_t getFileSize(const std::string& fileName);

/** remove file. cerr contains error code */
bool deleteFile(const std::string& fileName);

bool findFilesRecursive(const std::string& path, const std::string& mask, std::vector<std::string>* const fileList);

#endif  // directory_h
