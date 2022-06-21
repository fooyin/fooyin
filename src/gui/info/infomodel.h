#pragma once

#include <QAbstractItemModel>

class Track;
class InfoItem;

class InfoModel : public QAbstractItemModel
{
public:
    InfoModel(QObject* parent = nullptr);
    ~InfoModel() override;

    void setupModel();
    void reset(Track* track);

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex& child) const override;

private:
    std::unique_ptr<InfoItem> m_root;
    Track* m_currentTrack;
};
