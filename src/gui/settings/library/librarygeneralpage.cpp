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

#include "librarygeneralpage.h"

#include "librarymodel.h"

#include "core/internalcoresettings.h"
#include "core/library/libraryinfo.h"

#include <core/coresettings.h>
#include <core/library/musiclibrary.h>
#include <gui/guiconstants.h>
#include <utils/extendabletableview.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMenu>
#include <QPushButton>

namespace Fooyin {
class LibraryTableView : public ExtendableTableView
{
    Q_OBJECT

public:
    using ExtendableTableView::ExtendableTableView;

    void setupContextActions(QMenu* menu, const QPoint& pos) override;

signals:
    void refreshLibrary(const Fooyin::LibraryInfo& info);
    void rescanLibrary(const Fooyin::LibraryInfo& info);
};

void LibraryTableView::setupContextActions(QMenu* menu, const QPoint& pos)
{
    const QModelIndex index = indexAt(pos);
    if(!index.isValid()) {
        return;
    }

    const auto library   = index.data(Qt::UserRole).value<LibraryInfo>();
    const bool isPending = library.status == LibraryInfo::Status::Pending;

    auto* refresh = new QAction(tr("Refresh"), this);
    refresh->setEnabled(!isPending);
    QObject::connect(refresh, &QAction::triggered, this, [this, library]() { emit refreshLibrary(library); });

    auto* rescan = new QAction(tr("Rescan"), this);
    rescan->setEnabled(!isPending);
    QObject::connect(rescan, &QAction::triggered, this, [this, library]() { emit rescanLibrary(library); });

    menu->addAction(refresh);
    menu->addAction(rescan);
}

class LibraryGeneralPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit LibraryGeneralPageWidget(ActionManager* actionManager, LibraryManager* libraryManager,
                                      MusicLibrary* library, SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void addLibrary() const;

    LibraryManager* m_libraryManager;
    MusicLibrary* m_library;
    SettingsManager* m_settings;

    LibraryTableView* m_libraryView;
    LibraryModel* m_model;

    QCheckBox* m_autoRefresh;
    QCheckBox* m_monitorLibraries;
};

LibraryGeneralPageWidget::LibraryGeneralPageWidget(ActionManager* actionManager, LibraryManager* libraryManager,
                                                   MusicLibrary* library, SettingsManager* settings)
    : m_libraryManager{libraryManager}
    , m_library{library}
    , m_settings{settings}
    , m_libraryView{new LibraryTableView(actionManager, this)}
    , m_model{new LibraryModel(m_libraryManager, this)}
    , m_autoRefresh{new QCheckBox(tr("Auto refresh on startup"), this)}
    , m_monitorLibraries{new QCheckBox(tr("Monitor libraries"), this)}
{
    m_libraryView->setExtendableModel(m_model);

    // Hide Id column
    m_libraryView->hideColumn(0);

    m_libraryView->setExtendableColumn(1);
    m_libraryView->verticalHeader()->hide();
    m_libraryView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_libraryView->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_autoRefresh->setToolTip(tr("Scan libraries for changes on startup"));
    m_monitorLibraries->setToolTip(tr("Monitor libraries for external changes"));

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(m_libraryView, 0, 0, 1, 2);
    mainLayout->addWidget(m_autoRefresh, 1, 0, 1, 2);
    mainLayout->addWidget(m_monitorLibraries, 2, 0, 1, 2);

    mainLayout->setColumnStretch(1, 1);

    QObject::connect(m_model, &LibraryModel::requestAddLibrary, this, &LibraryGeneralPageWidget::addLibrary);
    QObject::connect(m_libraryView, &LibraryTableView::refreshLibrary, this,
                     [this](const auto& info) { m_library->refresh(info); });
    QObject::connect(m_libraryView, &LibraryTableView::rescanLibrary, this,
                     [this](const auto& info) { m_library->rescan(info); });
}

void LibraryGeneralPageWidget::load()
{
    m_autoRefresh->setChecked(m_settings->value<Settings::Core::AutoRefresh>());
    m_monitorLibraries->setChecked(m_settings->value<Settings::Core::Internal::MonitorLibraries>());

    m_model->populate();
}

void LibraryGeneralPageWidget::apply()
{
    m_settings->set<Settings::Core::AutoRefresh>(m_autoRefresh->isChecked());
    m_settings->set<Settings::Core::Internal::MonitorLibraries>(m_monitorLibraries->isChecked());

    m_model->processQueue();
}

void LibraryGeneralPageWidget::reset()
{
    m_settings->reset<Settings::Core::AutoRefresh>();
    m_settings->reset<Settings::Core::Internal::MonitorLibraries>();
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

    m_model->markForAddition({name, dir});
}

LibraryGeneralPage::LibraryGeneralPage(ActionManager* actionManager, LibraryManager* libraryManager,
                                       MusicLibrary* library, SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::LibraryGeneral);
    setName(tr("General"));
    setCategory({tr("Library")});
    setWidgetCreator([actionManager, libraryManager, library, settings] {
        return new LibraryGeneralPageWidget(actionManager, libraryManager, library, settings);
    });
}
} // namespace Fooyin

#include "librarygeneralpage.moc"
#include "moc_librarygeneralpage.cpp"
