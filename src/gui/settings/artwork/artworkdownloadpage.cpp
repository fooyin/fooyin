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

#include "artworkdownloadpage.h"

#include "internalguisettings.h"

#include <gui/guiconstants.h>
#include <gui/guipaths.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/stringutils.h>
#include <utils/utils.h>

#include <QButtonGroup>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>

namespace Fooyin {
class ArtworkDownloadPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit ArtworkDownloadPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void browseDestination() const;

    SettingsManager* m_settings;

    QRadioButton* m_manual;
    QRadioButton* m_autosave;
    QRadioButton* m_autosavePeriod;

    QRadioButton* m_embedded;
    QRadioButton* m_directory;

    QLineEdit* m_path;
    ScriptLineEdit* m_filename;
};

ArtworkDownloadPageWidget::ArtworkDownloadPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_manual{new QRadioButton(tr("Manual save"), this)}
    , m_autosave{new QRadioButton(tr("Autosave"), this)}
    , m_autosavePeriod{new QRadioButton(tr("Autosave after 60 seconds or 1/3 of track duration"), this)}
    , m_embedded{new QRadioButton(tr("Embed in file"), this)}
    , m_directory{new QRadioButton(tr("Save to directory"), this)}
    , m_path{new QLineEdit(this)}
    , m_filename{new ScriptLineEdit(this)}
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
    methodLayout->addWidget(m_embedded, row++, 0);
    methodLayout->addWidget(m_directory, row++, 0);

    auto* locationGroup  = new QGroupBox(tr("Save Location"), this);
    auto* locationLayout = new QGridLayout(locationGroup);

    auto* dirLabel      = new QLabel(tr("Directory") + u":", this);
    auto* filenameLabel = new QLabel(tr("Filename") + u":", this);

    row = 0;
    locationLayout->addWidget(dirLabel, row, 0);
    locationLayout->addWidget(m_path, row++, 1);
    locationLayout->addWidget(filenameLabel, row, 0);
    locationLayout->addWidget(m_filename, row++, 1);

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(schemeGroup, row++, 0);
    layout->addWidget(methodGroup, row++, 0);
    layout->addWidget(locationGroup, row++, 0);
    layout->setRowStretch(layout->rowCount(), 1);

    auto* browseAction = new QAction(Utils::iconFromTheme(::Fooyin::Constants::Icons::Options), {}, this);
    QObject::connect(browseAction, &QAction::triggered, this, &ArtworkDownloadPageWidget::browseDestination);
    m_path->addAction(browseAction, QLineEdit::TrailingPosition);
}

void ArtworkDownloadPageWidget::load()
{
    const auto saveScheme
        = static_cast<ArtworkSaveScheme>(m_settings->value<Settings::Gui::Internal::ArtworkSaveScheme>());
    m_manual->setChecked(saveScheme == ArtworkSaveScheme::Manual);
    m_autosave->setChecked(saveScheme == ArtworkSaveScheme::Autosave);
    m_autosavePeriod->setChecked(saveScheme == ArtworkSaveScheme::AutosavePeriod);

    const auto saveMethod
        = m_settings->value<Settings::Gui::Internal::ArtworkSaveMethods>().value<ArtworkSaveMethods>();
    if(saveMethod.contains(Track::Cover::Front)) {
        const auto& frontMethod = saveMethod.value(Track::Cover::Front);
        m_embedded->setChecked(frontMethod.method == ArtworkSaveMethod::Embedded);
        m_directory->setChecked(frontMethod.method == ArtworkSaveMethod::Directory);
        m_path->setText(frontMethod.dir);
        m_filename->setText(frontMethod.filename);
    }
}

void ArtworkDownloadPageWidget::apply()
{
    ArtworkSaveScheme saveScheme{ArtworkSaveScheme::Manual};
    if(m_manual->isChecked()) {
        saveScheme = ArtworkSaveScheme::Manual;
    }
    else if(m_autosave->isChecked()) {
        saveScheme = ArtworkSaveScheme::Autosave;
    }
    else {
        saveScheme = ArtworkSaveScheme::AutosavePeriod;
    }
    m_settings->set<Settings::Gui::Internal::ArtworkSaveScheme>(static_cast<int>(saveScheme));

    ArtworkSaveMethods saveMethods;
    auto& frontMethod = saveMethods[Track::Cover::Front];
    if(m_embedded->isChecked()) {
        frontMethod.method = ArtworkSaveMethod::Embedded;
    }
    else {
        frontMethod.method = ArtworkSaveMethod::Directory;
    }
    frontMethod.dir      = m_path->text();
    frontMethod.filename = m_filename->text();

    m_settings->set<Settings::Gui::Internal::ArtworkSaveMethods>(QVariant::fromValue(saveMethods));
}

void ArtworkDownloadPageWidget::reset()
{
    m_settings->reset<Settings::Gui::Internal::ArtworkSaveScheme>();
    m_settings->reset<Settings::Gui::Internal::ArtworkSaveMethods>();
    m_settings->reset<Settings::Gui::Internal::ArtworkAutoSearch>();
    m_settings->reset<Settings::Gui::Internal::ArtworkAlbumField>();
    m_settings->reset<Settings::Gui::Internal::ArtworkArtistField>();
    m_settings->reset<Settings::Gui::Internal::ArtworkMatchThreshold>();
}

void ArtworkDownloadPageWidget::browseDestination() const
{
    const QString path = !m_path->text().isEmpty() ? m_path->text() : QDir::homePath();
    const QString dir  = QFileDialog::getExistingDirectory(Utils::getMainWindow(), tr("Select Directory"), path);
    if(!dir.isEmpty()) {
        m_path->setText(dir);
    }
}

ArtworkDownloadPage::ArtworkDownloadPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::ArtworkDownload);
    setName(tr("Download"));
    setCategory({tr("Interface"), tr("Artwork")});
    setWidgetCreator([settings] { return new ArtworkDownloadPageWidget(settings); });
}
} // namespace Fooyin

#include "artworkdownloadpage.moc"
#include "moc_artworkdownloadpage.cpp"
