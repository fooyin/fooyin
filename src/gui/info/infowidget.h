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
class Settings;
class WidgetProvider;

class InfoWidget : public Widget
{
public:
    InfoWidget(WidgetProvider* widgetProvider, QWidget* parent = nullptr);
    ~InfoWidget() override;

    bool isHeaderHidden();
    void setHeaderHidden(bool showHeader);

    bool isScrollbarHidden();
    void setScrollbarHidden(bool showScrollBar);

    bool altRowColors();
    void setAltRowColors(bool altColours);

    void layoutEditingMenu(QMenu* menu) override;

protected:
    void setupUi();
    void spanHeaders();
    static void refreshTrack(Track* track);

private:
    Settings* m_settings;
    PlayerManager* m_playerManager;
    Library::MusicLibrary* m_library;

    QHBoxLayout* m_layout;
    InfoView m_view;
    InfoModel m_model;
};
