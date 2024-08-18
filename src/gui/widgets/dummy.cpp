/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "dummy.h"

#include <gui/guisettings.h>
#include <utils/settings/settingsmanager.h>

#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>

namespace Fooyin {
Dummy::Dummy(SettingsManager* settings, QWidget* parent)
    : Dummy{{}, settings, parent}
{ }

Dummy::Dummy(QString name, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_settings{settings}
    , m_missingName{std::move(name)}
    , m_label{new QLabel(this)}
{
    setObjectName(Dummy::name());

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QPalette palette = QWidget::palette();
    palette.setColor(m_label->backgroundRole(), palette.base().color());

    m_label->setPalette(palette);
    m_label->setAutoFillBackground(true);
    m_label->setWordWrap(true);
    m_label->setFrameShape(QFrame::StyledPanel);
    m_label->setFrameShadow(QFrame::Sunken);
    m_label->setAlignment(Qt::AlignCenter);

    layout->addWidget(m_label);

    m_settings->subscribe<Settings::Gui::LayoutEditing>(this, &Dummy::updateText);

    updateText();
}

QString Dummy::name() const
{
    return tr("Dummy");
}

QString Dummy::layoutName() const
{
    return QStringLiteral("Dummy");
}

void Dummy::saveLayoutData(QJsonObject& layout)
{
    if(!m_missingName.isEmpty()) {
        layout[QStringLiteral("MissingWidget")] = m_missingName;
    }
}

void Dummy::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(QStringLiteral("MissingWidget"))) {
        m_missingName = layout.value(QStringLiteral("MissingWidget")).toString();
    }
}

QString Dummy::missingName() const
{
    return m_missingName;
}

void Dummy::updateText()
{
    const bool isEditing = m_settings->value<Settings::Gui::LayoutEditing>();

    if(!m_missingName.isEmpty()) {
        m_label->setText(tr("Missing Widget") + QStringLiteral(":\n") + m_missingName);
    }
    else if(isEditing) {
        m_label->setText(tr("Right-click to add a new widget"));
    }
    else {
        m_label->setText(tr("Enter layout editing mode to edit"));
    }
}
} // namespace Fooyin

#include "moc_dummy.cpp"
