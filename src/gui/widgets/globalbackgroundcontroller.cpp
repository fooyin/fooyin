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

#include "globalbackgroundcontroller.h"

#include "backgroundrenderer.h"
#include "internalguisettings.h"

#include <gui/widgets/backgroundimagesource.h>
#include <utils/settings/settingsmanager.h>

#include <QEvent>
#include <QJsonObject>
#include <QPainter>
#include <QWidget>

using namespace Qt::StringLiterals;

namespace Fooyin {
GlobalBackgroundController::GlobalBackgroundController(QWidget* window, CoverProvider* coverProvider,
                                                       PlayerController* playerController,
                                                       TrackSelectionController* selectionController,
                                                       SettingsManager* settings)
    : QObject{window}
    , m_window{window}
    , m_source{new BackgroundImageSource(coverProvider, playerController, selectionController, this)}
    , m_renderer{new BackgroundRenderer(this)}
    , m_useSharedBase{false}
{
    window->installEventFilter(this);
    m_renderer->setCanvas(window->size(), window->devicePixelRatioF());

    QObject::connect(m_source, &BackgroundImageSource::pixmapChanged, m_renderer, &BackgroundRenderer::setPixmap);
    QObject::connect(m_renderer, &BackgroundRenderer::changed, this, &GlobalBackgroundController::changed);

    const auto applyConfig = [this](const QVariant& value) {
        const auto values        = QJsonObject::fromVariantMap(value.toMap());
        const bool useSharedBase = values.value("UseSharedBase"_L1).toBool();
        const bool baseChanged   = std::exchange(m_useSharedBase, useSharedBase) != useSharedBase;

        BackgroundConfig config;
        config.loadLayout(values);
        if(config.imageMode == static_cast<int>(BackgroundImageMode::Global)) {
            config.imageMode = static_cast<int>(BackgroundImageMode::None);
        }
        m_renderer->setConfig(config);
        m_source->setConfig(config);

        if(baseChanged) {
            Q_EMIT changed();
        }
    };

    applyConfig(settings->value<Settings::Gui::Internal::GlobalBackground>());
    settings->subscribe<Settings::Gui::Internal::GlobalBackground>(this, applyConfig);
}

GlobalBackgroundController* GlobalBackgroundController::forWidget(const QWidget* widget)
{
    return widget->window()->findChild<GlobalBackgroundController*>(QString{}, Qt::FindDirectChildrenOnly);
}

bool GlobalBackgroundController::hasImage() const
{
    return m_renderer->hasImage();
}

void GlobalBackgroundController::draw(QPainter& painter, const QWidget* viewport)
{
    m_renderer->setCanvas(m_window->size(), m_window->devicePixelRatioF());

    if(!hasImage()) {
        return;
    }

    const QPoint offset = viewport->mapTo(m_window, QPoint{});

    painter.save();

    painter.setClipRect(viewport->rect(), Qt::IntersectClip);
    painter.translate(-offset);
    if(m_useSharedBase) {
        QColor base = m_window->palette().color(QPalette::Base);
        base.setAlpha(255);
        painter.fillRect(m_window->rect(), base);
    }
    m_renderer->draw(painter);

    painter.restore();
}

bool GlobalBackgroundController::eventFilter(QObject* watched, QEvent* event)
{
    if(watched == m_window) {
        switch(event->type()) {
            case QEvent::Resize:
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
            case QEvent::DevicePixelRatioChange:
#endif
                m_renderer->setCanvas(m_window->size(), m_window->devicePixelRatioF());
                break;
            case QEvent::PaletteChange:
                Q_EMIT changed();
                break;
            default:
                break;
        }
    }
    return QObject::eventFilter(watched, event);
}
} // namespace Fooyin

#include "moc_globalbackgroundcontroller.cpp"
