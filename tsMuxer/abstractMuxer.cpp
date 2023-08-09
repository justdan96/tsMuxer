
#include "abstractMuxer.h"
#include "muxerManager.h"

using namespace std;

AbstractMuxer::AbstractMuxer(MuxerManager* owner) : m_owner(owner)
{
    m_interliaveBlockSize = 0;
    m_sectorSize = 0;
    m_fileFactory = nullptr;
}

void AbstractMuxer::setBlockMuxMode(int blockSize, int sectorSize)
{
    m_interliaveBlockSize = blockSize;
    m_sectorSize = sectorSize;
}

void AbstractMuxer::setFileName(const std::string& fileName, FileFactory* fileFactory)
{
    m_origFileName = fileName;
    m_fileFactory = fileFactory;
}
