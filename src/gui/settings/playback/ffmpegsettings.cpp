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

#include "ffmpegsettings.h"

#include <core/engine/audioloader.h>
#include <core/internalcoresettings.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
FFmpegSettings::FFmpegSettings(AudioLoader* audioLoader, SettingsManager* settings, QWidget* parent)
    : QDialog{parent}
    , m_audioLoader{audioLoader}
    , m_settings{settings}
    , m_allExtensions{new QCheckBox(tr("Enable all supported formats"), this)}
{
    setWindowTitle(tr("FFmpeg Settings"));

    m_allExtensions->setChecked(m_settings->fileValue(Settings::Core::Internal::FFmpegAllExtensions, false).toBool());

    auto* buttons
        = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset, this);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &FFmpegSettings::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &FFmpegSettings::reject);
    QObject::connect(buttons->button(QDialogButtonBox::Reset), &QAbstractButton::clicked, this,
                     [this] { m_allExtensions->setChecked(false); });

    auto* layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->addWidget(m_allExtensions);
    layout->addWidget(buttons);
}

void FFmpegSettings::accept()
{
    if(m_settings->fileSet(Settings::Core::Internal::FFmpegAllExtensions, m_allExtensions->isChecked())) {
        m_audioLoader->reloadDecoderExtensions(u"FFmpeg"_s);
        m_audioLoader->reloadReaderExtensions(u"FFmpeg"_s);
    }

    done(Accepted);
}
} // namespace Fooyin
