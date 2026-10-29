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

#include "lovecontrol.h"

#include <core/player/playercontroller.h>
#include <gui/guisettings.h>
#include <gui/guiutils.h>
#include <utils/hearteditor.h>
#include <utils/settings/settingsmanager.h>

#include <QEnterEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

using namespace Qt::StringLiterals;

constexpr auto ControlPadding = 0.2;

namespace Fooyin {
namespace {
int paddedControlMargin(int glyphSize)
{
    return static_cast<int>((ControlPadding * glyphSize) / (1.0 - (2.0 * ControlPadding)));
}
} // namespace

class LoveControlEditor : public QWidget
{
    Q_OBJECT

public:
    explicit LoveControlEditor(QWidget* parent = nullptr);

    void setValue(const HeartValue& value);
    void setInteractive(bool enabled);
    void setToolButtonOptions(Settings::Gui::ToolButtonOptions options);

    [[nodiscard]] QSize sizeHint() const override;

Q_SIGNALS:
    void loveToggled(bool loved);

protected:
    void paintEvent(QPaintEvent* /*event*/) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    [[nodiscard]] HeartValue valueForSize() const;

    void toggleLoved();

    HeartValue m_value;
    bool m_hoverLoved{false};
    bool m_hovered{false};
    bool m_stretchEnabled{false};
};

LoveControlEditor::LoveControlEditor(QWidget* parent)
    : QWidget{parent}
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
}

void LoveControlEditor::setValue(const HeartValue& value)
{
    m_value = value;

    if(m_hovered) {
        m_hoverLoved = !m_value.loved();
    }

    updateGeometry();
    update();
}

void LoveControlEditor::setInteractive(bool enabled)
{
    setEnabled(enabled);
    setCursor(enabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if(!enabled) {
        m_hovered = false;
        update();
    }
}

void LoveControlEditor::setToolButtonOptions(Settings::Gui::ToolButtonOptions options)
{
    m_stretchEnabled = options & Settings::Gui::Stretch;

    setSizePolicy(m_stretchEnabled ? QSizePolicy::Preferred : QSizePolicy::Fixed,
                  m_stretchEnabled ? QSizePolicy::Preferred : QSizePolicy::Fixed);

    updateGeometry();
    update();
}

QSize LoveControlEditor::sizeHint() const
{
    const int margin = paddedControlMargin(m_value.scale());
    return m_value.sizeHint() + QSize{2 * margin, 2 * margin};
}

void LoveControlEditor::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};

    auto displayedValue = valueForSize();
    if(m_hovered) {
        displayedValue.setLoved(m_hoverLoved);
    }
    displayedValue.paint(&painter, rect(), palette(), HeartValue::EditMode::Editable, Qt::AlignCenter);
}

void LoveControlEditor::enterEvent(QEnterEvent* event)
{
    if(isEnabled()) {
        m_hoverLoved = !m_value.loved();
        m_hovered    = true;
        update();
    }
    QWidget::enterEvent(event);
}

void LoveControlEditor::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void LoveControlEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if(isEnabled() && event->button() == Qt::LeftButton) {
        toggleLoved();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void LoveControlEditor::keyPressEvent(QKeyEvent* event)
{
    if(isEnabled()
       && (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        toggleLoved();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

HeartValue LoveControlEditor::valueForSize() const
{
    auto value{m_value};

    if(m_stretchEnabled) {
        const int length  = std::min(width(), height());
        const int padding = static_cast<int>(ControlPadding * length);
        value.setScale(std::max(m_value.scale(), length - (2 * padding)));
    }

    return value;
}

void LoveControlEditor::toggleLoved()
{
    m_value.setLoved(!m_value.loved());
    m_hovered = false;
    update();
    Q_EMIT loveToggled(m_value.loved());
}

LoveControl::LoveControl(PlayerController* playerController, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_editor{new LoveControlEditor(this)}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_editor);

    QObject::connect(m_editor, &LoveControlEditor::loveToggled, this, &LoveControl::changeLoved);
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &LoveControl::updateTrack);
    QObject::connect(m_playerController, &PlayerController::currentTrackUpdated, this, &LoveControl::updateTrack);
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

    m_settings->subscribe<Settings::Gui::LoveHeartColour>(this, &LoveControl::updateAppearance);
    m_settings->subscribe<Settings::Gui::UnlovedHeartColour>(this, &LoveControl::updateAppearance);
    m_settings->subscribe<Settings::Gui::LoveHeartSize>(this, &LoveControl::updateAppearance);

    updateTrack(m_playerController->currentTrack());
}

QString LoveControl::name() const
{
    return tr("Love Control");
}

QString LoveControl::layoutName() const
{
    return u"LoveControl"_s;
}

void LoveControl::updateTrack(const Track& track)
{
    m_editor->setValue({track.isValid() && track.isLoved(), m_settings->value<Settings::Gui::LoveHeartSize>(),
                        Gui::loveHeartColour(*m_settings), Gui::unlovedHeartColour(*m_settings)});
    m_editor->setInteractive(track.isValid() && m_playerController->playState() != Player::PlayState::Stopped);
    setMinimumSize(m_editor->sizeHint());
}

void LoveControl::updateAppearance()
{
    updateTrack(m_playerController->currentTrack());
}

void LoveControl::changeLoved(bool loved)
{
    Track track = m_playerController->currentTrack();
    if(!track.isValid() || m_playerController->playState() == Player::PlayState::Stopped) {
        return;
    }

    track.setLoved(loved);
    updateTrack(track);
    Q_EMIT trackLoved(track);
}
} // namespace Fooyin

#include "lovecontrol.moc"
#include "moc_lovecontrol.cpp"
