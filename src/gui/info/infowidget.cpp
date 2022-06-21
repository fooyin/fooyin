#include "infowidget.h"

#include "core/library/musiclibrary.h"
#include "core/player/playermanager.h"
#include "gui/info/infoitem.h"
#include "gui/info/itemdelegate.h"
#include "models/track.h"

#include <QHeaderView>
#include <QTableWidget>

InfoWidget::InfoWidget(PlayerManager* playerManager, Library::MusicLibrary* library, QWidget* parent)
    : Widget(parent)
    , m_playerManager(playerManager)
    , m_library(library)
    , m_layout(new QHBoxLayout(this))
{
    setObjectName("Info");
    setupUi();

    connect(m_playerManager, &PlayerManager::currentTrackChanged, this, &InfoWidget::refreshTrack);
    connect(m_playerManager, &PlayerManager::currentTrackChanged, &m_model, &InfoModel::reset);

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

void InfoWidget::layoutEditingMenu(QMenu* menu)
{
    Q_UNUSED(menu)
}
