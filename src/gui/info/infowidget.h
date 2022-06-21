#pragma once

#include "gui/info/infomodel.h"
#include "gui/info/infoview.h"
#include "gui/widgets/widget.h"

#include <QHBoxLayout>
#include <QWidget>

namespace Library {
class MusicLibrary;
}

class PlayerManager;
class Track;

class InfoWidget : public Widget
{
public:
    InfoWidget(PlayerManager* playerManager, Library::MusicLibrary* library, QWidget* parent = nullptr);
    ~InfoWidget() override;

    void layoutEditingMenu(QMenu* menu) override;

protected:
    void setupUi();
    void spanHeaders();
    void refreshTrack(Track* track);

private:
    PlayerManager* m_playerManager;
    Library::MusicLibrary* m_library;

    QHBoxLayout* m_layout;
    InfoView m_view;
    InfoModel m_model;
};
