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
#include <gui/widgets/extendabletableview.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QPushButton>

namespace Fooyin {
class LibraryTableView : public ExtendableTableView
{
    Q_OBJECT

public:
    using ExtendableTableView::ExtendableTableView;

protected:
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

    auto* refresh = new QAction(tr("&Scan for changes"), this);
    refresh->setEnabled(!isPending);
    QObject::connect(refresh, &QAction::triggered, this, [this, library]() { emit refreshLibrary(library); });

    auto* rescan = new QAction(tr("&Reload tracks"), this);
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

    QLineEdit* m_restrictTypes;
    QLineEdit* m_excludeTypes;

    QCheckBox* m_autoRefresh;
    QCheckBox* m_monitorLibraries;
    QCheckBox* m_markUnavailable;
    QCheckBox* m_markUnavailableStart;
    QCheckBox* m_useVariousCompilations;
    QCheckBox* m_saveRatings;
    QCheckBox* m_savePlaycounts;
};

LibraryGeneralPageWidget::LibraryGeneralPageWidget(ActionManager* actionManager, LibraryManager* libraryManager,
                                                   MusicLibrary* library, SettingsManager* settings)
    : m_libraryManager{libraryManager}
    , m_library{library}
    , m_settings{settings}
    , m_libraryView{new LibraryTableView(actionManager, this)}
    , m_model{new LibraryModel(m_libraryManager, this)}
    , m_restrictTypes{new QLineEdit(this)}
    , m_excludeTypes{new QLineEdit(this)}
    , m_autoRefresh{new QCheckBox(tr("Auto refresh on startup"), this)}
    , m_monitorLibraries{new QCheckBox(tr("Monitor libraries"), this)}
    , m_markUnavailable{new QCheckBox(tr("Mark unavailable tracks on playback"), this)}
    , m_markUnavailableStart{new QCheckBox(tr("Mark unavailable tracks on startup"), this)}
    , m_useVariousCompilations{new QCheckBox(tr("Use 'Various Artists' for compilations"), this)}
    , m_saveRatings{new QCheckBox(tr("Save ratings to file metadata"), this)}
    , m_savePlaycounts{new QCheckBox(tr("Save playcount to file metadata"), this)}
{
    m_libraryView->setExtendableModel(m_model);

    // Hide Id column
    m_libraryView->hideColumn(0);

    m_libraryView->setExtendableColumn(1);
    m_libraryView->verticalHeader()->hide();
    m_libraryView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    m_autoRefresh->setToolTip(tr("Scan libraries for changes on startup"));
    m_monitorLibraries->setToolTip(tr("Monitor libraries for external changes"));

    auto* fileTypesGroup  = new QGroupBox(tr("File Types"), this);
    auto* fileTypesLayout = new QGridLayout(fileTypesGroup);

    auto* restrictLabel = new QLabel(tr("Restrict to") + u":", this);
    auto* excludeLabel  = new QLabel(tr("Exclude") + u":", this);

    auto* fileHint = new QLabel(QStringLiteral("🛈 e.g. \"mp3;m4a\""), this);

    int row{0};
    fileTypesLayout->addWidget(restrictLabel, row, 0);
    fileTypesLayout->addWidget(m_restrictTypes, row++, 1);
    fileTypesLayout->addWidget(excludeLabel, row, 0);
    fileTypesLayout->addWidget(m_excludeTypes, row++, 1);
    fileTypesLayout->addWidget(fileHint, row++, 1);
    fileTypesLayout->setColumnStretch(1, 1);

    auto* mainLayout = new QGridLayout(this);

    row = 0;
    mainLayout->addWidget(m_libraryView, row++, 0, 1, 2);
    mainLayout->addWidget(fileTypesGroup, row++, 0, 1, 2);
    mainLayout->addWidget(m_autoRefresh, row++, 0, 1, 2);
    mainLayout->addWidget(m_monitorLibraries, row++, 0, 1, 2);
    mainLayout->addWidget(m_markUnavailable, row++, 0, 1, 2);
    mainLayout->addWidget(m_markUnavailableStart, row++, 0, 1, 2);
    mainLayout->addWidget(m_useVariousCompilations, row++, 0, 1, 2);
    mainLayout->addWidget(m_saveRatings, row++, 0, 1, 2);
    mainLayout->addWidget(m_savePlaycounts, row++, 0, 1, 2);
    mainLayout->setColumnStretch(1, 1);

    QObject::connect(m_model, &LibraryModel::requestAddLibrary, this, &LibraryGeneralPageWidget::addLibrary);
    QObject::connect(m_libraryView, &LibraryTableView::refreshLibrary, this,
                     [this](const auto& info) { m_library->refresh(info); });
    QObject::connect(m_libraryView, &LibraryTableView::rescanLibrary, this,
                     [this](const auto& info) { m_library->rescan(info); });
}

void LibraryGeneralPageWidget::load()
{
    m_model->populate();

    const QStringList restrictExtensions
        = m_settings->fileValue(Settings::Core::Internal::LibraryRestrictTypes).toStringList();
    const QStringList excludeExtensions
        = m_settings->fileValue(Settings::Core::Internal::LibraryExcludeTypes, QStringList{QStringLiteral("cue")})
              .toStringList();

    m_restrictTypes->setText(restrictExtensions.join(u';'));
    m_excludeTypes->setText(excludeExtensions.join(u';'));

    m_autoRefresh->setChecked(m_settings->value<Settings::Core::AutoRefresh>());
    m_monitorLibraries->setChecked(m_settings->value<Settings::Core::Internal::MonitorLibraries>());
    m_markUnavailable->setChecked(m_settings->fileValue(Settings::Core::Internal::MarkUnavailable, false).toBool());
    m_markUnavailableStart->setChecked(
        m_settings->fileValue(Settings::Core::Internal::MarkUnavailableStartup, false).toBool());
    m_useVariousCompilations->setChecked(m_settings->value<Settings::Core::UseVariousForCompilations>());
    m_saveRatings->setChecked(m_settings->value<Settings::Core::SaveRatingToMetadata>());
    m_savePlaycounts->setChecked(m_settings->value<Settings::Core::SavePlaycountToMetadata>());
}

void LibraryGeneralPageWidget::apply()
{
    m_model->processQueue();

    m_settings->fileSet(Settings::Core::Internal::LibraryRestrictTypes,
                        m_restrictTypes->text().split(u';', Qt::SkipEmptyParts));
    m_settings->fileSet(Settings::Core::Internal::LibraryExcludeTypes,
                        m_excludeTypes->text().split(u';', Qt::SkipEmptyParts));

    m_settings->set<Settings::Core::AutoRefresh>(m_autoRefresh->isChecked());
    m_settings->set<Settings::Core::Internal::MonitorLibraries>(m_monitorLibraries->isChecked());
    m_settings->fileSet(Settings::Core::Internal::MarkUnavailable, m_markUnavailable->isChecked());
    m_settings->fileSet(Settings::Core::Internal::MarkUnavailableStartup, m_markUnavailableStart->isChecked());
    m_settings->set<Settings::Core::UseVariousForCompilations>(m_useVariousCompilations->isChecked());
    m_settings->set<Settings::Core::SaveRatingToMetadata>(m_saveRatings->isChecked());
    m_settings->set<Settings::Core::SavePlaycountToMetadata>(m_savePlaycounts->isChecked());
}

void LibraryGeneralPageWidget::reset()
{
    m_settings->fileRemove(Settings::Core::Internal::LibraryRestrictTypes);
    m_settings->fileRemove(Settings::Core::Internal::LibraryExcludeTypes);

    m_settings->reset<Settings::Core::AutoRefresh>();
    m_settings->reset<Settings::Core::Internal::MonitorLibraries>();
    m_settings->fileRemove(Settings::Core::Internal::MarkUnavailable);
    m_settings->fileRemove(Settings::Core::Internal::MarkUnavailableStartup);
    m_settings->reset<Settings::Core::UseVariousForCompilations>();
    m_settings->reset<Settings::Core::SaveRatingToMetadata>();
    m_settings->reset<Settings::Core::SavePlaycountToMetadata>();
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
                                       MusicLibrary* library, SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
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
