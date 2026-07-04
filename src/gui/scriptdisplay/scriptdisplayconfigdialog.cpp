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

#include "scriptdisplayconfigdialog.h"

#include "gui/widgets/colourbutton.h"
#include "gui/widgets/fontbutton.h"
#include "gui/widgets/scriptlineedit.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>

using namespace Qt::StringLiterals;

namespace {
QFont fontFromString(const QString& fontString)
{
    QFont font;
    if(!fontString.isEmpty()) {
        font.fromString(fontString);
    }
    return font;
}

QString normaliseColour(const QString& colour)
{
    const QColor parsedColour{colour};
    return parsedColour.isValid() ? parsedColour.name(QColor::HexArgb) : QString{};
}

QColor colourOrDefault(const QString& colour, const QColor& defaultColour)
{
    const QColor parsedColour{colour};
    return parsedColour.isValid() ? parsedColour : defaultColour;
}
} // namespace

namespace Fooyin {
ScriptDisplayConfigDialog::ScriptDisplayConfigDialog(ScriptDisplay* widget, QWidget* parent)
    : WidgetConfigDialog<ScriptDisplay, ScriptDisplay::ConfigData>{widget, widget->name(), parent}
    , m_script{new ScriptTextEdit(this)}
    , m_tabs{new QTabWidget(this)}
    , m_formatTab{new QWidget(this)}
    , m_styleTab{new QWidget(this)}
    , m_horizontalAlignment{new QComboBox(this)}
    , m_verticalAlignment{new QComboBox(this)}
    , m_font{new FontButton(tr("Default") + u":"_s, true, this)}
    , m_textColour{new ColourButton(tr("Text"), true, this)}
    , m_backgroundColour{new ColourButton(tr("Background"), true, this)}
    , m_linkColour{new ColourButton(tr("Links"), true, this)}
{
    auto* layout{contentLayout()};
    layout->addWidget(m_tabs, 0, 0);
    layout->setRowStretch(0, 1);

    m_tabs->setDocumentMode(true);
    m_tabs->addTab(m_formatTab, tr("Format"));
    m_tabs->addTab(m_styleTab, tr("Appearance"));

    m_horizontalAlignment->addItem(tr("Left"), Qt::AlignLeft);
    m_horizontalAlignment->addItem(tr("Centre"), Qt::AlignHCenter);
    m_horizontalAlignment->addItem(tr("Right"), Qt::AlignRight);

    m_verticalAlignment->addItem(tr("Top"), Qt::AlignTop);
    m_verticalAlignment->addItem(tr("Centre"), Qt::AlignVCenter);
    m_verticalAlignment->addItem(tr("Bottom"), Qt::AlignBottom);

    auto* alignmentGroup  = new QGroupBox(tr("Alignment"), m_styleTab);
    auto* alignmentLayout = new QGridLayout(alignmentGroup);

    alignmentLayout->addWidget(new QLabel(tr("Horizontal") + u":"_s, this), 0, 0);
    alignmentLayout->addWidget(m_horizontalAlignment, 0, 1);
    alignmentLayout->addWidget(new QLabel(tr("Vertical") + u":"_s, this), 1, 0);
    alignmentLayout->addWidget(m_verticalAlignment, 1, 1);
    alignmentLayout->setColumnStretch(2, 1);

    auto* fontGroup  = new QGroupBox(tr("Font"), m_styleTab);
    auto* fontLayout = new QGridLayout(fontGroup);

    fontLayout->addWidget(m_font, 0, 0, 1, 2);
    fontLayout->setColumnStretch(2, 1);

    auto* colourGroup  = new QGroupBox(tr("Colours"), m_styleTab);
    auto* colourLayout = new QGridLayout(colourGroup);

    ColourButton::alignLabels({m_textColour, m_backgroundColour, m_linkColour});

    colourLayout->addWidget(m_textColour, 0, 0, 1, 2);
    colourLayout->addWidget(m_backgroundColour, 1, 0, 1, 2);
    colourLayout->addWidget(m_linkColour, 2, 0, 1, 2);
    colourLayout->setColumnStretch(1, 1);

    auto* styleLayout = new QGridLayout(m_styleTab);

    styleLayout->addWidget(alignmentGroup, 0, 0);
    styleLayout->addWidget(fontGroup, 1, 0);
    styleLayout->addWidget(colourGroup, 2, 0);
    styleLayout->setRowStretch(4, 1);

    auto* formatLayout = new QGridLayout(m_formatTab);
    formatLayout->setContentsMargins({});

    formatLayout->addWidget(m_script, 1, 0);

    loadCurrentConfig();
}

ScriptDisplay::ConfigData ScriptDisplayConfigDialog::config() const
{
    return {
        .script = m_script->text(),

        .font = m_font->isChecked() ? m_font->buttonFont().toString() : QString{},

        .bgColour = m_backgroundColour->isChecked()
                      ? normaliseColour(m_backgroundColour->colour().name(QColor::HexArgb))
                      : QString{},
        .fgColour
        = m_textColour->isChecked() ? normaliseColour(m_textColour->colour().name(QColor::HexArgb)) : QString{},
        .linkColour
        = m_linkColour->isChecked() ? normaliseColour(m_linkColour->colour().name(QColor::HexArgb)) : QString{},

        .horizontalAlignment = m_horizontalAlignment->currentData().toInt(),
        .verticalAlignment   = m_verticalAlignment->currentData().toInt(),
    };
}

void ScriptDisplayConfigDialog::setConfig(const ScriptDisplay::ConfigData& config)
{
    m_script->setText(config.script);

    const QFont defaultFont = config.font.isEmpty() ? widget()->font() : fontFromString(config.font);
    m_font->setChecked(!config.font.isEmpty());
    m_font->setButtonFont(defaultFont);

    m_textColour->setChecked(!config.fgColour.isEmpty());
    m_textColour->setColour(colourOrDefault(config.fgColour, widget()->palette().color(QPalette::WindowText)));

    m_backgroundColour->setChecked(!config.bgColour.isEmpty());
    m_backgroundColour->setColour(colourOrDefault(config.bgColour, widget()->palette().color(QPalette::Window)));

    m_linkColour->setChecked(!config.linkColour.isEmpty());
    m_linkColour->setColour(colourOrDefault(config.linkColour, widget()->palette().color(QPalette::Link)));

    if(const int index = m_horizontalAlignment->findData(config.horizontalAlignment); index >= 0) {
        m_horizontalAlignment->setCurrentIndex(index);
    }
    if(const int index = m_verticalAlignment->findData(config.verticalAlignment); index >= 0) {
        m_verticalAlignment->setCurrentIndex(index);
    }
}
} // namespace Fooyin
