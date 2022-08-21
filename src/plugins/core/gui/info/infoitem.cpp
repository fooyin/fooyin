#include "infoitem.h"

InfoItem::InfoItem(Type type, QString title)
    : m_type(type)
    , m_title(std::move(title))
{ }

void InfoItem::appendChild(InfoItem* child)
{
    m_children.append(child);
}

InfoItem::~InfoItem()
{
    qDeleteAll(m_children);
}

InfoItem* InfoItem::child(int number)
{
    if(number < 0 || number >= m_children.size()) {
        return nullptr;
    }

    return m_children.at(number);
}

int InfoItem::childCount() const
{
    return static_cast<int>(m_children.size());
}

int InfoItem::columnCount()
{
    return 2;
}

QString InfoItem::data() const
{
    return m_title;
}

InfoItem::Type InfoItem::type()
{
    return m_type;
}

int InfoItem::row() const
{
    if(m_parent) {
        return static_cast<int>(m_parent->m_children.indexOf(const_cast<InfoItem*>(this))); // NOLINT
    }

    return 0;
}

InfoItem* InfoItem::parent() const
{
    return m_parent;
}
