
#include "abstractDemuxer.h"

#include "subTrackFilter.h"

AbstractDemuxer::~AbstractDemuxer()
{
    for (auto itr = m_pidFilters.begin(); itr != m_pidFilters.end(); ++itr) delete itr->second;
}
