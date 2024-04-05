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

#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>

namespace Fooyin {
Dummy::Dummy(QWidget* parent)
    : Dummy{{}, parent}
{ }

Dummy::Dummy(QString name, QWidget* parent)
    : FyWidget{parent}
    , m_missingName{std::move(name)}
{
    setObjectName(Dummy::name());

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* label = new QLabel(!m_missingName.isEmpty() ? tr("Missing Widget:\n") + m_missingName
                                                      : tr("Right-Click to add a new widget"),
                             this);
    label->setAutoFillBackground(true);

    QPalette palette = QWidget::palette();
    palette.setColor(label->backgroundRole(), palette.base().color());
    label->setPalette(palette);

    label->setFrameShape(QFrame::StyledPanel);
    label->setFrameShadow(QFrame::Sunken);
    label->setAlignment(Qt::AlignCenter);

    layout->addWidget(label);
}

QString Dummy::name() const
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
} // namespace Fooyin

#include "moc_dummy.cpp"
