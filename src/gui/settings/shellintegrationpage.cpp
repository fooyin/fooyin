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

#include "shellintegrationpage.h"

#include <core/coresettings.h>
#include <core/internalcoresettings.h>
#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>

namespace Fooyin {
class ShellIntegrationPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit ShellIntegrationPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QLineEdit* m_restrictTypes;
    QLineEdit* m_excludeTypes;
    QLineEdit* m_externalSortScript;
    QCheckBox* m_alwaysSend;
    QLineEdit* m_externalPlaylist;
};

ShellIntegrationPageWidget::ShellIntegrationPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_restrictTypes{new QLineEdit(this)}
    , m_excludeTypes{new QLineEdit(this)}
    , m_externalSortScript{new QLineEdit(this)}
    , m_alwaysSend{new QCheckBox(tr("Always send to playlist"), this)}
    , m_externalPlaylist{new QLineEdit(this)}
{
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

    auto* playlistGroup       = new QGroupBox(tr("Playlist"), this);
    auto* playlistGroupLayout = new QGridLayout(playlistGroup);

    auto* playlistLabel           = new QLabel(tr("Playlist name") + QStringLiteral(":"), this);
    auto* sortExternalScriptLabel = new QLabel(tr("Sort incoming tracks by") + u":", this);

    m_externalPlaylist->setToolTip(tr("When opening files, always send to playlist, replacing all existing tracks"));

    row = 0;
    playlistGroupLayout->addWidget(m_alwaysSend, row++, 0, 1, 2);
    playlistGroupLayout->addWidget(playlistLabel, row, 0);
    playlistGroupLayout->addWidget(m_externalPlaylist, row++, 1);
    playlistGroupLayout->addWidget(sortExternalScriptLabel, row, 0);
    playlistGroupLayout->addWidget(m_externalSortScript, row++, 1);
    playlistGroupLayout->setRowStretch(row, 1);
    playlistGroupLayout->setColumnStretch(1, 1);

    auto* mainLayout = new QGridLayout(this);

    row = 0;
    mainLayout->addWidget(fileTypesGroup, row++, 0);
    mainLayout->addWidget(playlistGroup, row, 0);
    mainLayout->setRowStretch(row, 1);
}

void ShellIntegrationPageWidget::load()
{
    const QStringList restrictExtensions
        = m_settings->fileValue(Settings::Core::Internal::ExternalRestrictTypes).toStringList();
    const QStringList excludeExtensions
        = m_settings->fileValue(Settings::Core::Internal::ExternalExcludeTypes).toStringList();

    m_restrictTypes->setText(restrictExtensions.join(u';'));
    m_excludeTypes->setText(excludeExtensions.join(u';'));

    m_alwaysSend->setChecked(m_settings->value<Settings::Core::OpenFilesSendTo>());
    m_externalPlaylist->setText(m_settings->value<Settings::Core::OpenFilesPlaylist>());
    m_externalSortScript->setText(m_settings->value<Settings::Core::ExternalSortScript>());
}

void ShellIntegrationPageWidget::apply()
{
    m_settings->fileSet(Settings::Core::Internal::ExternalRestrictTypes,
                        m_restrictTypes->text().split(u';', Qt::SkipEmptyParts));
    m_settings->fileSet(Settings::Core::Internal::ExternalExcludeTypes,
                        m_excludeTypes->text().split(u';', Qt::SkipEmptyParts));

    m_settings->set<Settings::Core::OpenFilesSendTo>(m_alwaysSend->isChecked());
    m_settings->set<Settings::Core::OpenFilesPlaylist>(m_externalPlaylist->text());
    m_settings->set<Settings::Core::ExternalSortScript>(m_externalSortScript->text());
}

void ShellIntegrationPageWidget::reset()
{
    m_settings->fileRemove(Settings::Core::Internal::ExternalRestrictTypes);
    m_settings->fileRemove(Settings::Core::Internal::ExternalExcludeTypes);

    m_settings->reset<Settings::Core::OpenFilesSendTo>();
    m_settings->reset<Settings::Core::OpenFilesPlaylist>();
    m_settings->reset<Settings::Core::ExternalSortScript>();
}

ShellIntegrationPage::ShellIntegrationPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::ShellIntegration);
    setName(tr("General"));
    setCategory({tr("Shell Integration")});
    setWidgetCreator([settings] { return new ShellIntegrationPageWidget(settings); });
}
} // namespace Fooyin

#include "moc_shellintegrationpage.cpp"
#include "shellintegrationpage.moc"
