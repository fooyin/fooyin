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

#include "settings/tageditorfieldregistry.h"
#include "tageditorconstants.h"
#include "tageditormodel.h"
#include "tageditorview.h"

#include <core/constants.h>
#include <core/coresettings.h>
#include <gui/guiconstants.h>
#include <gui/widgets/multilinedelegate.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/stardelegate.h>

#include <QCheckBox>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QToolButton>

constexpr auto DontAskAgain = "TagEditor/DontAskAgain";
constexpr auto State        = "TagEditor/State";

namespace Fooyin::TagEditor {
TagEditorWidget::TagEditorWidget(ActionManager* actionManager, TagEditorFieldRegistry* registry,
                                 SettingsManager* settings, QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_registry{registry}
    , m_settings{settings}
    , m_readOnly{false}
    , m_view{new TagEditorView(actionManager, this)}
    , m_model{new TagEditorModel(settings, this)}
    , m_multilineDelegate{nullptr}
    , m_starDelegate{nullptr}
    , m_autoTrackNum{new QAction(tr("Auto &track number"), this)}
    , m_changeFields{new QAction(tr("&Change default fields…"), this)}
{
    setObjectName(TagEditorWidget::name());
    setWindowFlags(Qt::Dialog);
    resize(600, 720);
    setMinimumSize(300, 400);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    m_view->setExtendableModel(m_model);
    m_view->setupActions();

    QObject::connect(m_model, &QAbstractItemModel::rowsInserted, m_view, &QTableView::resizeRowsToContents);
    QObject::connect(m_model, &QAbstractItemModel::modelReset, this, [this]() {
        m_view->resizeColumnsToContents();
        m_view->resizeRowsToContents();
        restoreState();
    });
    QObject::connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        const QModelIndexList selected = m_view->selectionModel()->selectedIndexes();
        m_view->removeRowAction()->setEnabled(!m_readOnly && !selected.empty());
    });
    QObject::connect(m_autoTrackNum, &QAction::triggered, m_model, &TagEditorModel::autoNumberTracks);
    QObject::connect(m_changeFields, &QAction::triggered, this,
                     [this]() { m_settings->settingsDialog()->openAtPage(Constants::Page::TagEditorFields); });
}

TagEditorWidget::~TagEditorWidget()
{
    saveState();
}

void TagEditorWidget::setTracks(const TrackList& tracks)
{
    // Reset all delegates
    for(const int row : m_delegateRows) {
        m_view->setItemDelegateForRow(row, nullptr);
    }
    m_delegateRows.clear();
    m_view->setRatingRow(-1);
    m_model->setRatingRow(-1);

    const auto items = m_registry->items();
    for(int row{0}; const auto& item : items) {
        if(item.scriptField.compare(QLatin1String{Fooyin::Constants::MetaData::RatingEditor}, Qt::CaseInsensitive)
           == 0) {
            if(!m_starDelegate) {
                m_starDelegate = new StarDelegate(this);
            }
            m_view->setItemDelegateForRow(row, m_starDelegate);
            m_view->setRatingRow(row);
            m_model->setRatingRow(row);
            m_delegateRows.emplace(row);
        }
        else if(item.multiline) {
            if(!m_multilineDelegate) {
                m_multilineDelegate = new MultiLineEditDelegate(this);
            }
            m_view->setItemDelegateForRow(row, m_multilineDelegate);
            m_delegateRows.emplace(row);
        }
        ++row;
    }

    m_model->reset(tracks, items);

    const auto refreshModel = [this]() {
        setTracks(m_model->tracks());
    };

    QObject::disconnect(m_registry, nullptr, this, nullptr);
    QObject::connect(m_registry, &TagEditorFieldRegistry::itemAdded, this, refreshModel);
    QObject::connect(m_registry, &TagEditorFieldRegistry::itemChanged, this, refreshModel);
    QObject::connect(m_registry, &TagEditorFieldRegistry::itemRemoved, this, refreshModel);
}

void TagEditorWidget::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;

    m_view->setTagEditTriggers(readOnly ? QAbstractItemView::NoEditTriggers : QAbstractItemView::AllEditTriggers);
    m_view->addRowAction()->setDisabled(readOnly);
    m_view->removeRowAction()->setDisabled(readOnly);
    m_autoTrackNum->setDisabled(readOnly);
}

QString TagEditorWidget::name() const
{
    return QStringLiteral("Tag Editor");
}

QString TagEditorWidget::layoutName() const
{
    return QStringLiteral("TagEditor");
}

bool TagEditorWidget::hasTools() const
{
    return true;
}

void TagEditorWidget::addTools(QMenu* menu)
{
    menu->addAction(m_autoTrackNum);
    menu->addAction(m_changeFields);
}

void TagEditorWidget::apply()
{
    if(!m_model->haveChanges()) {
        return;
    }

    const bool updateStats = m_model->haveOnlyStatChanges()
                          && !m_settings->value<Settings::Core::SaveRatingToMetadata>()
                          && !m_settings->value<Settings::Core::SavePlaycountToMetadata>();

    auto applyChanges = [this, updateStats]() {
        m_model->applyChanges();
        if(updateStats) {
            emit trackStatsChanged(m_model->tracks());
        }
        else {
            emit trackMetadataChanged(m_model->tracks());
        }
    };

    if(m_settings->fileValue(DontAskAgain).toBool()) {
        applyChanges();
        return;
    }

    QMessageBox message;
    message.setIcon(QMessageBox::Warning);
    message.setText(tr("Are you sure?"));
    message.setInformativeText(tr("Metadata in the associated files will be overwritten."));

    auto* dontAskAgain = new QCheckBox(tr("Don't ask again"), &message);
    message.setCheckBox(dontAskAgain);

    message.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    message.setDefaultButton(QMessageBox::No);

    const int buttonClicked = message.exec();

    if(buttonClicked == QMessageBox::Yes) {
        if(dontAskAgain->isChecked()) {
            m_settings->fileSet(DontAskAgain, true);
        }
        applyChanges();
    }
}

void TagEditorWidget::saveState() const
{
    const QByteArray state = m_view->horizontalHeader()->saveState();
    m_settings->fileSet(State, state);
}

void TagEditorWidget::restoreState() const
{
    const QByteArray state = m_settings->fileValue(State).toByteArray();

    if(state.isEmpty()) {
        return;
    }

    m_view->horizontalHeader()->restoreState(state);
}
} // namespace Fooyin::TagEditor

#include "moc_tageditorwidget.cpp"
