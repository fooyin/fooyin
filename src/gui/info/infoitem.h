#pragma once

#include <QList>

class InfoItem
{
public:
    enum class Type : qint8
    {
        Header = 0,
        Entry,
    };

    explicit InfoItem(Type type = Type::Header, QString title = {});
    ~InfoItem();

    void appendChild(InfoItem* child);

    [[nodiscard]] InfoItem* child(int number);
    [[nodiscard]] int childCount() const;
    [[nodiscard]] static int columnCount();
    [[nodiscard]] QString data() const;
    [[nodiscard]] Type type();
    [[nodiscard]] int row() const;
    [[nodiscard]] InfoItem* parent() const;

private:
    QList<InfoItem*> m_children;
    Type m_type;
    QString m_title;
    InfoItem* m_parent;
};
