#include "id.h"

#include <QHash>

namespace {
unsigned int idFromString(const QString& str)
{
    unsigned int result{0};
    if(!str.isEmpty()) {
        result = std::hash<QString>{}(str);
    }
    return result;
}
} // namespace

namespace Util {
Id::Id(const QString& str)
    : m_id{idFromString(str)}
    , m_name{str}
{ }

bool Id::isValid() const
{
    return (m_id > 0 && !m_name.isNull());
}

unsigned int Id::id() const
{
    return m_id;
}

QString Id::name() const
{
    return m_name;
}

bool Id::operator==(const Id& id) const
{
    return m_id == id.m_id;
}

bool Id::operator!=(const Id& id) const
{
    return m_id != id.m_id;
}

size_t qHash(const Id& id) noexcept
{
    return static_cast<size_t>(id.m_id);
}
} // namespace Util
