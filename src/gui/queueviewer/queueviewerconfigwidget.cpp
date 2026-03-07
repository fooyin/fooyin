/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "queueviewerconfigwidget.h"

#include <gui/widgets/scriptlineedit.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>

using namespace Qt::StringLiterals;

namespace Fooyin {
QueueViewerConfigDialog::QueueViewerConfigDialog(QueueViewer* queueViewer, QWidget* parent)
    : WidgetConfigDialog{queueViewer, tr("Configure %1").arg(queueViewer->name()), parent}
    , m_titleScript{new ScriptLineEdit(this)}
    , m_subtitleScript{new ScriptLineEdit(this)}
    , m_headers{new QCheckBox(tr("Show header"), this)}
    , m_scrollBars{new QCheckBox(tr("Show scrollbar"), this)}
    , m_altRowColours{new QCheckBox(tr("Alternating row colours"), this)}
    , m_showCurrent{new QCheckBox(tr("Show playing queue track"), this)}
    , m_showIcon{new QGroupBox(tr("Icon"), this)}
    , m_iconWidth{new QSpinBox(this)}
    , m_iconHeight{new QSpinBox(this)}
{
    auto* general       = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(general);

    generalLayout->addWidget(new QLabel(tr("Left script") + u":"_s, this), 0, 0);
    generalLayout->addWidget(m_titleScript, 0, 1);
    generalLayout->addWidget(new QLabel(tr("Right script") + u":"_s, this), 1, 0);
    generalLayout->addWidget(m_subtitleScript, 1, 1);

    auto* appearance       = new QGroupBox(tr("Appearance"), this);
    auto* appearanceLayout = new QGridLayout(appearance);

    m_showIcon->setCheckable(true);

    auto* iconGroupLayout = new QGridLayout(m_showIcon);

    m_iconWidth->setSuffix(u"px"_s);
    m_iconHeight->setSuffix(u"px"_s);
    m_iconWidth->setMaximum(512);
    m_iconHeight->setMaximum(512);
    m_iconWidth->setSingleStep(5);
    m_iconHeight->setSingleStep(5);

    auto* iconSizeHint = new QLabel(
        u"🛈 "_s + tr("Size can also be changed using %1 in the widget.").arg(u"<b>Ctrl+Scroll</b>"_s), this);
    iconSizeHint->setTextFormat(Qt::RichText);

    int row{0};
    iconGroupLayout->addWidget(new QLabel(tr("Width") + u":"_s, this), row, 0);
    iconGroupLayout->addWidget(m_iconWidth, row++, 1);
    iconGroupLayout->addWidget(new QLabel(tr("Height") + u":"_s, this), row, 0);
    iconGroupLayout->addWidget(m_iconHeight, row++, 1);
    iconGroupLayout->addWidget(iconSizeHint, row, 0, 1, 4);
    iconGroupLayout->setColumnStretch(2, 1);

    row = 0;
    appearanceLayout->addWidget(m_headers, row++, 0);
    appearanceLayout->addWidget(m_scrollBars, row++, 0);
    appearanceLayout->addWidget(m_altRowColours, row++, 0);
    appearanceLayout->addWidget(m_showCurrent, row++, 0);
    appearanceLayout->addWidget(m_showIcon, row++, 0);

    auto* layout = contentLayout();
    layout->addWidget(general, 0, 0);
    layout->addWidget(appearance, 1, 0);
    layout->setColumnStretch(0, 1);
    layout->setRowStretch(2, 1);

    loadCurrentConfig();
}

void QueueViewerConfigDialog::setConfig(const QueueViewer::ConfigData& config)
{
    m_titleScript->setText(config.leftScript);
    m_subtitleScript->setText(config.rightScript);
    m_headers->setChecked(config.showHeader);
    m_scrollBars->setChecked(config.showScrollBar);
    m_altRowColours->setChecked(config.alternatingRows);
    m_showCurrent->setChecked(config.showCurrent);
    m_showIcon->setChecked(config.showIcon);
    m_iconWidth->setValue(config.iconSize.width());
    m_iconHeight->setValue(config.iconSize.height());
}

QueueViewer::ConfigData QueueViewerConfigDialog::config() const
{
    return {
        .leftScript      = m_titleScript->text(),
        .rightScript     = m_subtitleScript->text(),
        .showCurrent     = m_showCurrent->isChecked(),
        .showIcon        = m_showIcon->isChecked(),
        .iconSize        = {m_iconWidth->value(), m_iconHeight->value()},
        .showHeader      = m_headers->isChecked(),
        .showScrollBar   = m_scrollBars->isChecked(),
        .alternatingRows = m_altRowColours->isChecked(),
    };
}
} // namespace Fooyin
