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

#include "artworkpage.h"

#include "internalguisettings.h"

#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>

#include <QGridLayout>
#include <QPlainTextEdit>
#include <QTabWidget>

namespace Fooyin {
class ArtworkPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit ArtworkPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QTabWidget* m_coverPaths;
    QPlainTextEdit* m_frontCovers;
    QPlainTextEdit* m_backCovers;
    QPlainTextEdit* m_artistCovers;
};

ArtworkPageWidget::ArtworkPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_coverPaths{new QTabWidget(this)}
    , m_frontCovers{new QPlainTextEdit(this)}
    , m_backCovers{new QPlainTextEdit(this)}
    , m_artistCovers{new QPlainTextEdit(this)}
{
    auto* layout = new QGridLayout(this);

    m_coverPaths->addTab(m_frontCovers, tr("Front Cover"));
    m_coverPaths->addTab(m_backCovers, tr("Back Cover"));
    m_coverPaths->addTab(m_artistCovers, tr("Artist"));

    layout->addWidget(m_coverPaths, 0, 0);
}

void ArtworkPageWidget::load()
{
    const auto paths = m_settings->value<Settings::Gui::Internal::TrackCoverPaths>().value<GuiSettings::CoverPaths>();

    m_frontCovers->setPlainText(paths.frontCoverPaths.join(QStringLiteral("\n")));
    m_backCovers->setPlainText(paths.backCoverPaths.join(QStringLiteral("\n")));
    m_artistCovers->setPlainText(paths.artistPaths.join(QStringLiteral("\n")));
}

void ArtworkPageWidget::apply()
{
    GuiSettings::CoverPaths paths;

    paths.frontCoverPaths = m_frontCovers->toPlainText().split(QStringLiteral("\n"), Qt::SkipEmptyParts);
    paths.backCoverPaths  = m_backCovers->toPlainText().split(QStringLiteral("\n"), Qt::SkipEmptyParts);
    paths.artistPaths     = m_artistCovers->toPlainText().split(QStringLiteral("\n"), Qt::SkipEmptyParts);

    m_settings->set<Settings::Gui::Internal::TrackCoverPaths>(QVariant::fromValue(paths));
}

void ArtworkPageWidget::reset()
{
    m_settings->reset<Settings::Gui::Internal::TrackCoverPaths>();
}

ArtworkPage::ArtworkPage(SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::Artwork);
    setName(tr("Artwork"));
    setCategory({tr("Interface"), tr("Artwork")});
    setWidgetCreator([settings] { return new ArtworkPageWidget(settings); });
}
} // namespace Fooyin

#include "artworkpage.moc"
#include "moc_artworkpage.cpp"
