/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "librarygeneralpage.h"

#include "librarymodel.h"

#include "core/library/libraryinfo.h"

#include <core/coresettings.h>
#include <gui/guiconstants.h>
#include <utils/extendabletableview.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QTableView>

namespace Fooyin {
class LibraryGeneralPageWidget : public SettingsPageWidget
{
public:
    explicit LibraryGeneralPageWidget(ActionManager* actionManager, LibraryManager* libraryManager,
                                      SettingsManager* settings);

    void apply() override;
    void reset() override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void resizeTable();
    void addLibrary() const;

    LibraryManager* m_libraryManager;
    SettingsManager* m_settings;

    ExtendableTableView* m_libraryView;
    LibraryModel* m_model;

    QCheckBox* m_autoRefresh;

    QLineEdit* m_sortScript;
};

LibraryGeneralPageWidget::LibraryGeneralPageWidget(ActionManager* actionManager, LibraryManager* libraryManager,
                                                   SettingsManager* settings)
    : m_libraryManager{libraryManager}
    , m_settings{settings}
    , m_libraryView{new ExtendableTableView(actionManager, this)}
    , m_model{new LibraryModel(m_libraryManager, this)}
    , m_autoRefresh{new QCheckBox(tr("Auto refresh on startup"), this)}
    , m_sortScript{new QLineEdit(this)}
{
    m_libraryView->setExtendableModel(m_model);

    // Hide Id column
    m_libraryView->hideColumn(0);

    m_libraryView->setExtendableColumn(1);
    m_libraryView->verticalHeader()->hide();
    m_libraryView->horizontalHeader()->setStretchLastSection(true);
    m_libraryView->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_autoRefresh->setToolTip(tr("Scan libraries for changes on startup."));
    m_autoRefresh->setChecked(m_settings->value<Settings::Core::AutoRefresh>());

    auto* sortScriptLabel = new QLabel(tr("Sort tracks by:"), this);

    m_sortScript->setText(m_settings->value<Settings::Core::LibrarySortScript>());

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(m_libraryView, 0, 0, 1, 2);
    mainLayout->addWidget(m_autoRefresh, 1, 0, 1, 2);
    mainLayout->addWidget(sortScriptLabel, 2, 0);
    mainLayout->addWidget(m_sortScript, 2, 1);

    mainLayout->setColumnStretch(1, 1);

    QObject::connect(m_libraryView, &ExtendableTableView::newRowClicked, this, &LibraryGeneralPageWidget::addLibrary);

    m_model->populate();
    resizeTable();
}

void LibraryGeneralPageWidget::apply()
{
    m_settings->set<Settings::Core::AutoRefresh>(m_autoRefresh->isChecked());
    m_settings->set<Settings::Core::LibrarySortScript>(m_sortScript->text());

    m_model->processQueue();
}

void LibraryGeneralPageWidget::reset()
{
    m_settings->reset<Settings::Core::AutoRefresh>();
    m_settings->reset<Settings::Core::LibrarySortScript>();
}

void LibraryGeneralPageWidget::resizeEvent(QResizeEvent* event)
{
    SettingsPageWidget::resizeEvent(event);

    resizeTable();
}

void LibraryGeneralPageWidget::resizeTable()
{
    m_libraryView->setColumnWidth(1, static_cast<int>(m_libraryView->width() * 0.25));
    m_libraryView->setColumnWidth(2, static_cast<int>(m_libraryView->width() * 0.55));
    m_libraryView->setColumnWidth(3, static_cast<int>(m_libraryView->width() * 0.20));
}

void LibraryGeneralPageWidget::addLibrary() const
{
    const QString dir = QFileDialog::getExistingDirectory(m_libraryView, tr("Directory"), QDir::homePath(),
                                                          QFileDialog::ShowDirsOnly);

    if(dir.isEmpty()) {
        m_model->markForAddition({});
        return;
    }

    const QFileInfo info{dir};
    const QString name = info.fileName();

    const QString text
        = QInputDialog::getText(m_libraryView, tr("Add Library"), tr("Library Name:"), QLineEdit::Normal, name);

    m_model->markForAddition({text, dir});
}

LibraryGeneralPage::LibraryGeneralPage(ActionManager* actionManager, LibraryManager* libraryManager,
                                       SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::LibraryGeneral);
    setName(tr("General"));
    setCategory({tr("Library")});
    setWidgetCreator([actionManager, libraryManager, settings] {
        return new LibraryGeneralPageWidget(actionManager, libraryManager, settings);
    });
}

} // namespace Fooyin
