#include "infoview.h"

#include <QHeaderView>

InfoView::InfoView(QWidget* parent)
    : QTreeView(parent)
{
    setRootIsDecorated(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setMouseTracking(true);
    setItemsExpandable(false);
    setIndentation(0);
    setExpandsOnDoubleClick(false);
    setWordWrap(true);
    setTextElideMode(Qt::ElideLeft);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setSortingEnabled(false);
    setAlternatingRowColors(true);
}

InfoView::~InfoView() = default;
