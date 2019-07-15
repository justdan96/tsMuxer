#ifndef __BUFFERED_RANDOM_ACCESS_READER_H
#define __BUFFERED_RANDOM_ACCESS_READER_H

#include "abstractreader.h"

namespace vodcore
{
    class BufferedRandomAccessReader
    {
        public:
            BufferedRandomAccessReader() {}
            virtual ~BufferedRandomAccessReader() {}
        
            virtual bool skipBlock(uint32_t readerID, uint32_t blockCnt) = 0;
            virtual bool gotoBlock(uint32_t readerID, uint32_t blockNum) = 0;
            virtual bool gotoByte(uint32_t readerID, uint64_t seekDist) = 0;
            virtual std::string getFileName () = 0;
    };
}

#endif
