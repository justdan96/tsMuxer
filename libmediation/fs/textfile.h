#ifndef LIBMEDIATION_TEXTFILE_H
#define LIBMEDIATION_TEXTFILE_H

#include "file.h"

class TextFile : public File
{
   public:
    TextFile() : File() {}
    TextFile(const char* fName, unsigned int oflag, unsigned int systemDependentFlags = 0)
        : File(fName, oflag, systemDependentFlags)
    {
    }

    bool readLine(std::string& line)
    {
        line = "";
        char ch;
        int readCnt = 0;
        do
        {
            readCnt = read(&ch, 1);
        } while (readCnt == 1 && (ch == '\n' || ch == '\r'));
        if (readCnt != 1)
            return false;
        if (ch != '\n' && ch != '\r')
            line += ch;
        while (1)
        {
            readCnt = read(&ch, 1);
            if (readCnt == 1 && ch != '\n' && ch != '\r')
                line += ch;
            else
                break;
        };
        return readCnt == 1 || !line.empty();
    }
    bool writeLine(const std::string& line)
    {
        if (write(line.c_str(), (uint32_t)line.size()) != line.size())
            return false;
        return write("\r\n", 2) == 2;
    }
};

#endif
