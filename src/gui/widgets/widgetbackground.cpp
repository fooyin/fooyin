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

#include <gui/widgets/widgetbackground.h>

#include "backgroundrenderer.h"
#include "globalbackgroundcontroller.h"

#include <QApplication>
#include <QEvent>
#include <QPainter>
#include <QPointer>
#include <QStyleOptionViewItem>
#include <QWidget>

#include <vector>

constexpr auto OpaqueAltAlphaThreshold = 185;
constexpr auto TransparentAltAlpha     = 80;

namespace Fooyin {
class WidgetBackgroundPrivate
{
public:
    WidgetBackgroundPrivate(WidgetBackground* self, QWidget* viewport)
        : m_self{self}
        , m_viewport{viewport}
        , m_renderer{new BackgroundRenderer(self)}
    { }

    void watchAncestors()
    {
        for(const auto& ancestor : m_ancestors) {
            if(ancestor) {
                ancestor->removeEventFilter(m_self);
            }
        }
        m_ancestors.clear();

        if(m_config.imageMode != static_cast<int>(BackgroundImageMode::Global)) {
            return;
        }

        auto* ancestor{m_viewport};
        while(!ancestor->isWindow() && (ancestor = ancestor->parentWidget())) {
            ancestor->installEventFilter(m_self);
            m_ancestors.emplace_back(ancestor);
        }
    }

    void updateRowTransparency()
    {
        if(m_rowView) {
            m_rowView->setProperty("transparent_base_rows", m_self->hasImage());
        }
    }

    void updateGlobalController()
    {
        auto* controller = m_config.imageMode == static_cast<int>(BackgroundImageMode::Global)
                             ? GlobalBackgroundController::forWidget(m_viewport)
                             : nullptr;
        if(m_global == controller) {
            return;
        }

        QObject::disconnect(m_globalChanged);
        m_global = controller;
        if(m_global) {
            m_globalChanged = QObject::connect(m_global, &GlobalBackgroundController::changed, m_self, [this] {
                updateRowTransparency();
                m_viewport->update();
            });
        }
    }

    WidgetBackground* m_self;
    QWidget* m_viewport;
    QPointer<QWidget> m_rowView;
    BackgroundConfig m_config;
    BackgroundRenderer* m_renderer;
    QPointer<GlobalBackgroundController> m_global;
    QMetaObject::Connection m_globalChanged;
    std::vector<QPointer<QWidget>> m_ancestors;
};

WidgetBackground::WidgetBackground(QWidget* viewport)
    : QObject{viewport}
    , p{std::make_unique<WidgetBackgroundPrivate>(this, viewport)}
{
    viewport->installEventFilter(this);

    QObject::connect(p->m_renderer, &BackgroundRenderer::changed, this, [this] {
        p->updateRowTransparency();
        p->m_viewport->update();
    });
}

WidgetBackground::WidgetBackground(QWidget* viewport, QWidget* rowView)
    : WidgetBackground{viewport}
{
    p->m_rowView = rowView;
    p->updateRowTransparency();
}

WidgetBackground::~WidgetBackground() = default;

void WidgetBackground::setConfig(const BackgroundConfig& config)
{
    auto validated{config};
    validated.normalise();
    p->m_config = std::move(validated);
    p->watchAncestors();
    p->updateGlobalController();
    p->m_renderer->setConfig(p->m_config);
    p->updateRowTransparency();
    p->m_viewport->update();
}

void WidgetBackground::setPixmap(const QPixmap& pixmap)
{
    p->m_renderer->setPixmap(pixmap);
}

void WidgetBackground::draw(QPainter& painter)
{
    p->updateGlobalController();
    p->updateRowTransparency();
    if(p->m_config.imageMode == static_cast<int>(BackgroundImageMode::Global)) {
        if(p->m_global) {
            p->m_global->draw(painter, p->m_viewport);
        }
        return;
    }
    p->m_renderer->setCanvas(p->m_viewport->size(), p->m_viewport->devicePixelRatioF());
    p->m_renderer->draw(painter);
}

bool WidgetBackground::hasImage() const
{
    p->updateGlobalController();
    if(p->m_config.imageMode == static_cast<int>(BackgroundImageMode::Global)) {
        return p->m_global && p->m_global->hasImage();
    }
    return p->m_renderer->hasImage();
}

bool WidgetBackground::hasTransparentRows(const QWidget* view)
{
    return view && view->property("transparent_base_rows").toBool();
}

void WidgetBackground::makeRowPaletteTransparent(QPalette& palette)
{
    for(const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        QColor base = palette.color(group, QPalette::Base);
        base.setAlpha(base.alpha() >= OpaqueAltAlphaThreshold ? 0 : base.alpha());
        palette.setColor(group, QPalette::Base, base);

        QColor alternate = palette.color(group, QPalette::AlternateBase);
        alternate.setAlpha(alternate.alpha() >= OpaqueAltAlphaThreshold ? TransparentAltAlpha : alternate.alpha());
        palette.setColor(group, QPalette::AlternateBase, alternate);
    }
}

void WidgetBackground::clearDelegateBackground(QStyleOptionViewItem& option)
{
    if(hasTransparentRows(option.widget) && !option.state.testFlag(QStyle::State_Selected)
       && option.backgroundBrush.style() == Qt::NoBrush) {
        option.palette.setBrush(QPalette::Base, Qt::NoBrush);
        option.features.setFlag(QStyleOptionViewItem::Alternate, false);
    }
}

void WidgetBackground::drawItemBackground(QPainter* painter, const QStyleOptionViewItem& option)
{
    const auto* style      = option.widget ? option.widget->style() : QApplication::style();
    const bool hoverColour = option.backgroundBrush.style() != Qt::NoBrush
                          && option.state.testFlag(QStyle::State_MouseOver)
                          && !option.state.testFlag(QStyle::State_Selected);

    QStyleOptionViewItem background{option};
    if(hoverColour) {
        background.state.setFlag(QStyle::State_MouseOver, false);
    }
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &background, painter, option.widget);

    if(hoverColour) {
        background.state           = option.state;
        background.backgroundBrush = Qt::NoBrush;
        background.palette.setBrush(QPalette::Base, Qt::NoBrush);
        background.features.setFlag(QStyleOptionViewItem::Alternate, false);
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &background, painter, option.widget);
    }
}

bool WidgetBackground::eventFilter(QObject* watched, QEvent* event)
{
    if(watched == p->m_viewport) {
        switch(event->type()) {
            case QEvent::Resize:
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
            case QEvent::DevicePixelRatioChange:
#endif
                p->m_renderer->setCanvas(p->m_viewport->size(), p->m_viewport->devicePixelRatioF());
                break;
            default:
                break;
        }
    }
    if(event->type() == QEvent::ParentChange || event->type() == QEvent::Show || event->type() == QEvent::Move) {
        if(event->type() != QEvent::Move) {
            p->watchAncestors();
        }
        p->updateGlobalController();
        p->updateRowTransparency();
        p->m_viewport->update();
    }
    return QObject::eventFilter(watched, event);
}

void WidgetBackground::timerEvent(QTimerEvent* event)
{
    QObject::timerEvent(event);
}
} // namespace Fooyin
