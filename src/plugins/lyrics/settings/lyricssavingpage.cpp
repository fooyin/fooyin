/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "lyricssavingpage.h"

#include "lyricsconstants.h"
#include "lyricssaver.h"
#include "lyricssettings.h"

#include <gui/guiconstants.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QCheckBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMainWindow>
#include <QRadioButton>

using namespace Qt::StringLiterals;

namespace Fooyin::Lyrics {
class LyricsSavingPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit LyricsSavingPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void browseDestination() const;

    SettingsManager* m_settings;

    QRadioButton* m_manual;
    QRadioButton* m_autosave;
    QRadioButton* m_autosavePeriod;

    QRadioButton* m_tag;
    QRadioButton* m_directory;

    QRadioButton* m_noPreference;
    QRadioButton* m_saveSynced;
    QRadioButton* m_saveUnsynced;

    ScriptLineEdit* m_syncedTag;
    ScriptLineEdit* m_unyncedTag;
    QLineEdit* m_path;
    ScriptLineEdit* m_filename;

    QCheckBox* m_collapse;
    QCheckBox* m_metadata;
};

LyricsSavingPageWidget::LyricsSavingPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_manual{new QRadioButton(tr("Manual save"), this)}
    , m_autosave{new QRadioButton(tr("Autosave"), this)}
    , m_autosavePeriod{new QRadioButton(tr("Autosave after 60 seconds or 1/3 of track duration"), this)}
    , m_tag{new QRadioButton(tr("Save to metadata tag"), this)}
    , m_directory{new QRadioButton(tr("Save to directory"), this)}
    , m_noPreference{new QRadioButton(tr("Save all"), this)}
    , m_saveSynced{new QRadioButton(tr("Synced only"), this)}
    , m_saveUnsynced{new QRadioButton(tr("Unsynced only"), this)}
    , m_syncedTag{new ScriptLineEdit(this)}
    , m_unyncedTag{new ScriptLineEdit(this)}
    , m_path{new QLineEdit(this)}
    , m_filename{new ScriptLineEdit(this)}
    , m_collapse{new QCheckBox(tr("Collapse duplicate lines"), this)}
    , m_metadata{new QCheckBox(tr("Save metadata"), this)}
{
    auto* schemeGroup  = new QGroupBox(tr("Save Scheme"), this);
    auto* schemeLayout = new QGridLayout(schemeGroup);

    int row{0};
    schemeLayout->addWidget(m_manual, row++, 0);
    schemeLayout->addWidget(m_autosave, row++, 0);
    schemeLayout->addWidget(m_autosavePeriod, row++, 0);

    auto* methodGroup  = new QGroupBox(tr("Save Method"), this);
    auto* methodLayout = new QGridLayout(methodGroup);

    row = 0;
    methodLayout->addWidget(m_tag, row++, 0);
    methodLayout->addWidget(m_directory, row++, 0);

    auto* preferGroup  = new QGroupBox(tr("Autosave Preference"), this);
    auto* preferLayout = new QGridLayout(preferGroup);

    row = 0;
    preferLayout->addWidget(m_noPreference, row++, 0);
    preferLayout->addWidget(m_saveSynced, row++, 0);
    preferLayout->addWidget(m_saveUnsynced, row++, 0);

    auto* locationGroup  = new QGroupBox(tr("Save Location"), this);
    auto* locationLayout = new QGridLayout(locationGroup);

    auto* syncedLabel   = new QLabel(tr("Synced lyric tag") + ":"_L1, this);
    auto* unsyncedLabel = new QLabel(tr("Unsynced lyric tag") + ":"_L1, this);
    auto* dirLabel      = new QLabel(tr("Directory") + ":"_L1, this);
    auto* filenameLabel = new QLabel(tr("Filename") + ":"_L1, this);

    row = 0;
    locationLayout->addWidget(syncedLabel, row, 0);
    locationLayout->addWidget(m_syncedTag, row++, 1);
    locationLayout->addWidget(unsyncedLabel, row, 0);
    locationLayout->addWidget(m_unyncedTag, row++, 1);
    locationLayout->addWidget(dirLabel, row, 0);
    locationLayout->addWidget(m_path, row++, 1);
    locationLayout->addWidget(filenameLabel, row, 0);
    locationLayout->addWidget(m_filename, row++, 1);

    auto* formatGroup  = new QGroupBox(tr("Save Format"), this);
    auto* formatLayout = new QGridLayout(formatGroup);

    row = 0;
    formatLayout->addWidget(m_collapse, row++, 0);
    formatLayout->addWidget(m_metadata, row++, 0);

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(schemeGroup, row, 0);
    layout->addWidget(preferGroup, row++, 1);
    layout->addWidget(methodGroup, row++, 0, 1, 2);
    layout->addWidget(locationGroup, row++, 0, 1, 2);
    layout->addWidget(formatGroup, row++, 0, 1, 2);
    layout->setRowStretch(layout->rowCount(), 1);

    auto* browseAction = new QAction(Utils::iconFromTheme(::Fooyin::Constants::Icons::Options), {}, this);
    QObject::connect(browseAction, &QAction::triggered, this, &LyricsSavingPageWidget::browseDestination);
    m_path->addAction(browseAction, QLineEdit::TrailingPosition);

    const auto updateVisibility = [this, syncedLabel, unsyncedLabel, dirLabel, filenameLabel](bool tag) {
        syncedLabel->setVisible(tag);
        m_syncedTag->setVisible(tag);
        unsyncedLabel->setVisible(tag);
        m_unyncedTag->setVisible(tag);

        dirLabel->setVisible(!tag);
        m_path->setVisible(!tag);
        filenameLabel->setVisible(!tag);
        m_filename->setVisible(!tag);
    };

    QObject::connect(m_tag, &QRadioButton::toggled, this,
                     [updateVisibility](const bool checked) { updateVisibility(checked); });
    QObject::connect(m_directory, &QRadioButton::toggled, this,
                     [updateVisibility](const bool checked) { updateVisibility(!checked); });
}

void LyricsSavingPageWidget::load()
{
    const auto saveScheme = static_cast<SaveScheme>(m_settings->value<Settings::Lyrics::SaveScheme>());
    m_manual->setChecked(saveScheme == SaveScheme::Manual);
    m_autosave->setChecked(saveScheme == SaveScheme::Autosave);
    m_autosavePeriod->setChecked(saveScheme == SaveScheme::AutosavePeriod);

    const auto saveMethod = static_cast<SaveMethod>(m_settings->value<Settings::Lyrics::SaveMethod>());
    m_tag->setChecked(saveMethod == SaveMethod::Tag);
    m_directory->setChecked(saveMethod == SaveMethod::Directory);

    const auto savePrefer = static_cast<SavePrefer>(m_settings->value<Settings::Lyrics::SavePrefer>());
    m_noPreference->setChecked(savePrefer == SavePrefer::None);
    m_saveSynced->setChecked(savePrefer == SavePrefer::Synced);
    m_saveUnsynced->setChecked(savePrefer == SavePrefer::Unsynced);

    m_syncedTag->setText(m_settings->value<Settings::Lyrics::SaveSyncedTag>());
    m_unyncedTag->setText(m_settings->value<Settings::Lyrics::SaveUnsyncedTag>());

    m_path->setText(m_settings->value<Settings::Lyrics::SaveDir>());
    m_filename->setText(m_settings->value<Settings::Lyrics::SaveFilename>());

    const auto opts = static_cast<LyricsSaver::SaveOptions>(m_settings->value<Settings::Lyrics::SaveOptions>());
    m_collapse->setChecked(opts & LyricsSaver::Collapse);
    m_metadata->setChecked(opts & LyricsSaver::Metadata);
}

void LyricsSavingPageWidget::apply()
{
    SaveScheme saveScheme{SaveScheme::Manual};
    if(m_manual->isChecked()) {
        saveScheme = SaveScheme::Manual;
    }
    else if(m_autosave->isChecked()) {
        saveScheme = SaveScheme::Autosave;
    }
    else {
        saveScheme = SaveScheme::AutosavePeriod;
    }
    m_settings->set<Settings::Lyrics::SaveScheme>(static_cast<int>(saveScheme));

    SaveMethod saveMethod{SaveMethod::Tag};
    if(m_tag->isChecked()) {
        saveMethod = SaveMethod::Tag;
    }
    else {
        saveMethod = SaveMethod::Directory;
    }
    m_settings->set<Settings::Lyrics::SaveMethod>(static_cast<int>(saveMethod));

    SavePrefer savePrefer{SavePrefer::None};
    if(m_noPreference->isChecked()) {
        savePrefer = SavePrefer::None;
    }
    else if(m_saveSynced->isChecked()) {
        savePrefer = SavePrefer::Synced;
    }
    else {
        savePrefer = SavePrefer::Unsynced;
    }
    m_settings->set<Settings::Lyrics::SavePrefer>(static_cast<int>(savePrefer));

    m_settings->set<Settings::Lyrics::SaveSyncedTag>(m_syncedTag->text());
    m_settings->set<Settings::Lyrics::SaveUnsyncedTag>(m_unyncedTag->text());

    m_settings->set<Settings::Lyrics::SaveDir>(m_path->text());
    m_settings->set<Settings::Lyrics::SaveFilename>(m_filename->text());

    LyricsSaver::SaveOptions opts;
    if(m_collapse->isChecked()) {
        opts |= LyricsSaver::Collapse;
    }
    if(m_metadata->isChecked()) {
        opts |= LyricsSaver::Metadata;
    }
    m_settings->set<Settings::Lyrics::SaveOptions>(static_cast<int>(opts));
}

void LyricsSavingPageWidget::reset()
{
    m_settings->reset<Settings::Lyrics::SaveScheme>();
    m_settings->reset<Settings::Lyrics::SaveMethod>();
    m_settings->reset<Settings::Lyrics::SavePrefer>();
    m_settings->reset<Settings::Lyrics::SaveSyncedTag>();
    m_settings->reset<Settings::Lyrics::SaveUnsyncedTag>();
    m_settings->reset<Settings::Lyrics::SaveDir>();
    m_settings->reset<Settings::Lyrics::SaveFilename>();
    m_settings->reset<Settings::Lyrics::SaveOptions>();
}

void LyricsSavingPageWidget::browseDestination() const
{
    const QString path = !m_path->text().isEmpty() ? m_path->text() : QDir::homePath();
    const QString dir  = QFileDialog::getExistingDirectory(Utils::getMainWindow(), tr("Select Directory"), path);
    if(!dir.isEmpty()) {
        m_path->setText(dir);
    }
}

LyricsSavingPage::LyricsSavingPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::LyricsSaving);
    setName(tr("Saving"));
    setCategory({tr("Lyrics")});
    setWidgetCreator([settings] { return new LyricsSavingPageWidget(settings); });
}
} // namespace Fooyin::Lyrics

#include "lyricssavingpage.moc"
#include "moc_lyricssavingpage.cpp"
