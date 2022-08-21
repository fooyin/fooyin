#pragma once

#include <QTreeView>

class InfoView : public QTreeView
{
public:
    explicit InfoView(QWidget* parent = nullptr);
    ~InfoView() override;
};
