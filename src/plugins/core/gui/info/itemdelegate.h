#pragma once

#include <QStyledItemDelegate>

class ItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ItemDelegate(QObject* parent = nullptr);
    ~ItemDelegate() override;

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    static void paintHeader(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index);
    static void paintEntry(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index);
};
