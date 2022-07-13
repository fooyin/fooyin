#include "filtermanager.h"

FilterManager::FilterManager() = default;

int FilterManager::registerFilter(Filters::FilterType type)
{
    m_filters.append(type);
    return static_cast<int>(m_filters.size() - 1);
}

FilterManager::~FilterManager() = default;
