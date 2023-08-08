
#include "abstractDemuxer.h"

#include "subTrackFilter.h"

AbstractDemuxer::~AbstractDemuxer()
{
    for (const auto &m_pidFilter : m_pidFilters)
        delete m_pidFilter.second;
}
