/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "tageditorwidget.h"

#include "tageditoritem.h"
#include "tageditormodel.h"

#include <utils/actions/actionmanager.h>
#include <utils/actions/widgetcontext.h>
#include <utils/multilinedelegate.h>
#include <utils/settings/settingsmanager.h>
#include <utils/stardelegate.h>

#include <QCheckBox>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QTableView>

namespace Fooyin::TagEditor {
class TagEditorDelegate : public MultiLineEditDelegate
{
public:
    using MultiLineEditDelegate::MultiLineEditDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if(index.data(TagEditorItem::IsDefault).toBool()) {
            // NOLINTNEXTLINE
            return QStyledItemDelegate::createEditor(parent, option, index); // clazy:exclude=skipped-base-method
        }
        return MultiLineEditDelegate::createEditor(parent, option, index);
    }
};

TagEditorView::TagEditorView(ActionManager* actionManager, QWidget* parent)
    : ExtendableTableView{actionManager, parent}
{
    setItemDelegateForColumn(1, new TagEditorDelegate(this));
    setItemDelegateForRow(13, new StarDelegate(this));
    setTextElideMode(Qt::ElideRight);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setSectionsClickable(false);
    verticalHeader()->setVisible(false);
}

int TagEditorView::sizeHintForRow(int row) const
{
    if(!model()->hasIndex(row, 0, {})) {
        return 0;
    }

    // Only resize rows based on first column
    return model()->index(row, 0, {}).data(Qt::SizeHintRole).toSize().height();
}

void TagEditorView::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::RightButton) {
        // Don't start editing on right-click
        setEditTriggers(QAbstractItemView::NoEditTriggers);
    }
    else {
        setEditTriggers(QAbstractItemView::AllEditTriggers);
    }
    QTableView::mousePressEvent(event);
}

TagEditorWidget::TagEditorWidget(const TrackList& tracks, ActionManager* actionManager, SettingsManager* settings,
                                 QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_actionManager{actionManager}
    , m_settings{settings}
    , m_context{new WidgetContext(this, Context{"Context.TagEditor"}, this)}
    , m_view{new TagEditorView(actionManager, this)}
    , m_model{new TagEditorModel(settings, this)}
{
    setObjectName(TagEditorWidget::name());
    setWindowFlags(Qt::Dialog);
    resize(600, 720);
    setMinimumSize(300, 400);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    m_view->setExtendableModel(m_model);

    m_actionManager->addContextObject(m_context);

    QObject::connect(m_model, &QAbstractItemModel::rowsInserted, m_view, &QTableView::resizeRowsToContents);
    QObject::connect(m_model, &QAbstractItemModel::modelReset, this, [this]() {
        m_view->resizeColumnsToContents();
        m_view->resizeRowsToContents();
        restoreState();
    });
    QObject::connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        const QModelIndexList selected = m_view->selectionModel()->selectedIndexes();
        m_view->removeAction()->setEnabled(!selected.empty());
    });

    m_model->reset(tracks);
}

TagEditorWidget::~TagEditorWidget()
{
    saveState();
}

QString TagEditorWidget::name() const
{
    return QStringLiteral("Tag Editor");
}

QString TagEditorWidget::layoutName() const
{
    return QStringLiteral("TagEditor");
}

void TagEditorWidget::apply()
{
    if(!m_model->haveChanges()) {
        return;
    }

    if(m_settings->fileValue(QStringLiteral("TagEditor/DontAskAgain")).toBool()) {
        m_model->applyChanges();
        emit trackMetadataChanged(m_model->tracks());
        return;
    }

    QMessageBox message;
    message.setIcon(QMessageBox::Warning);
    message.setText(QStringLiteral("Are you sure?"));
    message.setInformativeText(tr("Metadata in the associated files will be overwritten."));

    auto* dontAskAgain = new QCheckBox(tr("Don't ask again"), &message);
    message.setCheckBox(dontAskAgain);

    message.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    message.setDefaultButton(QMessageBox::No);

    const int buttonClicked = message.exec();

    if(buttonClicked == QMessageBox::Yes) {
        if(dontAskAgain->isChecked()) {
            m_settings->fileSet(QStringLiteral("TagEditor/DontAskAgain"), true);
        }
        m_model->applyChanges();
        emit trackMetadataChanged(m_model->tracks());
    }
}

void TagEditorWidget::saveState() const
{
    const QByteArray state = m_view->horizontalHeader()->saveState();
    m_settings->fileSet(QStringLiteral("TagEditor/State"), state);
}

void TagEditorWidget::restoreState() const
{
    const QByteArray state = m_settings->fileValue(QStringLiteral("TagEditor/State")).toByteArray();

    if(state.isEmpty()) {
        return;
    }

    m_view->horizontalHeader()->restoreState(state);
}
} // namespace Fooyin::TagEditor

#include "moc_tageditorwidget.cpp"
