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

#include <QAbstractButton>
#include <QGroupBox>
#include <QLabel>
#include <QTextDocument>

using namespace Qt::StringLiterals;

namespace {
QString searchText(QString text)
{
    for(qsizetype index{0}; index < text.size(); ++index) {
        if(text.at(index) == '&'_L1) {
            text.remove(index, 1);
        }
    }
    return text;
}
} // namespace

namespace Fooyin {
SettingsPage::SettingsPage(SettingsDialogController* controller, QObject* parent)
    : QObject{parent}
    , m_position{SettingsPagePosition::Default}
    , m_relativePosition{SettingsPageRelativePosition::None}
    , m_selector{SettingsPageSelector::Tabs}
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

SettingsPageSelector SettingsPage::selector() const
{
    return m_selector;
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

bool SettingsPage::matches(const QString& text)
{
    if(text.isEmpty()) {
        return true;
    }

    if(searchText(m_name).contains(text, Qt::CaseInsensitive)
       || searchText(m_category.join(' '_L1)).contains(text, Qt::CaseInsensitive)) {
        return true;
    }

    if(m_searchKeywords) {
        return m_searchKeywords->contains(text, Qt::CaseInsensitive);
    }

    if(!m_searchText) {
        QStringList keywords;
        if(auto* pageWidget = widget()) {
            const auto pageChildren = pageWidget->findChildren<QWidget*>();
            for(const auto* child : pageChildren) {
                if(child->window() != pageWidget->window()) {
                    continue;
                }

                if(const auto* label = qobject_cast<const QLabel*>(child)) {
                    QString labelText = label->text();
                    if(label->textFormat() == Qt::RichText
                       || (label->textFormat() == Qt::AutoText && Qt::mightBeRichText(labelText))) {
                        QTextDocument document;
                        document.setHtml(labelText);
                        labelText = document.toPlainText();
                    }
                    keywords.append(labelText);
                }
                else if(const auto* button = qobject_cast<const QAbstractButton*>(child)) {
                    keywords.append(button->text());
                }
                else if(const auto* group = qobject_cast<const QGroupBox*>(child)) {
                    keywords.append(group->title());
                }
            }
        }
        m_searchText = searchText(keywords.join('\n'_L1));
    }

    return m_searchText->contains(text, Qt::CaseInsensitive);
}

void SettingsPage::setSearchFilter(const QString& text)
{
    if(auto* pageWidget = qobject_cast<SettingsPageWidget*>(m_widget)) {
        pageWidget->setSearchFilter(text);
    }
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

    m_searchText.reset();
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

void SettingsPage::setSelector(SettingsPageSelector selector)
{
    m_selector = selector;
}

void SettingsPage::setSearchKeywords(const QStringList& keywords)
{
    m_searchKeywords = searchText(keywords.join('\n'_L1));
}

void SettingsPage::setWidgetCreator(const WidgetCreator& widgetCreator)
{
    m_widgetCreator = widgetCreator;
}
} // namespace Fooyin

#include "utils/settings/moc_settingspage.cpp"
