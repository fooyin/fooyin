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

#include "ratingcontrol.h"

#include <core/player/playercontroller.h>
#include <gui/guisettings.h>
#include <gui/guiutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/stareditor.h>
#include <utils/starrating.h>

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

using namespace Qt::StringLiterals;

constexpr double ControlPadding = 0.2;

namespace Fooyin {
namespace {
int paddedControlMargin(int glyphSize)
{
    return static_cast<int>((ControlPadding * glyphSize) / (1.0 - (2.0 * ControlPadding)));
}
} // namespace

class RatingControlEditor : public QWidget
{
    Q_OBJECT

public:
    explicit RatingControlEditor(QWidget* parent = nullptr);

    void setRating(const StarRating& rating);
    void setInteractive(bool enabled);
    void setToolButtonOptions(Settings::Gui::ToolButtonOptions options);

    [[nodiscard]] QSize sizeHint() const override;

Q_SIGNALS:
    void ratingChanged(float rating);

protected:
    void paintEvent(QPaintEvent* /*event*/) override;
    void leaveEvent(QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    [[nodiscard]] StarRating ratingForSize() const;
    void setCurrentRating(float rating);

    StarRating m_rating;
    float m_previewRating{0};
    bool m_stretchEnabled{false};
};

RatingControlEditor::RatingControlEditor(QWidget* parent)
    : QWidget{parent}
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
}

void RatingControlEditor::setRating(const StarRating& rating)
{
    m_rating        = rating;
    m_previewRating = rating.rating();

    updateGeometry();
    update();
}

void RatingControlEditor::setInteractive(bool enabled)
{
    setEnabled(enabled);
    setCursor(enabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if(!enabled) {
        m_previewRating = m_rating.rating();
        update();
    }
}

void RatingControlEditor::setToolButtonOptions(Settings::Gui::ToolButtonOptions options)
{
    m_stretchEnabled = options & Settings::Gui::Stretch;

    setSizePolicy(m_stretchEnabled ? QSizePolicy::Preferred : QSizePolicy::Fixed,
                  m_stretchEnabled ? QSizePolicy::Preferred : QSizePolicy::Fixed);
    updateGeometry();
    update();
}

QSize RatingControlEditor::sizeHint() const
{
    const int margin = paddedControlMargin(m_rating.starScale());
    return m_rating.sizeHint() + QSize{2 * margin, 2 * margin};
}

void RatingControlEditor::paintEvent(QPaintEvent*)
{
    QPainter painter{this};

    auto displayedRating = ratingForSize();
    displayedRating.setRating(m_previewRating);
    displayedRating.paint(&painter, rect(), palette(), StarRating::EditMode::Editable, Qt::AlignCenter);
}

void RatingControlEditor::leaveEvent(QEvent* event)
{
    m_previewRating = m_rating.rating();
    update();
    QWidget::leaveEvent(event);
}

void RatingControlEditor::mouseMoveEvent(QMouseEvent* event)
{
    if(isEnabled()) {
        m_previewRating
            = StarEditor::ratingAtPosition(event->position().toPoint(), rect(), ratingForSize(), Qt::AlignCenter);
        update();
    }

    QWidget::mouseMoveEvent(event);
}

void RatingControlEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if(isEnabled() && event->button() == Qt::LeftButton) {
        setCurrentRating(
            StarEditor::ratingAtPosition(event->position().toPoint(), rect(), ratingForSize(), Qt::AlignCenter));
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void RatingControlEditor::keyPressEvent(QKeyEvent* event)
{
    if(isEnabled() && (event->key() == Qt::Key_Left || event->key() == Qt::Key_Down)) {
        setCurrentRating(std::max(0.0F, m_rating.rating() - 0.1F));
        event->accept();
        return;
    }
    if(isEnabled() && (event->key() == Qt::Key_Right || event->key() == Qt::Key_Up)) {
        setCurrentRating(std::min(1.0F, std::max(0.0F, m_rating.rating()) + 0.1F));
        event->accept();
        return;
    }
    if(isEnabled() && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_0)) {
        setCurrentRating(0.0F);
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

StarRating RatingControlEditor::ratingForSize() const
{
    auto rating{m_rating};

    if(m_stretchEnabled) {
        const int padding = static_cast<int>(ControlPadding * std::min(width(), height()));
        const int scale   = std::min(height() - (2 * padding), (width() - (2 * padding)) / rating.maxStarCount());
        rating.setStarScale(std::max(m_rating.starScale(), scale));
    }

    return rating;
}

void RatingControlEditor::setCurrentRating(float rating)
{
    m_rating.setRating(rating);
    m_previewRating = rating;
    update();
    Q_EMIT ratingChanged(rating);
}

RatingControl::RatingControl(PlayerController* playerController, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_editor{new RatingControlEditor(this)}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_editor);

    QObject::connect(m_editor, &RatingControlEditor::ratingChanged, this, &RatingControl::changeRating);
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &RatingControl::updateTrack);
    QObject::connect(m_playerController, &PlayerController::currentTrackUpdated, this, &RatingControl::updateTrack);
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this,
                     [this]() { updateTrack(m_playerController->currentTrack()); });

    const auto updateToolButtonOptions = [this](int value) {
        const auto options = static_cast<Settings::Gui::ToolButtonOptions>(value);
        const bool stretch = options & Settings::Gui::Stretch;
        setSizePolicy(stretch ? QSizePolicy::Preferred : QSizePolicy::Fixed,
                      stretch ? QSizePolicy::Preferred : QSizePolicy::Fixed);
        m_editor->setToolButtonOptions(options);
    };
    updateToolButtonOptions(m_settings->value<Settings::Gui::ToolButtonStyle>());
    m_settings->subscribe<Settings::Gui::ToolButtonStyle>(this, updateToolButtonOptions);

    m_settings->subscribe<Settings::Gui::RatingOneStarColour>(this, &RatingControl::updateAppearance);
    m_settings->subscribe<Settings::Gui::RatingTwoStarColour>(this, &RatingControl::updateAppearance);
    m_settings->subscribe<Settings::Gui::RatingThreeStarColour>(this, &RatingControl::updateAppearance);
    m_settings->subscribe<Settings::Gui::RatingFourStarColour>(this, &RatingControl::updateAppearance);
    m_settings->subscribe<Settings::Gui::RatingFiveStarColour>(this, &RatingControl::updateAppearance);
    m_settings->subscribe<Settings::Gui::UnratedStarColour>(this, &RatingControl::updateAppearance);
    m_settings->subscribe<Settings::Gui::StarRatingSize>(this, &RatingControl::updateAppearance);

    updateTrack(m_playerController->currentTrack());
}

QString RatingControl::name() const
{
    return tr("Rating Control");
}

QString RatingControl::layoutName() const
{
    return u"RatingControl"_s;
}

void RatingControl::updateTrack(const Track& track)
{
    const float rating = track.isValid() ? std::max(0.0F, track.rating()) : 0.0F;
    m_editor->setRating({rating, 5, m_settings->value<Settings::Gui::StarRatingSize>(),
                         Gui::ratingStarColours(*m_settings), Gui::unratedStarColour(*m_settings)});
    m_editor->setInteractive(track.isValid() && m_playerController->playState() != Player::PlayState::Stopped);
    setMinimumSize(m_editor->sizeHint());
}

void RatingControl::updateAppearance()
{
    updateTrack(m_playerController->currentTrack());
}

void RatingControl::changeRating(float rating)
{
    Track track = m_playerController->currentTrack();
    if(!track.isValid() || m_playerController->playState() == Player::PlayState::Stopped) {
        return;
    }

    track.setRating(rating);
    updateTrack(track);
    Q_EMIT trackRated(track);
}
} // namespace Fooyin

#include "moc_ratingcontrol.cpp"
#include "ratingcontrol.moc"
