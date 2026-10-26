/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "librarytreegroupeditordialog.h"

#include "librarytree/librarytreegroupregistry.h"
#include "settings/librarytree/librarytreegroupmodel.h"

#include <gui/widgets/checkboxdelegate.h>
#include <gui/widgets/extendabletableview.h>
#include <gui/widgets/scriptlineedit.h>

#include <QAction>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
class LibraryTreeGroupEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LibraryTreeGroupEditorWidget(LibraryTreeGroupRegistry* groupsRegistry, QWidget* parent = nullptr);

    void apply();
    void reset();

private:
    void populate(const LibraryTreeGrouping& selected = {}, int row = 0);
    void loadCurrentGroup();
    void setField(int column, const QString& text);
    void updateButtonState();

    LibraryTreeGroupRegistry* m_groupsRegistry;
    ExtendableTableView* m_groupList;
    LibraryTreeGroupModel* m_model;
    QWidget* m_details;
    QLineEdit* m_name;
    ScriptTextEdit* m_grouping;
    ScriptTextEdit* m_display;
    ScriptTextEdit* m_sort;
};

LibraryTreeGroupEditorWidget::LibraryTreeGroupEditorWidget(LibraryTreeGroupRegistry* groupsRegistry, QWidget* parent)
    : QWidget{parent}
    , m_groupsRegistry{groupsRegistry}
    , m_groupList{new ExtendableTableView(this)}
    , m_model{new LibraryTreeGroupModel(m_groupsRegistry, this)}
    , m_details{new QWidget(this)}
    , m_name{new QLineEdit(m_details)}
    , m_grouping{new ScriptTextEdit(m_details)}
    , m_display{new ScriptTextEdit(m_details)}
    , m_sort{new ScriptTextEdit(m_details)}
{
    m_groupList->setExtendableModel(m_model);
    m_groupList->setExtendableColumn(2);
    m_groupList->setEditOnAdd(false);
    m_groupList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_groupList->setItemDelegateForColumn(0, new CheckBoxDelegate(this));

    for(const int column : {1, 3, 4, 5}) {
        m_groupList->hideColumn(column);
    }

    m_groupList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_groupList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_groupList->verticalHeader()->hide();
    m_groupList->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_groupList->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    m_grouping->setPlaceholderText(tr("Use the display script"));
    m_display->setPlaceholderText(tr("Enter a display script"));
    m_sort->setPlaceholderText(tr("Use the grouping script"));

    auto* nameLayout = new QFormLayout();
    nameLayout->addRow(tr("Name") + u":"_s, m_name);

    auto* detailsLayout = new QVBoxLayout(m_details);
    detailsLayout->setContentsMargins({});
    detailsLayout->addLayout(nameLayout);

    const auto levelsHelp = tr("Use <code>||</code> between tree levels. All scripts must produce matching levels and "
                               "multi-value expansions.");

    const auto addScript = [&](const QString& title, const QString& help, ScriptTextEdit* editor) {
        auto* section = new QWidget(m_details);
        auto* label   = new QLabel(title, section);
        editor->addHint(u"<p>%1</p><p>%2</p>"_s.arg(help, levelsHelp));

        auto* layout = new QVBoxLayout(section);
        layout->setContentsMargins({});
        layout->addWidget(label);
        layout->addWidget(editor, 1);
        detailsLayout->addWidget(section, 1);
    };

    addScript(tr("Grouping script"),
              tr("Tracks with matching results share tree nodes. Leave empty to use the display script."), m_grouping);
    addScript(tr("Display script"),
              tr("Use <code>%trackcount%</code> and <code>%childcount%</code> for counts, "
                 "<code>&lt;right&gt;</code> for right-aligned text, or <code>%frontcover%</code> "
                 "at the start or end of a level for artwork."),
              m_display);
    addScript(tr("Sort script"),
              tr("Sets the order within each level. Leave a level empty to use its grouping script."), m_sort);

    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins({});
    mainLayout->addWidget(m_groupList);
    mainLayout->addWidget(m_details, 1);

    QWidget::setTabOrder(m_groupList, m_name);
    QWidget::setTabOrder(m_name, m_grouping);
    QWidget::setTabOrder(m_grouping, m_display);
    QWidget::setTabOrder(m_display, m_sort);

    QObject::connect(m_groupList->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
                     &LibraryTreeGroupEditorWidget::loadCurrentGroup);
    QObject::connect(m_groupList->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &LibraryTreeGroupEditorWidget::updateButtonState);
    QObject::connect(m_model, &QAbstractItemModel::dataChanged, this, &LibraryTreeGroupEditorWidget::updateButtonState);
    QObject::connect(m_model, &QAbstractItemModel::modelReset, this, [this]() {
        if(m_model->rowCount({}) > 0) {
            m_groupList->setCurrentIndex(m_model->index(0, 2, {}));
        }
        loadCurrentGroup();
    });
    QObject::connect(m_name, &QLineEdit::textEdited, this, [this](const QString& text) { setField(2, text); });
    QObject::connect(m_grouping, &QPlainTextEdit::textChanged, this, [this]() { setField(3, m_grouping->text()); });
    QObject::connect(m_display, &QPlainTextEdit::textChanged, this, [this]() { setField(4, m_display->text()); });
    QObject::connect(m_sort, &QPlainTextEdit::textChanged, this, [this]() { setField(5, m_sort->text()); });

    QObject::connect(m_groupList->addRowAction(), &QAction::triggered, this, [this]() {
        m_name->setFocus();
        m_name->selectAll();
    });

    populate();
}

void LibraryTreeGroupEditorWidget::apply()
{
    const auto index   = m_groupList->currentIndex();
    auto selected      = index.data(Qt::UserRole).value<LibraryTreeGrouping>();
    auto* item         = index.isValid() ? static_cast<LibraryTreeGroupItem*>(index.internalPointer()) : nullptr;
    const bool removed = item && item->status() == LibraryTreeGroupItem::Removed;
    const int row      = index.row();

    m_model->processQueue();

    if(item && !removed) {
        selected = item->group();
    }
    populate(selected, row);
}

void LibraryTreeGroupEditorWidget::reset()
{
    m_groupsRegistry->reset();
    populate();
}

void LibraryTreeGroupEditorWidget::populate(const LibraryTreeGrouping& selected, int row)
{
    m_model->populate();

    const int count = m_model->rowCount({});
    if(count == 0) {
        return;
    }

    row = std::clamp(row, 0, count - 1);
    for(int i{0}; i < count; ++i) {
        const auto group = m_model->index(i, 2, {}).data(Qt::UserRole).value<LibraryTreeGrouping>();
        if((selected.id >= 0 && group.id == selected.id)
           || (selected.id < 0 && !selected.name.isEmpty() && group.name == selected.name)) {
            row = i;
            break;
        }
    }

    m_groupList->setCurrentIndex(m_model->index(row, 2, {}));
}

void LibraryTreeGroupEditorWidget::loadCurrentGroup()
{
    const QSignalBlocker nameBlocker{m_name};
    const QSignalBlocker groupingBlocker{m_grouping};
    const QSignalBlocker displayBlocker{m_display};
    const QSignalBlocker sortBlocker{m_sort};

    const auto index = m_groupList->currentIndex();
    const auto group = index.data(Qt::UserRole).value<LibraryTreeGrouping>();

    m_name->setText(group.name);
    m_grouping->setText(group.groupingScript);
    m_display->setText(group.script);
    m_sort->setText(group.sortScript);

    updateButtonState();
}

void LibraryTreeGroupEditorWidget::setField(int column, const QString& text)
{
    const auto index = m_groupList->currentIndex();
    if(index.isValid()) {
        m_model->setData(index.siblingAtColumn(column), text, Qt::EditRole);
    }
}

void LibraryTreeGroupEditorWidget::updateButtonState()
{
    const auto index   = m_groupList->currentIndex();
    const auto group   = index.data(Qt::UserRole).value<LibraryTreeGrouping>();
    const bool removed = index.data(Qt::FontRole).value<QFont>().strikeOut();
    m_details->setEnabled(index.isValid() && !removed);
    m_groupList->removeRowAction()->setEnabled(index.isValid() && m_groupList->selectionModel()->hasSelection()
                                               && !group.isDefault && !removed);
    if(m_name->text() != group.name) {
        const QSignalBlocker blocker{m_name};
        m_name->setText(group.name);
    }
}

LibraryTreeGroupEditorDialog::LibraryTreeGroupEditorDialog(LibraryTreeGroupRegistry* groupsRegistry, QWidget* parent)
    : QDialog{parent}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Manage Library Tree Groupings"));
    resize(1000, 760);

    auto* editor = new LibraryTreeGroupEditorWidget(groupsRegistry, this);

    auto* buttons
        = new QDialogButtonBox(QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Apply
                                   | QDialogButtonBox::StandardButton::Cancel | QDialogButtonBox::StandardButton::Reset,
                               this);
    if(auto* reset = buttons->button(QDialogButtonBox::StandardButton::Reset)) {
        reset->setText(tr("Restore Defaults"));
    }

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(editor, 1);
    layout->addWidget(buttons);

    QObject::connect(buttons->button(QDialogButtonBox::StandardButton::Apply), &QPushButton::clicked, editor,
                     &LibraryTreeGroupEditorWidget::apply);
    QObject::connect(buttons->button(QDialogButtonBox::StandardButton::Reset), &QPushButton::clicked, editor,
                     &LibraryTreeGroupEditorWidget::reset);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, [this, editor]() {
        editor->apply();
        accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
} // namespace Fooyin

#include "librarytreegroupeditordialog.moc"
