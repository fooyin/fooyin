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

#include <utils/settings/settingspage.h>

#include <utils/settings/settingsdialogcontroller.h>

namespace Fooyin {
SettingsPage::SettingsPage(SettingsDialogController* controller, QObject* parent)
    : QObject{parent}
    , m_position{SettingsPagePosition::Default}
    , m_relativePosition{SettingsPageRelativePosition::None}
    , m_widget{nullptr}
{
    if(controller) {
        controller->addPage(this);
    }
}

Id SettingsPage::id() const
{
    return m_id;
}

QString SettingsPage::name() const
{
    return m_name;
}

QStringList SettingsPage::category() const
{
    return m_category;
}

SettingsPagePosition SettingsPage::position() const
{
    return m_position;
}

SettingsPageRelativePosition SettingsPage::relativePosition() const
{
    return m_relativePosition;
}

Id SettingsPage::positionPage() const
{
    return m_positionPage;
}

void SettingsPage::setWidgetCreator(const WidgetCreator& widgetCreator)
{
    m_widgetCreator = widgetCreator;
}

QWidget* SettingsPage::widget()
{
    if(!m_widget && m_widgetCreator) {
        m_widget = m_widgetCreator();
        if(auto* pageWidget = qobject_cast<SettingsPageWidget*>(m_widget); pageWidget && !m_state.isEmpty()) {
            pageWidget->restoreState(m_state);
        }
    }

    return m_widget;
}

void SettingsPage::load()
{
    if(m_widget) {
        if(auto* pageWidget = qobject_cast<SettingsPageWidget*>(m_widget)) {
            pageWidget->load();
        }
    }
}

void SettingsPage::apply()
{
    if(m_widget) {
        if(auto* pageWidget = qobject_cast<SettingsPageWidget*>(m_widget)) {
            pageWidget->apply();
        }
    }
}

void SettingsPage::finish()
{
    if(m_widget) {
        if(auto* pageWidget = qobject_cast<SettingsPageWidget*>(m_widget)) {
            pageWidget->finish();
            m_state = pageWidget->saveState();
        }
        delete m_widget;
        m_widget = nullptr;
    }
}

void SettingsPage::reset()
{
    if(widget()) {
        if(auto* pageWidget = qobject_cast<SettingsPageWidget*>(m_widget)) {
            pageWidget->reset();
        }
    }
}

QByteArray SettingsPage::state() const
{
    return m_state;
}

void SettingsPage::setState(const QByteArray& state)
{
    m_state = state;
    if(m_widget) {
        if(auto* pageWidget = qobject_cast<SettingsPageWidget*>(m_widget)) {
            pageWidget->restoreState(m_state);
        }
    }
}

QString SettingsPage::validationError() const
{
    if(m_widget) {
        if(auto* pageWidget = qobject_cast<SettingsPageWidget*>(m_widget)) {
            return pageWidget->validationError();
        }
    }
    return {};
}

void SettingsPage::setId(const Id& id)
{
    m_id = id;
}

void SettingsPage::setName(const QString& name)
{
    m_name = name;
}

void SettingsPage::setCategory(const QStringList& category)
{
    m_category = category;
}

void SettingsPage::setPosition(SettingsPagePosition position)
{
    m_position         = position;
    m_relativePosition = SettingsPageRelativePosition::None;
    m_positionPage     = {};
}

void SettingsPage::setRelativePosition(SettingsPageRelativePosition position, const Id& page)
{
    if(position == SettingsPageRelativePosition::None || !page.isValid()) {
        setPosition(SettingsPagePosition::Default);
        return;
    }

    m_position         = SettingsPagePosition::Default;
    m_relativePosition = position;
    m_positionPage     = page;
}
} // namespace Fooyin

#include "utils/settings/moc_settingspage.cpp"
