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

#include "playlistguipage.h"

#include "internalguisettings.h"
#include "widgets/pixmapfadecontroller.h"

#include <gui/guiconstants.h>
#include <gui/guiutils.h>
#include <gui/iconloader.h>
#include <gui/widgets/slidereditor.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
void addComboItem(QComboBox* combo, const QString& text, int data, const QString& tooltip = {})
{
    combo->addItem(text, data);
    if(!tooltip.isEmpty()) {
        combo->setItemData(combo->count() - 1, tooltip, Qt::ToolTipRole);
    }
}
} // namespace

class PlaylistAppearancePageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit PlaylistAppearancePageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void browseBackgroundImage();
    void updateBackgroundControls();

    SettingsManager* m_settings;

    QCheckBox* m_scrollBars;
    QCheckBox* m_header;
    QCheckBox* m_altColours;
    QSpinBox* m_imagePadding;
    QSpinBox* m_imagePaddingTop;
    QComboBox* m_backgroundImage;
    QLabel* m_backgroundCoverTypeLabel;
    QComboBox* m_backgroundCoverType;
    QLabel* m_customImageLabel;
    QLineEdit* m_customImage;
    QComboBox* m_backgroundScaling;
    QComboBox* m_backgroundPosition;
    QSpinBox* m_maxCoverSize;
    SliderEditor* m_blur;
    SliderEditor* m_opacity;
    SliderEditor* m_fadeDuration;
};

PlaylistAppearancePageWidget::PlaylistAppearancePageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_scrollBars{new QCheckBox(tr("Show scrollbar"), this)}
    , m_header{new QCheckBox(tr("Show header"), this)}
    , m_altColours{new QCheckBox(tr("Alternating row colours"), this)}
    , m_imagePadding{new QSpinBox(this)}
    , m_imagePaddingTop{new QSpinBox(this)}
    , m_backgroundImage{new QComboBox(this)}
    , m_backgroundCoverTypeLabel{new QLabel(tr("Artwork type") + u":"_s, this)}
    , m_backgroundCoverType{new QComboBox(this)}
    , m_customImageLabel{new QLabel(tr("File") + u":"_s, this)}
    , m_customImage{new QLineEdit(this)}
    , m_backgroundScaling{new QComboBox(this)}
    , m_backgroundPosition{new QComboBox(this)}
    , m_maxCoverSize{new QSpinBox(this)}
    , m_blur{new SliderEditor(tr("Blur"), this)}
    , m_opacity{new SliderEditor(tr("Opacity"), this)}
    , m_fadeDuration{new SliderEditor(tr("Fade length"), this)}
{
    auto* browseAction = new QAction({}, this);
    Gui::setThemeIcon(browseAction, Constants::Icons::Options);
    browseAction->setToolTip(tr("Choose a custom background image file"));
    QObject::connect(browseAction, &QAction::triggered, this, &PlaylistAppearancePageWidget::browseBackgroundImage);
    m_customImage->addAction(browseAction, QLineEdit::TrailingPosition);

    m_imagePadding->setMinimum(0);
    m_imagePadding->setMaximum(100);
    m_imagePadding->setSuffix(u" px"_s);

    m_imagePaddingTop->setMinimum(0);
    m_imagePaddingTop->setMaximum(100);
    m_imagePaddingTop->setSuffix(u" px"_s);

    addComboItem(m_backgroundImage, tr("No background image"), static_cast<int>(PlaylistBgImage::None));
    addComboItem(m_backgroundImage, tr("Current track artwork"), static_cast<int>(PlaylistBgImage::AlbumCover),
                 tr("Use the currently playing track's artwork as the playlist background"));
    addComboItem(m_backgroundImage, tr("Custom image"), static_cast<int>(PlaylistBgImage::Custom),
                 tr("Use the selected image file as the playlist background"));

    addComboItem(m_backgroundCoverType, tr("Front"), static_cast<int>(Track::Cover::Front),
                 tr("Use the front cover for current track artwork"));
    addComboItem(m_backgroundCoverType, tr("Back"), static_cast<int>(Track::Cover::Back),
                 tr("Use the back cover for current track artwork"));
    addComboItem(m_backgroundCoverType, tr("Artist"), static_cast<int>(Track::Cover::Artist),
                 tr("Use the artist picture for current track artwork"));

    addComboItem(m_backgroundScaling, tr("Scaled and cropped"), static_cast<int>(PlaylistBgScaling::ScaledAndCropped),
                 tr("Fill the playlist area while preserving proportions; edges may be cropped"));
    addComboItem(m_backgroundScaling, tr("Scaled"), static_cast<int>(PlaylistBgScaling::Scaled),
                 tr("Stretch the image to fill the playlist area; proportions may change"));
    addComboItem(m_backgroundScaling, tr("Scaled, keep proportions"),
                 static_cast<int>(PlaylistBgScaling::ScaledKeepProportions),
                 tr("Fit the whole image inside the playlist area without cropping"));
    addComboItem(m_backgroundScaling, tr("Original size"), static_cast<int>(PlaylistBgScaling::OriginalSize),
                 tr("Draw the image at its original size, optionally limited by maximum size"));

    addComboItem(m_backgroundPosition, tr("Top left"), static_cast<int>(PlaylistBgImagePosition::TopLeft));
    addComboItem(m_backgroundPosition, tr("Top"), static_cast<int>(PlaylistBgImagePosition::Top));
    addComboItem(m_backgroundPosition, tr("Top right"), static_cast<int>(PlaylistBgImagePosition::TopRight));
    addComboItem(m_backgroundPosition, tr("Left"), static_cast<int>(PlaylistBgImagePosition::Left));
    addComboItem(m_backgroundPosition, tr("Middle"), static_cast<int>(PlaylistBgImagePosition::Middle));
    addComboItem(m_backgroundPosition, tr("Right"), static_cast<int>(PlaylistBgImagePosition::Right));
    addComboItem(m_backgroundPosition, tr("Bottom left"), static_cast<int>(PlaylistBgImagePosition::BottomLeft));
    addComboItem(m_backgroundPosition, tr("Bottom"), static_cast<int>(PlaylistBgImagePosition::Bottom));
    addComboItem(m_backgroundPosition, tr("Bottom right"), static_cast<int>(PlaylistBgImagePosition::BottomRight));

    m_maxCoverSize->setMinimum(0);
    m_maxCoverSize->setMaximum(4096);
    m_maxCoverSize->setSuffix(u" px"_s);
    m_maxCoverSize->setSpecialValueText(tr("Disabled"));

    m_blur->setRange(0, 100);
    m_opacity->setRange(0, 100);
    m_fadeDuration->setRange(0, PixmapFadeController::MaxDurationMs);
    m_fadeDuration->setSingleStep(50);

    m_blur->setSuffix(u" px"_s);
    m_opacity->setSuffix(u" %"_s);
    m_fadeDuration->setSuffix(u" ms"_s);
    m_fadeDuration->addSpecialValue(0, tr("Disabled"));

    m_customImage->setToolTip(tr("Path to the custom background image"));
    m_backgroundImage->setToolTip(tr("Select which image source to use for the playlist background"));
    m_backgroundCoverType->setToolTip(tr("Select which artwork type to use for current track artwork"));
    m_backgroundScaling->setToolTip(tr("Controls how the background image is scaled to the playlist area"));
    m_backgroundPosition->setToolTip(tr("Alignment for original-size background images"));
    m_maxCoverSize->setToolTip(tr("Maximum width or height for original-size background images"));
    m_blur->setToolTip(tr("Applies blur to the background image"));
    m_opacity->setToolTip(tr("Controls how strongly the background image is shown"));
    m_fadeDuration->setToolTip(tr("Duration for fading between background images; set to 0 to disable"));

    QObject::connect(m_backgroundImage, &QComboBox::currentIndexChanged, this,
                     &PlaylistAppearancePageWidget::updateBackgroundControls);
    QObject::connect(m_backgroundScaling, &QComboBox::currentIndexChanged, this,
                     &PlaylistAppearancePageWidget::updateBackgroundControls);

    int row{0};

    auto* appearance       = new QGroupBox(tr("Appearance"), this);
    auto* appearanceLayout = new QGridLayout(appearance);

    row = 0;
    appearanceLayout->addWidget(m_scrollBars, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_header, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_altColours, row++, 0, 1, 2);
    appearanceLayout->addWidget(Gui::createSectionHeader(tr("Image padding"), this), row++, 0, 1, 3);
    appearanceLayout->addWidget(new QLabel(tr("Left/Right") + u":"_s, this), row, 0);
    appearanceLayout->addWidget(m_imagePadding, row++, 1);
    appearanceLayout->addWidget(new QLabel(tr("Top") + u":"_s, this), row, 0);
    appearanceLayout->addWidget(m_imagePaddingTop, row++, 1);
    appearanceLayout->setColumnStretch(2, 1);

    auto* background       = new QGroupBox(tr("Background Image"), this);
    auto* backgroundLayout = new QGridLayout(background);

    row = 0;
    backgroundLayout->addWidget(Gui::createSectionHeader(tr("Image source"), this), row++, 0, 1, 2);
    backgroundLayout->addWidget(new QLabel(tr("Source") + u":"_s, this), row, 0);
    backgroundLayout->addWidget(m_backgroundImage, row++, 1);
    backgroundLayout->addWidget(m_backgroundCoverTypeLabel, row, 0);
    backgroundLayout->addWidget(m_backgroundCoverType, row, 1);
    backgroundLayout->addWidget(m_customImageLabel, row, 0);
    backgroundLayout->addWidget(m_customImage, row++, 1);
    backgroundLayout->addWidget(Gui::createSectionHeader(tr("Layout"), this), row++, 0, 1, 2);
    backgroundLayout->addWidget(new QLabel(tr("Scale mode") + u":"_s, this), row, 0);
    backgroundLayout->addWidget(m_backgroundScaling, row++, 1);
    backgroundLayout->addWidget(new QLabel(tr("Alignment") + u":"_s, this), row, 0);
    backgroundLayout->addWidget(m_backgroundPosition, row++, 1);
    backgroundLayout->addWidget(new QLabel(tr("Maximum size") + u":"_s, this), row, 0);
    backgroundLayout->addWidget(m_maxCoverSize, row++, 1);
    backgroundLayout->addWidget(Gui::createSectionHeader(tr("Effects"), this), row++, 0, 1, 2);
    backgroundLayout->addWidget(m_blur, row++, 0, 1, 2);
    backgroundLayout->addWidget(m_opacity, row++, 0, 1, 2);
    backgroundLayout->addWidget(m_fadeDuration, row++, 0, 1, 2);
    backgroundLayout->setColumnStretch(1, 1);

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(appearance, 0, 0);
    mainLayout->addWidget(background, 1, 0);
    mainLayout->setRowStretch(2, 1);
}

void PlaylistAppearancePageWidget::load()
{
    m_scrollBars->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistScrollBar>());
    m_header->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistHeader>());
    m_altColours->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistAltColours>());
    m_imagePadding->setValue(m_settings->value<Settings::Gui::Internal::PlaylistImagePadding>());
    m_imagePaddingTop->setValue(m_settings->value<Settings::Gui::Internal::PlaylistImagePaddingTop>());
    m_backgroundImage->setCurrentIndex(
        m_backgroundImage->findData(m_settings->value<Settings::Gui::Internal::PlaylistBackgroundImageMode>()));
    m_backgroundCoverType->setCurrentIndex(
        m_backgroundCoverType->findData(m_settings->value<Settings::Gui::Internal::PlaylistBackgroundCoverType>()));
    m_customImage->setText(m_settings->value<Settings::Gui::Internal::PlaylistBackgroundCustomImage>());
    m_backgroundScaling->setCurrentIndex(
        m_backgroundScaling->findData(m_settings->value<Settings::Gui::Internal::PlaylistBackgroundScaling>()));
    m_backgroundPosition->setCurrentIndex(
        m_backgroundPosition->findData(m_settings->value<Settings::Gui::Internal::PlaylistBackgroundPosition>()));
    m_maxCoverSize->setValue(m_settings->value<Settings::Gui::Internal::PlaylistBackgroundMaxSize>());
    m_blur->setValue(m_settings->value<Settings::Gui::Internal::PlaylistBackgroundBlur>());
    m_opacity->setValue(m_settings->value<Settings::Gui::Internal::PlaylistBackgroundOpacity>());
    m_fadeDuration->setValue(m_settings->value<Settings::Gui::Internal::PlaylistBackgroundFadeDuration>());
    updateBackgroundControls();
}

void PlaylistAppearancePageWidget::apply()
{
    m_settings->set<Settings::Gui::Internal::PlaylistScrollBar>(m_scrollBars->isChecked());
    m_settings->set<Settings::Gui::Internal::PlaylistHeader>(m_header->isChecked());
    m_settings->set<Settings::Gui::Internal::PlaylistAltColours>(m_altColours->isChecked());
    m_settings->set<Settings::Gui::Internal::PlaylistImagePadding>(m_imagePadding->value());
    m_settings->set<Settings::Gui::Internal::PlaylistImagePaddingTop>(m_imagePaddingTop->value());
    m_settings->set<Settings::Gui::Internal::PlaylistBackgroundImageMode>(m_backgroundImage->currentData().toInt());
    m_settings->set<Settings::Gui::Internal::PlaylistBackgroundCoverType>(m_backgroundCoverType->currentData().toInt());
    m_settings->set<Settings::Gui::Internal::PlaylistBackgroundCustomImage>(m_customImage->text());
    m_settings->set<Settings::Gui::Internal::PlaylistBackgroundScaling>(m_backgroundScaling->currentData().toInt());
    m_settings->set<Settings::Gui::Internal::PlaylistBackgroundPosition>(m_backgroundPosition->currentData().toInt());
    m_settings->set<Settings::Gui::Internal::PlaylistBackgroundMaxSize>(m_maxCoverSize->value());
    m_settings->set<Settings::Gui::Internal::PlaylistBackgroundBlur>(m_blur->value());
    m_settings->set<Settings::Gui::Internal::PlaylistBackgroundOpacity>(m_opacity->value());
    m_settings->set<Settings::Gui::Internal::PlaylistBackgroundFadeDuration>(m_fadeDuration->value());
}

void PlaylistAppearancePageWidget::reset()
{
    m_settings->reset<Settings::Gui::Internal::PlaylistScrollBar>();
    m_settings->reset<Settings::Gui::Internal::PlaylistHeader>();
    m_settings->reset<Settings::Gui::Internal::PlaylistAltColours>();
    m_settings->reset<Settings::Gui::Internal::PlaylistImagePadding>();
    m_settings->reset<Settings::Gui::Internal::PlaylistImagePaddingTop>();
    m_settings->reset<Settings::Gui::Internal::PlaylistBackgroundImageMode>();
    m_settings->reset<Settings::Gui::Internal::PlaylistBackgroundCoverType>();
    m_settings->reset<Settings::Gui::Internal::PlaylistBackgroundCustomImage>();
    m_settings->reset<Settings::Gui::Internal::PlaylistBackgroundScaling>();
    m_settings->reset<Settings::Gui::Internal::PlaylistBackgroundPosition>();
    m_settings->reset<Settings::Gui::Internal::PlaylistBackgroundMaxSize>();
    m_settings->reset<Settings::Gui::Internal::PlaylistBackgroundBlur>();
    m_settings->reset<Settings::Gui::Internal::PlaylistBackgroundOpacity>();
    m_settings->reset<Settings::Gui::Internal::PlaylistBackgroundFadeDuration>();
}

void PlaylistAppearancePageWidget::browseBackgroundImage()
{
    const QString path = !m_customImage->text().isEmpty() ? m_customImage->text() : QDir::homePath();
    const QString file = QFileDialog::getOpenFileName(this, tr("Select Background Image"), path,
                                                      tr("Images (*.png *.jpg *.jpeg *.bmp *.webp)"), nullptr,
                                                      QFileDialog::DontResolveSymlinks);
    if(!file.isEmpty()) {
        m_customImage->setText(file);
    }
}

void PlaylistAppearancePageWidget::updateBackgroundControls()
{
    const int imageMode    = m_backgroundImage->currentData().toInt();
    const bool hasImage    = imageMode != static_cast<int>(PlaylistBgImage::None);
    const bool coverImage  = imageMode == static_cast<int>(PlaylistBgImage::AlbumCover);
    const bool customImage = imageMode == static_cast<int>(PlaylistBgImage::Custom);
    const bool originalSize
        = m_backgroundScaling->currentData().toInt() == static_cast<int>(PlaylistBgScaling::OriginalSize);

    m_customImage->setEnabled(customImage);
    m_backgroundCoverType->setEnabled(coverImage);
    m_backgroundCoverType->setVisible(coverImage);
    m_backgroundCoverTypeLabel->setVisible(coverImage);
    m_customImage->setVisible(customImage);
    m_customImageLabel->setVisible(customImage);
    m_backgroundScaling->setEnabled(hasImage);
    m_backgroundPosition->setEnabled(hasImage && originalSize);
    m_maxCoverSize->setEnabled(hasImage && originalSize);
    m_blur->setEnabled(hasImage);
    m_opacity->setEnabled(hasImage);
    m_fadeDuration->setEnabled(hasImage);
}

PlaylistGuiPage::PlaylistGuiPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::PlaylistAppearance);
    setName(tr("Appearance"));
    setCategory({tr("Playlist")});
    setWidgetCreator([settings] { return new PlaylistAppearancePageWidget(settings); });
}
} // namespace Fooyin

#include "moc_playlistguipage.cpp"
#include "playlistguipage.moc"
