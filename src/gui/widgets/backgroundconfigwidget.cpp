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

#include <gui/widgets/backgroundconfigwidget.h>

#include "pixmapfadecontroller.h"

#include <core/track.h>
#include <gui/guiconstants.h>
#include <gui/guiutils.h>
#include <gui/iconloader.h>
#include <gui/widgets/hintlabel.h>
#include <gui/widgets/slidereditor.h>

#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
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

class BackgroundConfigWidgetPrivate
{
public:
    explicit BackgroundConfigWidgetPrivate(BackgroundConfigWidget* self)
        : m_self{self}
        , m_backgroundImage{new QComboBox(self)}
        , m_globalHint{new HintLabel(
              BackgroundConfigWidget::tr("Uses the image and appearance settings from Interface → "
                                         "Background, aligned across the main window."),
              self)}
        , m_backgroundCoverTypeLabel{new QLabel(BackgroundConfigWidget::tr("Artwork type") + u":"_s, self)}
        , m_backgroundCoverType{new QComboBox(self)}
        , m_backgroundTrackPreferenceLabel{new QLabel(BackgroundConfigWidget::tr("Preferred track") + u":"_s, self)}
        , m_backgroundTrackPreference{new QComboBox(self)}
        , m_customImageLabel{new QLabel(BackgroundConfigWidget::tr("File") + u":"_s, self)}
        , m_customImage{new QLineEdit(self)}
        , m_backgroundScaling{new QComboBox(self)}
        , m_backgroundPosition{new QComboBox(self)}
        , m_backgroundMaxSize{new QSpinBox(self)}
        , m_backgroundBlur{new SliderEditor(BackgroundConfigWidget::tr("Blur"), self)}
        , m_backgroundOpacity{new SliderEditor(BackgroundConfigWidget::tr("Opacity"), self)}
        , m_backgroundFadeDuration{new SliderEditor(BackgroundConfigWidget::tr("Fade length"), self)}
        , m_options{BackgroundConfigWidget::TrackPreference | BackgroundConfigWidget::FadeDuration}
    { }

    void browseBackgroundImage() const
    {
        const QString initialPath = m_customImage->text().isEmpty() ? QDir::homePath() : m_customImage->text();
        const QString filename    = QFileDialog::getOpenFileName(
            m_self, BackgroundConfigWidget::tr("Choose Background Image"), initialPath,
            BackgroundConfigWidget::tr("Images (*.png *.jpg *.jpeg *.webp *.bmp);;All files (*)"), nullptr,
            QFileDialog::DontResolveSymlinks);
        if(!filename.isEmpty()) {
            m_customImage->setText(filename);
        }
    }

    void updateBackgroundControls() const
    {
        const auto imageMode    = static_cast<BackgroundImageMode>(m_backgroundImage->currentData().toInt());
        const bool globalImage  = imageMode == BackgroundImageMode::Global;
        const bool hasImage     = imageMode != BackgroundImageMode::None && !globalImage;
        const bool customImage  = imageMode == BackgroundImageMode::Custom;
        const bool albumCover   = imageMode == BackgroundImageMode::AlbumCover;
        const bool originalSize = static_cast<BackgroundImageScaling>(m_backgroundScaling->currentData().toInt())
                               == BackgroundImageScaling::OriginalSize;

        m_globalHint->setVisible(globalImage);
        m_backgroundCoverTypeLabel->setVisible(albumCover);
        m_backgroundCoverType->setVisible(albumCover);

        const bool trackPreference = albumCover && m_options.testFlag(BackgroundConfigWidget::TrackPreference);
        m_backgroundTrackPreferenceLabel->setVisible(trackPreference);
        m_backgroundTrackPreference->setVisible(trackPreference);

        m_customImageLabel->setVisible(customImage);
        m_customImage->setVisible(customImage);
        m_backgroundScaling->setEnabled(hasImage);
        m_backgroundPosition->setEnabled(hasImage && originalSize);
        m_backgroundMaxSize->setEnabled(hasImage && originalSize);
        m_backgroundBlur->setEnabled(hasImage);
        m_backgroundOpacity->setEnabled(hasImage);
        m_backgroundFadeDuration->setEnabled(hasImage);
        m_backgroundFadeDuration->setVisible(m_options.testFlag(BackgroundConfigWidget::FadeDuration));
    }

    BackgroundConfigWidget* m_self;
    QComboBox* m_backgroundImage;
    QLabel* m_globalHint;
    QLabel* m_backgroundCoverTypeLabel;
    QComboBox* m_backgroundCoverType;
    QLabel* m_backgroundTrackPreferenceLabel;
    QComboBox* m_backgroundTrackPreference;
    QLabel* m_customImageLabel;
    QLineEdit* m_customImage;
    QComboBox* m_backgroundScaling;
    QComboBox* m_backgroundPosition;
    QSpinBox* m_backgroundMaxSize;
    SliderEditor* m_backgroundBlur;
    SliderEditor* m_backgroundOpacity;
    SliderEditor* m_backgroundFadeDuration;
    BackgroundConfigWidget::Options m_options;
};

BackgroundConfigWidget::BackgroundConfigWidget(QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<BackgroundConfigWidgetPrivate>(this)}
{
    auto* browseAction = new QAction(this);
    Gui::setThemeIcon(browseAction, Constants::Icons::Options);
    browseAction->setToolTip(tr("Choose a custom background image file"));
    p->m_customImage->addAction(browseAction, QLineEdit::TrailingPosition);

    p->m_globalHint->setWordWrap(true);

    addComboItem(p->m_backgroundImage, tr("No background image"), static_cast<int>(BackgroundImageMode::None));
    addComboItem(p->m_backgroundImage, tr("Track artwork"), static_cast<int>(BackgroundImageMode::AlbumCover),
                 tr("Use track artwork as the background"));
    addComboItem(p->m_backgroundImage, tr("Custom image"), static_cast<int>(BackgroundImageMode::Custom),
                 tr("Use the selected image file as the background"));
    addComboItem(p->m_backgroundImage, tr("Global background"), static_cast<int>(BackgroundImageMode::Global),
                 tr("Use the shared main window background"));

    addComboItem(p->m_backgroundCoverType, tr("Front"), static_cast<int>(Track::Cover::Front),
                 tr("Use the front cover for current track artwork"));
    addComboItem(p->m_backgroundCoverType, tr("Back"), static_cast<int>(Track::Cover::Back),
                 tr("Use the back cover for current track artwork"));
    addComboItem(p->m_backgroundCoverType, tr("Artist"), static_cast<int>(Track::Cover::Artist),
                 tr("Use the artist picture for current track artwork"));

    addComboItem(p->m_backgroundTrackPreference, tr("Playing track"),
                 static_cast<int>(TrackDisplayPreference::PlayingTrack));
    addComboItem(p->m_backgroundTrackPreference, tr("Selected track"),
                 static_cast<int>(TrackDisplayPreference::SelectedTrack));
    addComboItem(p->m_backgroundTrackPreference, tr("Playing (or selected when stopped)"),
                 static_cast<int>(TrackDisplayPreference::PlayingTrackSelectedWhenStopped));
    addComboItem(p->m_backgroundTrackPreference, tr("Playing (blank at startup)"),
                 static_cast<int>(TrackDisplayPreference::PlayingTrackBlankAtStartup));
    addComboItem(p->m_backgroundTrackPreference, tr("Playing (blank when stopped)"),
                 static_cast<int>(TrackDisplayPreference::PlayingTrackBlankWhenStopped));

    addComboItem(p->m_backgroundScaling, tr("Scaled and cropped"),
                 static_cast<int>(BackgroundImageScaling::ScaledAndCropped),
                 tr("Fill the panel area while preserving proportions; edges may be cropped"));
    addComboItem(p->m_backgroundScaling, tr("Scaled"), static_cast<int>(BackgroundImageScaling::Scaled),
                 tr("Stretch the image to fill the panel area; proportions may change"));
    addComboItem(p->m_backgroundScaling, tr("Scaled, keep proportions"),
                 static_cast<int>(BackgroundImageScaling::ScaledKeepProportions),
                 tr("Fit the whole image inside the panel area without cropping"));
    addComboItem(p->m_backgroundScaling, tr("Original size"), static_cast<int>(BackgroundImageScaling::OriginalSize),
                 tr("Draw the image at its original size, optionally limited by maximum size"));

    addComboItem(p->m_backgroundPosition, tr("Top left"), static_cast<int>(BackgroundImagePosition::TopLeft));
    addComboItem(p->m_backgroundPosition, tr("Top"), static_cast<int>(BackgroundImagePosition::Top));
    addComboItem(p->m_backgroundPosition, tr("Top right"), static_cast<int>(BackgroundImagePosition::TopRight));
    addComboItem(p->m_backgroundPosition, tr("Left"), static_cast<int>(BackgroundImagePosition::Left));
    addComboItem(p->m_backgroundPosition, tr("Middle"), static_cast<int>(BackgroundImagePosition::Middle));
    addComboItem(p->m_backgroundPosition, tr("Right"), static_cast<int>(BackgroundImagePosition::Right));
    addComboItem(p->m_backgroundPosition, tr("Bottom left"), static_cast<int>(BackgroundImagePosition::BottomLeft));
    addComboItem(p->m_backgroundPosition, tr("Bottom"), static_cast<int>(BackgroundImagePosition::Bottom));
    addComboItem(p->m_backgroundPosition, tr("Bottom right"), static_cast<int>(BackgroundImagePosition::BottomRight));

    p->m_backgroundMaxSize->setRange(0, 4096);
    p->m_backgroundMaxSize->setSuffix(u" px"_s);
    p->m_backgroundMaxSize->setSpecialValueText(tr("Disabled"));
    p->m_backgroundBlur->setRange(0, 100);
    p->m_backgroundBlur->setSuffix(u" px"_s);
    p->m_backgroundOpacity->setRange(0, 100);
    p->m_backgroundOpacity->setSuffix(u" %"_s);
    p->m_backgroundFadeDuration->setRange(0, PixmapFadeController::MaxDurationMs);
    p->m_backgroundFadeDuration->setSingleStep(50);
    p->m_backgroundFadeDuration->setSuffix(u" ms"_s);
    p->m_backgroundFadeDuration->addSpecialValue(0, tr("Disabled"));

    p->m_customImage->setToolTip(tr("Path to the custom background image"));
    p->m_backgroundImage->setToolTip(tr("Select which image source to use for the background"));
    p->m_backgroundCoverType->setToolTip(tr("Select which artwork type to use for current track artwork"));
    p->m_backgroundTrackPreference->setToolTip(tr("Select which track supplies the background artwork"));
    p->m_backgroundScaling->setToolTip(tr("Controls how the background image is scaled to the panel area"));
    p->m_backgroundPosition->setToolTip(tr("Alignment for original-size background images"));
    p->m_backgroundMaxSize->setToolTip(tr("Maximum width or height for original-size background images"));
    p->m_backgroundBlur->setToolTip(tr("Applies blur to the background image"));
    p->m_backgroundOpacity->setToolTip(tr("Controls how strongly the background image is shown"));
    p->m_backgroundFadeDuration->setToolTip(tr("Duration for fading between background images; set to 0 to disable"));

    auto* backgroundLayout = new QGridLayout(this);

    int row{0};
    backgroundLayout->addWidget(Gui::createSectionHeader(tr("Image source"), this), row++, 0, 1, 2);
    backgroundLayout->addWidget(new QLabel(tr("Source") + u":"_s, this), row, 0);
    backgroundLayout->addWidget(p->m_backgroundImage, row++, 1);
    backgroundLayout->addWidget(p->m_globalHint, row++, 0, 1, 2);
    backgroundLayout->addWidget(p->m_backgroundCoverTypeLabel, row, 0);
    backgroundLayout->addWidget(p->m_backgroundCoverType, row++, 1);
    backgroundLayout->addWidget(p->m_backgroundTrackPreferenceLabel, row, 0);
    backgroundLayout->addWidget(p->m_backgroundTrackPreference, row++, 1);
    backgroundLayout->addWidget(p->m_customImageLabel, row, 0);
    backgroundLayout->addWidget(p->m_customImage, row++, 1);
    backgroundLayout->addWidget(Gui::createSectionHeader(tr("Layout"), this), row++, 0, 1, 2);
    backgroundLayout->addWidget(new QLabel(tr("Scale mode") + u":"_s, this), row, 0);
    backgroundLayout->addWidget(p->m_backgroundScaling, row++, 1);
    backgroundLayout->addWidget(new QLabel(tr("Alignment") + u":"_s, this), row, 0);
    backgroundLayout->addWidget(p->m_backgroundPosition, row++, 1);
    backgroundLayout->addWidget(new QLabel(tr("Maximum size") + u":"_s, this), row, 0);
    backgroundLayout->addWidget(p->m_backgroundMaxSize, row++, 1);
    backgroundLayout->addWidget(Gui::createSectionHeader(tr("Effects"), this), row++, 0, 1, 2);
    backgroundLayout->addWidget(p->m_backgroundBlur, row++, 0, 1, 2);
    backgroundLayout->addWidget(p->m_backgroundOpacity, row++, 0, 1, 2);
    backgroundLayout->addWidget(p->m_backgroundFadeDuration, row, 0, 1, 2);
    backgroundLayout->setColumnStretch(1, 1);

    QObject::connect(browseAction, &QAction::triggered, this, [this]() { p->browseBackgroundImage(); });
    QObject::connect(p->m_backgroundImage, &QComboBox::currentIndexChanged, this,
                     [this]() { p->updateBackgroundControls(); });
    QObject::connect(p->m_backgroundScaling, &QComboBox::currentIndexChanged, this,
                     [this]() { p->updateBackgroundControls(); });

    for(auto* combo : {p->m_backgroundImage, p->m_backgroundCoverType, p->m_backgroundTrackPreference,
                       p->m_backgroundScaling, p->m_backgroundPosition}) {
        QObject::connect(combo, &QComboBox::currentIndexChanged, this, &BackgroundConfigWidget::configChanged);
    }
    QObject::connect(p->m_customImage, &QLineEdit::textChanged, this, &BackgroundConfigWidget::configChanged);
    QObject::connect(p->m_backgroundMaxSize, &QSpinBox::valueChanged, this, &BackgroundConfigWidget::configChanged);
    for(auto* slider : {p->m_backgroundBlur, p->m_backgroundOpacity, p->m_backgroundFadeDuration}) {
        QObject::connect(slider, &SliderEditor::valueChanged, this, &BackgroundConfigWidget::configChanged);
    }

    setConfig({});
}

BackgroundConfigWidget::~BackgroundConfigWidget() = default;

BackgroundConfig BackgroundConfigWidget::config() const
{
    return {
        .imageMode       = p->m_backgroundImage->currentData().toInt(),
        .customImage     = p->m_customImage->text(),
        .coverType       = p->m_backgroundCoverType->currentData().toInt(),
        .scaling         = p->m_backgroundScaling->currentData().toInt(),
        .position        = p->m_backgroundPosition->currentData().toInt(),
        .maxSize         = p->m_backgroundMaxSize->value(),
        .blur            = p->m_backgroundBlur->value(),
        .opacity         = p->m_backgroundOpacity->value(),
        .fadeDuration    = p->m_backgroundFadeDuration->value(),
        .trackPreference = static_cast<TrackDisplayPreference>(p->m_backgroundTrackPreference->currentData().toInt()),
    };
}

void BackgroundConfigWidget::setConfig(const BackgroundConfig& config)
{
    const QSignalBlocker blocker{this};

    p->m_backgroundImage->setCurrentIndex(std::max(0, p->m_backgroundImage->findData(config.imageMode)));
    p->m_customImage->setText(config.customImage);
    p->m_backgroundCoverType->setCurrentIndex(p->m_backgroundCoverType->findData(config.coverType));
    p->m_backgroundTrackPreference->setCurrentIndex(
        p->m_backgroundTrackPreference->findData(static_cast<int>(config.trackPreference)));
    p->m_backgroundScaling->setCurrentIndex(p->m_backgroundScaling->findData(config.scaling));
    p->m_backgroundPosition->setCurrentIndex(p->m_backgroundPosition->findData(config.position));
    p->m_backgroundMaxSize->setValue(config.maxSize);
    p->m_backgroundBlur->setValue(config.blur);
    p->m_backgroundOpacity->setValue(config.opacity);
    p->m_backgroundFadeDuration->setValue(config.fadeDuration);

    p->updateBackgroundControls();
}

void BackgroundConfigWidget::setOptions(Options options)
{
    if(std::exchange(p->m_options, options) != options) {
        const int globalIndex = p->m_backgroundImage->findData(static_cast<int>(BackgroundImageMode::Global));
        if(options.testFlag(NoGlobalSource) && globalIndex >= 0) {
            p->m_backgroundImage->removeItem(globalIndex);
        }
        else if(!options.testFlag(NoGlobalSource) && globalIndex < 0) {
            addComboItem(p->m_backgroundImage, tr("Global background"), static_cast<int>(BackgroundImageMode::Global),
                         tr("Use the shared main window background"));
        }
        p->updateBackgroundControls();
    }
}
} // namespace Fooyin

#include "gui/widgets/moc_backgroundconfigwidget.cpp"
