#include "id.h"

namespace Utils {
unsigned int idFromString(const QString& str)
{
    unsigned int result{0};
    if(!str.isEmpty()) {
        result = std::hash<QString>{}(str);
    }
    return result;
}

Id::Id(const QString& str)
    : m_id{idFromString(str)}
    , m_name{str}
{ }

Id::Id(const char* const str)
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

Id Id::append(const Id& id)
{
    QString name = m_name.append(id.name());
    return Id{name};
}

Id Id::append(const QString& str)
{
    QString name = m_name.append(str);
    return Id{name};
}

Id Id::append(const char* const str)
{
    return append(QString{str});
}

Id Id::append(int num)
{
    QString name = m_name.append(QString::number(num));
    return Id{name};
}

Id Id::append(quintptr addr)
{
    QString name = m_name.append(QString::number(addr));
    return Id{name};
}

bool Id::operator==(const Id& id) const
{
    return (m_id == id.m_id) && (m_name == id.m_name);
}

bool Id::operator!=(const Id& id) const
{
    return m_id != id.m_id;
}

size_t qHash(const Id& id) noexcept
{
    return static_cast<size_t>(id.m_id);
}
} // namespace Utils
