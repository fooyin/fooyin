#include "infowidget.h"

#include "core/library/musiclibrary.h"
#include "core/player/playermanager.h"
#include "gui/info/infoitem.h"
#include "gui/info/itemdelegate.h"
#include "models/track.h"
#include "utils/settings.h"

#include <QHeaderView>
#include <QMenu>
#include <QTableWidget>

InfoWidget::InfoWidget(PlayerManager* playerManager, Library::MusicLibrary* library, QWidget* parent)
    : Widget(parent)
    , m_settings(Settings::instance())
    , m_playerManager(playerManager)
    , m_library(library)
    , m_layout(new QHBoxLayout(this))
{
    setObjectName("Info");
    setupUi();

    setHeaderHidden(!m_settings->value(Settings::Setting::InfoHeader).toBool());
    setScrollbarHidden(!m_settings->value(Settings::Setting::InfoScrollBar).toBool());
    setAltRowColors(m_settings->value(Settings::Setting::InfoAltColours).toBool());

    connect(m_playerManager, &PlayerManager::currentTrackChanged, this, &InfoWidget::refreshTrack);
    connect(m_playerManager, &PlayerManager::currentTrackChanged, &m_model, &InfoModel::reset);

    connect(m_settings, &Settings::infoAltColorsChanged, this, [this] {
        setAltRowColors(!altRowColors());
    });
    connect(m_settings, &Settings::infoHeaderChanged, this, [this] {
        setHeaderHidden(!isHeaderHidden());
    });
    connect(m_settings, &Settings::infoScrollBarChanged, this, [this] {
        setScrollbarHidden(!isScrollbarHidden());
    });

    spanHeaders();
}

InfoWidget::~InfoWidget() = default;

void InfoWidget::setupUi()
{
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_view.setItemDelegate(new ItemDelegate(this));
    m_view.setModel(&m_model);

    m_layout->addWidget(&m_view);

    m_view.header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    spanHeaders();
}

void InfoWidget::spanHeaders()
{
    for(int i = 0; i < m_model.rowCount({}); i++) {
        auto type = m_model.index(i, 0, {}).data(Role::Type).value<InfoItem::Type>();
        if(type == InfoItem::Type::Header) {
            m_view.setFirstColumnSpanned(i, {}, true);
        }
    }
}

void InfoWidget::refreshTrack(Track* track)
{
    if(track) {
        //        spanHeaders();
        //        m_table.resetLayout();
        //        m_table.resizeColumnToContents(0);
    }
}

bool InfoWidget::isHeaderHidden()
{
    return m_view.isHeaderHidden();
}

void InfoWidget::setHeaderHidden(bool b)
{
    m_view.setHeaderHidden(b);
}

bool InfoWidget::isScrollbarHidden()
{
    return m_view.verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff;
}

void InfoWidget::setScrollbarHidden(bool b)
{
    m_view.setVerticalScrollBarPolicy(b ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
}

bool InfoWidget::altRowColors()
{
    return m_view.alternatingRowColors();
}

void InfoWidget::setAltRowColors(bool b)
{
    m_view.setAlternatingRowColors(b);
}

void InfoWidget::layoutEditingMenu(QMenu* menu)
{
    auto* showHeaders = new QAction("Show Header", this);
    showHeaders->setCheckable(true);
    showHeaders->setChecked(!isHeaderHidden());
    QAction::connect(showHeaders, &QAction::triggered, this, [this] {
        m_settings->set(Settings::Setting::InfoHeader, isHeaderHidden());
    });

    auto* showScrollBar = new QAction("Show Scrollbar", menu);
    showScrollBar->setCheckable(true);
    showScrollBar->setChecked(!isScrollbarHidden());
    QAction::connect(showScrollBar, &QAction::triggered, this, [this] {
        m_settings->set(Settings::Setting::InfoScrollBar, isScrollbarHidden());
    });
    menu->addAction(showScrollBar);

    auto* altColours = new QAction("Alternate Row Colours", this);
    altColours->setCheckable(true);
    altColours->setChecked(altRowColors());
    QAction::connect(altColours, &QAction::triggered, this, [this] {
        m_settings->set(Settings::Setting::InfoAltColours, !altRowColors());
    });

    menu->addAction(showHeaders);
    menu->addAction(showScrollBar);
    menu->addAction(altColours);
}
