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

#include "artwork/artworkfinder.h"
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
#include <QTabWidget>

using namespace Qt::StringLiterals;

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
    void browseDestination(Track::Cover type) const;

    SettingsManager* m_settings;

    QRadioButton* m_manual;
    QRadioButton* m_autosave;
    QRadioButton* m_autosavePeriod;

    QTabWidget* m_coverTypes;
    QWidget* m_frontWidget;
    QWidget* m_backWidget;
    QWidget* m_artistWidget;

    struct CoverControls
    {
        QRadioButton* embedded;
        QRadioButton* directory;
        QLineEdit* path;
        ScriptLineEdit* filename;

        explicit CoverControls(QWidget* parent)
            : embedded{new QRadioButton(tr("Embed in file"), parent)}
            , directory{new QRadioButton(tr("Save to directory"), parent)}
            , path{new QLineEdit(parent)}
            , filename{new ScriptLineEdit(parent)}
        { }
    };

    std::map<Track::Cover, CoverControls> m_typeControls;
};

ArtworkDownloadPageWidget::ArtworkDownloadPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_manual{new QRadioButton(tr("Manual save"), this)}
    , m_autosave{new QRadioButton(tr("Autosave"), this)}
    , m_autosavePeriod{new QRadioButton(tr("Autosave after 60 seconds or 1/3 of track duration"), this)}
    , m_coverTypes{new QTabWidget(this)}
    , m_frontWidget{new QWidget(this)}
    , m_backWidget{new QWidget(this)}
    , m_artistWidget{new QWidget(this)}
{
    auto* schemeGroup  = new QGroupBox(tr("Save Scheme"), this);
    auto* schemeLayout = new QGridLayout(schemeGroup);

    int row{0};
    schemeLayout->addWidget(m_manual, row++, 0);
    schemeLayout->addWidget(m_autosave, row++, 0);
    schemeLayout->addWidget(m_autosavePeriod, row++, 0);

    m_coverTypes->setDocumentMode(true);
    m_coverTypes->addTab(m_frontWidget, tr("Front Cover"));
    m_coverTypes->addTab(m_backWidget, tr("Back Cover"));
    m_coverTypes->addTab(m_artistWidget, tr("Artist"));

    const auto addType = [this, &row](Track::Cover type, QWidget* widget) {
        auto* methodGroup  = new QGroupBox(tr("Save Method"), widget);
        auto* methodLayout = new QGridLayout(methodGroup);

        CoverControls& controls = m_typeControls.emplace(type, CoverControls{widget}).first->second;

        row = 0;
        methodLayout->addWidget(controls.embedded, row++, 0);
        methodLayout->addWidget(controls.directory, row++, 0);

        auto* locationGroup  = new QGroupBox(tr("Save Location"), widget);
        auto* locationLayout = new QGridLayout(locationGroup);

        auto* dirLabel      = new QLabel(tr("Directory") + u":", widget);
        auto* filenameLabel = new QLabel(tr("Filename") + u":", widget);

        row = 0;
        locationLayout->addWidget(dirLabel, row, 0);
        locationLayout->addWidget(controls.path, row++, 1);
        locationLayout->addWidget(filenameLabel, row, 0);
        locationLayout->addWidget(controls.filename, row++, 1);

        auto* typeLayout = new QGridLayout(widget);

        row = 0;
        typeLayout->addWidget(methodGroup, row++, 0);
        typeLayout->addWidget(locationGroup, row++, 0);

        auto* browseAction = new QAction(Utils::iconFromTheme(::Fooyin::Constants::Icons::Options), {}, widget);
        QObject::connect(browseAction, &QAction::triggered, widget, [this, type]() { browseDestination(type); });
        controls.path->addAction(browseAction, QLineEdit::TrailingPosition);
    };

    addType(Track::Cover::Front, m_frontWidget);
    addType(Track::Cover::Back, m_backWidget);
    addType(Track::Cover::Artist, m_artistWidget);

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(schemeGroup, row++, 0);
    layout->addWidget(m_coverTypes, row++, 0);
    layout->setRowStretch(layout->rowCount(), 1);
}

void ArtworkDownloadPageWidget::load()
{
    const auto saveScheme
        = static_cast<ArtworkSaveScheme>(m_settings->value<Settings::Gui::Internal::ArtworkSaveScheme>());
    m_manual->setChecked(saveScheme == ArtworkSaveScheme::Manual);
    m_autosave->setChecked(saveScheme == ArtworkSaveScheme::Autosave);
    m_autosavePeriod->setChecked(saveScheme == ArtworkSaveScheme::AutosavePeriod);

    const auto saveMethods
        = m_settings->value<Settings::Gui::Internal::ArtworkSaveMethods>().value<ArtworkSaveMethods>();

    for(const auto& [type, controls] : m_typeControls) {
        const auto& coverMethod = saveMethods.value(type);
        controls.embedded->setChecked(coverMethod.method == ArtworkSaveMethod::Embedded);
        controls.directory->setChecked(coverMethod.method == ArtworkSaveMethod::Directory);
        controls.path->setText(coverMethod.dir);
        controls.filename->setText(coverMethod.filename);
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

    for(const auto& [type, controls] : m_typeControls) {
        auto& coverMethod = saveMethods[type];
        coverMethod.method
            = controls.embedded->isChecked() ? ArtworkSaveMethod::Embedded : ArtworkSaveMethod::Directory;
        coverMethod.dir      = controls.path->text();
        coverMethod.filename = controls.filename->text();
    }

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

void ArtworkDownloadPageWidget::browseDestination(Track::Cover type) const
{
    auto* lineEdit     = m_typeControls.at(type).path;
    const QString path = !lineEdit->text().isEmpty() ? lineEdit->text() : QDir::homePath();
    const QString dir  = QFileDialog::getExistingDirectory(Utils::getMainWindow(), tr("Select Directory"), path);
    if(!dir.isEmpty()) {
        lineEdit->setText(dir);
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
