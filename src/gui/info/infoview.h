#pragma once

#include <QTreeView>

class InfoView : public QTreeView
{
public:
    InfoView(QWidget* parent = nullptr);
    ~InfoView() override;
};
