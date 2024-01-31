/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QTableView>

constexpr auto TagEditorState        = "TagEditor/State";
constexpr auto TagEditorDontAskAgain = "TagEditor/DontAskAgain";

namespace Fooyin::TagEditor {
TagEditorView::TagEditorView(ActionManager* actionManager, QWidget* parent)
    : ExtendableTableView{actionManager, parent}
{ }

int TagEditorView::sizeHintForRow(int row) const
{
    if(!model()->hasIndex(row, 0, {})) {
        return 0;
    }

    // Only resize rows based on first column
    return model()->index(row, 0, {}).data(Qt::SizeHintRole).toSize().height();
}

struct TagEditorWidget::Private
{
    TagEditorWidget* self;

    ActionManager* actionManager;
    SettingsManager* settings;

    WidgetContext* context;

    TagEditorView* view;
    TagEditorModel* model;

    Private(TagEditorWidget* self_, ActionManager* actionManager_, SettingsManager* settings_)
        : self{self_}
        , actionManager{actionManager_}
        , settings{settings_}
        , context{new WidgetContext(self, Context{"Context.TagEditor"}, self)}
        , view{new TagEditorView(actionManager, self)}
        , model{new TagEditorModel(settings, self)}
    {
        auto* layout = new QHBoxLayout(self);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(view);

        view->setExtendableModel(model);

        view->horizontalHeader()->setStretchLastSection(true);
        view->horizontalHeader()->setSectionsClickable(false);
        view->verticalHeader()->setVisible(false);
        view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

        actionManager->addContextObject(context);
    }

    void saveState() const
    {
        const QByteArray state = view->horizontalHeader()->saveState();
        settings->fileSet(TagEditorState, state);
    }

    void restoreState() const
    {
        const QByteArray state = settings->fileValue(TagEditorState).toByteArray();

        if(state.isEmpty()) {
            return;
        }

        view->horizontalHeader()->restoreState(state);
    };
};

TagEditorWidget::TagEditorWidget(const TrackList& tracks, ActionManager* actionManager, SettingsManager* settings,
                                 QWidget* parent)
    : PropertiesTabWidget{parent}
    , p{std::make_unique<Private>(this, actionManager, settings)}
{
    setObjectName(TagEditorWidget::name());
    setWindowFlags(Qt::Dialog);
    resize(600, 720);
    setMinimumSize(300, 400);

    const QFontMetrics fontMetrics{font()};
    const int width = fontMetrics.horizontalAdvance(TagEditorModel::defaultFieldText()) + 15;

    p->view->setColumnWidth(0, width);

    QObject::connect(p->model, &TagEditorModel::trackMetadataChanged, this, &TagEditorWidget::trackMetadataChanged);
    QObject::connect(p->view->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        const QModelIndexList selected = p->view->selectionModel()->selectedIndexes();
        const bool canRemove           = std::ranges::any_of(std::as_const(selected), [](const QModelIndex& index) {
            return !index.data(TagEditorItem::IsDefault).toBool();
        });
        p->view->removeAction()->setEnabled(canRemove);
    });

    p->model->reset(tracks);

    p->restoreState();
}

TagEditorWidget::~TagEditorWidget()
{
    p->saveState();
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
    if(!p->model->tagsHaveChanged()) {
        return;
    }

    if(p->settings->fileValue(TagEditorDontAskAgain).toBool()) {
        p->model->processQueue();
        return;
    }

    QMessageBox message;
    message.setIcon(QMessageBox::Warning);
    message.setText("Are you sure?");
    message.setInformativeText(tr("Metadata in the associated files will be overwritten."));

    auto* dontAskAgain = new QCheckBox(tr("Don't ask again"), &message);
    message.setCheckBox(dontAskAgain);

    message.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    message.setDefaultButton(QMessageBox::No);

    const int buttonClicked = message.exec();

    if(buttonClicked == QMessageBox::Yes) {
        if(dontAskAgain->isChecked()) {
            p->settings->fileSet(TagEditorDontAskAgain, true);
        }
        p->model->processQueue();
    }
}

void TagEditorWidget::contextMenuEvent(QContextMenuEvent* /*event*/) { }
} // namespace Fooyin::TagEditor

#include "moc_tageditorwidget.cpp"
