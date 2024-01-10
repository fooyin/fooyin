/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

void SettingsPage::setWidgetCreator(const WidgetCreator& widgetCreator)
{
    m_widgetCreator = widgetCreator;
}

QWidget* SettingsPage::widget()
{
    if(!m_widget) {
        if(m_widgetCreator) {
            m_widget = m_widgetCreator();
        }
    }
    return m_widget;
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
        }
        delete m_widget;
        m_widget = nullptr;
    }
}

void SettingsPage::reset()
{
    if(m_widget) {
        if(auto* pageWidget = qobject_cast<SettingsPageWidget*>(m_widget)) {
            pageWidget->reset();
        }
    }
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
} // namespace Fooyin

#include "utils/settings/moc_settingspage.cpp"
