/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <gui/fywidget.h>

#include <utils/crypto.h>

#include <QAction>
#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLayout>
#include <QMenu>
#include <QPointer>

using namespace Qt::StringLiterals;

namespace Fooyin {
class FyWidgetPrivate
{
public:
    explicit FyWidgetPrivate(FyWidget* self)
        : m_self{self}
    { }

    void saveCommonLayoutData(QJsonObject& layout, bool includeId) const
    {
        if(includeId
           && (m_features & FyWidget::PersistId || m_features & (FyWidget::Search | FyWidget::ExclusiveSearch))) {
            layout["ID"_L1] = m_id.name();
        }

        if(m_hasCustomMargins && m_self->layout()) {
            const QMargins margins = m_self->layout()->contentsMargins();
            QJsonObject marginData;
            marginData["Left"_L1]   = margins.left();
            marginData["Top"_L1]    = margins.top();
            marginData["Right"_L1]  = margins.right();
            marginData["Bottom"_L1] = margins.bottom();
            layout["Margins"_L1]    = marginData;
        }
    }

    FyWidget* m_self;
    Id m_id{Utils::generateUniqueHash()};
    FyWidget::Features m_features;
    bool m_hasCustomMargins{false};
    QPointer<QDialog> m_configDialog;
};

QString LayoutCopyContext::mappedString(const QString& scope, const QString& value)
{
    const QString key = scope + u':' + value;
    if(!m_stringMappings.contains(key)) {
        m_stringMappings.emplace(key, Utils::generateUniqueHash());
    }
    return m_stringMappings.at(key);
}

FyWidget::FyWidget(QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<FyWidgetPrivate>(this)}
{ }

FyWidget::~FyWidget() = default;

Id FyWidget::id() const
{
    return p->m_id;
}

FyWidget::Features FyWidget::features() const
{
    return p->m_features;
}

bool FyWidget::hasFeature(Feature feature) const
{
    return p->m_features & feature;
}

void FyWidget::setFeature(Feature feature, bool on)
{
    if(on) {
        p->m_features |= feature;
    }
    else {
        p->m_features &= ~feature;
    }
}

FyWidget* FyWidget::findParent() const
{
    QWidget* parent = parentWidget();
    while(parent && !qobject_cast<FyWidget*>(parent)) {
        parent = parent->parentWidget();
    }
    return qobject_cast<FyWidget*>(parent);
}

QRect FyWidget::widgetGeometry() const
{
    int x = this->x();
    int y = this->y();

    const auto* widget{this};

    while((widget = widget->findParent())) {
        x += widget->x();
        y += widget->y();
    }

    return {x, y, width(), height()};
}

void FyWidget::saveLayout(QJsonArray& layout)
{
    QJsonObject widgetData;

    p->saveCommonLayoutData(widgetData, true);
    saveLayoutData(widgetData);

    QJsonObject widgetObject;
    widgetObject[layoutName()] = widgetData;

    layout.append(widgetObject);
}

void FyWidget::saveBaseLayout(QJsonArray& layout)
{
    QJsonObject widgetData;

    p->saveCommonLayoutData(widgetData, false);
    saveLayoutData(widgetData);

    QJsonObject widgetObject;
    widgetObject[layoutName()] = widgetData;

    layout.append(widgetObject);
}

void FyWidget::saveCopyLayout(QJsonArray& layout, LayoutCopyContext& context, bool isRoot)
{
    QJsonObject widgetData;

    p->saveCommonLayoutData(widgetData, false);
    saveCopyLayoutData(widgetData, context, isRoot);

    QJsonObject widgetObject;
    widgetObject[layoutName()] = widgetData;

    layout.append(widgetObject);
}

void FyWidget::loadLayout(const QJsonObject& layout)
{
    if(layout.contains("ID"_L1)) {
        p->m_id = Id{layout["ID"_L1].toString()};
    }

    p->m_hasCustomMargins = false;

    if(this->layout() && layout.value("Margins"_L1).isObject()) {
        const QJsonObject marginData = layout.value("Margins"_L1).toObject();
        this->layout()->setContentsMargins(marginData.value("Left"_L1).toInt(), marginData.value("Top"_L1).toInt(),
                                           marginData.value("Right"_L1).toInt(), marginData.value("Bottom"_L1).toInt());
        p->m_hasCustomMargins = true;
    }

    loadLayoutData(layout);
}

void FyWidget::searchEvent(const SearchRequest& /*request*/) { }

void FyWidget::layoutEditingMenu(QMenu* /*menu*/) { }

void FyWidget::saveLayoutData(QJsonObject& /*object*/) { }

void FyWidget::saveCopyLayoutData(QJsonObject& layout, LayoutCopyContext& /*context*/, bool /*isRoot*/)
{
    saveLayoutData(layout);
    layout.remove("ID"_L1);
}

void FyWidget::loadLayoutData(const QJsonObject& /*object*/) { }

void FyWidget::finalise() { }

void FyWidget::openConfigDialog() { }

QAction* FyWidget::addConfigureAction(QMenu* menu, bool addSeparator)
{
    if(!menu) {
        return nullptr;
    }

    if(addSeparator) {
        menu->addSeparator();
    }

    auto* configure = new QAction(tr("Configure…"), menu);
    QObject::connect(configure, &QAction::triggered, this, &FyWidget::openConfigDialog);
    menu->addAction(configure);

    return configure;
}

void FyWidget::showConfigDialog(QDialog* dialog)
{
    if(!dialog) {
        return;
    }

    if(p->m_configDialog) {
        dialog->deleteLater();
        p->m_configDialog->raise();
        p->m_configDialog->activateWindow();
        return;
    }

    p->m_configDialog = dialog;
    QObject::connect(dialog, &QDialog::destroyed, this, [this]() { p->m_configDialog = nullptr; });
    dialog->open();
}
} // namespace Fooyin

#include "gui/moc_fywidget.cpp"
