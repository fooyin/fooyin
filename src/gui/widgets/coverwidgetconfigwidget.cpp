/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include "coverwidgetconfigwidget.h"

#include <gui/widgets/slidereditor.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
CoverWidgetConfigDialog::CoverWidgetConfigDialog(CoverWidget* coverWidget, QWidget* parent)
    : WidgetConfigDialog{coverWidget, tr("Configure %1").arg(coverWidget->name()), parent}
    , m_coverTypeGroup{new QButtonGroup(this)}
    , m_alignmentGroup{new QButtonGroup(this)}
    , m_keepAspectRatio{new QCheckBox(tr("Keep aspect ratio"), this)}
    , m_fadeEnabled{new QCheckBox(tr("Fade cover changes"), this)}
    , m_fadeDuration{new SliderEditor(tr("Fade length"), this)}
{
    auto* displayGroup  = new QGroupBox(tr("Display"), this);
    auto* displayLayout = new QGridLayout(displayGroup);

    auto* coverTypeBox = new QGroupBox(tr("Cover type"), displayGroup);

    auto* frontButton  = new QRadioButton(tr("Front"), coverTypeBox);
    auto* backButton   = new QRadioButton(tr("Back"), coverTypeBox);
    auto* artistButton = new QRadioButton(tr("Artist"), coverTypeBox);

    auto* coverTypeLayout = new QVBoxLayout(coverTypeBox);
    coverTypeLayout->addWidget(frontButton);
    coverTypeLayout->addWidget(backButton);
    coverTypeLayout->addWidget(artistButton);
    coverTypeLayout->addStretch();

    m_coverTypeGroup->addButton(frontButton, static_cast<int>(Track::Cover::Front));
    m_coverTypeGroup->addButton(backButton, static_cast<int>(Track::Cover::Back));
    m_coverTypeGroup->addButton(artistButton, static_cast<int>(Track::Cover::Artist));

    auto* alignmentBox = new QGroupBox(tr("Alignment"), displayGroup);

    auto* centreButton = new QRadioButton(tr("Centre"), alignmentBox);
    auto* leftButton   = new QRadioButton(tr("Left"), alignmentBox);
    auto* rightButton  = new QRadioButton(tr("Right"), alignmentBox);

    auto* alignmentLayout = new QVBoxLayout(alignmentBox);
    alignmentLayout->addWidget(centreButton);
    alignmentLayout->addWidget(leftButton);
    alignmentLayout->addWidget(rightButton);
    alignmentLayout->addStretch();

    m_alignmentGroup->addButton(centreButton, Qt::AlignCenter);
    m_alignmentGroup->addButton(leftButton, Qt::AlignLeft);
    m_alignmentGroup->addButton(rightButton, Qt::AlignRight);

    int row{0};
    displayLayout->addWidget(coverTypeBox, row, 0);
    displayLayout->addWidget(alignmentBox, row++, 1);
    displayLayout->addWidget(m_keepAspectRatio, row++, 0, 1, 2);
    displayLayout->setColumnStretch(0, 1);
    displayLayout->setColumnStretch(1, 1);

    auto* fadeGroup  = new QGroupBox(tr("Cover change"), this);
    auto* fadeLayout = new QGridLayout(fadeGroup);

    m_fadeDuration->setRange(50, 3000);
    m_fadeDuration->setSingleStep(50);
    m_fadeDuration->setSuffix(u" ms"_s);

    row = 0;
    fadeLayout->addWidget(m_fadeEnabled, row++, 0);
    fadeLayout->addWidget(m_fadeDuration, row++, 0);
    fadeLayout->setRowStretch(row, 1);

    auto* layout{contentLayout()};
    layout->addWidget(displayGroup, 0, 0);
    layout->addWidget(fadeGroup, 1, 0);
    layout->setColumnStretch(0, 1);
    layout->setRowStretch(2, 1);

    loadCurrentConfig();
}

void CoverWidgetConfigDialog::setConfig(const CoverWidget::ConfigData& config)
{
    if(auto* button = m_coverTypeGroup->button(static_cast<int>(config.coverType))) {
        button->setChecked(true);
    }
    if(auto* button = m_alignmentGroup->button(config.coverAlignment)) {
        button->setChecked(true);
    }

    m_keepAspectRatio->setChecked(config.keepAspectRatio);
    m_fadeEnabled->setChecked(config.fadeCoverChanges);
    m_fadeDuration->setValue(config.fadeDurationMs);
}

CoverWidget::ConfigData CoverWidgetConfigDialog::config() const
{
    return {
        .coverType        = static_cast<Track::Cover>(m_coverTypeGroup->checkedId()),
        .coverAlignment   = static_cast<Qt::Alignment>(m_alignmentGroup->checkedId()),
        .keepAspectRatio  = m_keepAspectRatio->isChecked(),
        .fadeCoverChanges = m_fadeEnabled->isChecked(),
        .fadeDurationMs   = m_fadeDuration->value(),
    };
}
} // namespace Fooyin
