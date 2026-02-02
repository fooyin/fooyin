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

#include "outputcontroller.h"

#include "audiopipeline.h"

#include <QLoggingCategory>
#include <QMetaObject>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(ENGINE)

using namespace Qt::StringLiterals;

namespace {
QString describeFormat(const Fooyin::AudioFormat& format)
{
    if(!format.isValid()) {
        return u"invalid"_s;
    }

    return u"%1Hz %2ch %3"_s.arg(format.sampleRate()).arg(format.channelCount()).arg(format.prettyFormat())
         + u" [%1]"_s.arg(format.prettyChannelLayout());
}

} // namespace

namespace Fooyin {
OutputController::OutputController(QObject* signalTarget, AudioPipeline* pipeline)
    : m_signalTarget{signalTarget}
    , m_pipeline{pipeline}
{ }

void OutputController::setOutputCreator(const OutputCreator& outputCreator)
{
    m_outputCreator = outputCreator;
}

void OutputController::setOutputDevice(const QString& device)
{
    m_outputDevice = device;
}

const QString& OutputController::outputDevice() const
{
    return m_outputDevice;
}

bool OutputController::initOutput(const AudioFormat& format, double volume)
{
    const auto emitError = [this](const QString& error) {
        qCWarning(ENGINE) << error;
        if(m_signalTarget) {
            QMetaObject::invokeMethod(m_signalTarget, "deviceError", Qt::QueuedConnection, Q_ARG(QString, error));
        }
    };

    if(!m_outputCreator) {
        emitError(u"No audio output backend is configured"_s);
        return false;
    }

    auto output = m_outputCreator();
    if(!output) {
        emitError(u"Failed to create audio output backend instance"_s);
        return false;
    }

    if(!m_outputDevice.isEmpty()) {
        output->setDevice(m_outputDevice);
    }

    m_pipeline->setOutput(std::move(output));

    if(!m_pipeline->init(format)) {
        QString error = m_pipeline->lastInitError().trimmed();
        if(error.isEmpty()) {
            error = u"Failed to initialise audio output"_s;
        }

        emitError(error);
        return false;
    }

    m_pipeline->setOutputVolume(volume);

    const auto actualOutput = m_pipeline->outputFormat();

    const QString inputFormat  = describeFormat(format);
    const QString actualFormat = describeFormat(actualOutput);
    const QString deviceLabel  = m_outputDevice.isEmpty() ? u"default"_s : m_outputDevice;

    if(inputFormat == actualFormat) {
        qCInfo(ENGINE) << "Output initialized:" << actualFormat << "(device:" << deviceLabel << ")";
    }
    else {
        qCInfo(ENGINE) << "Output initialized: input" << inputFormat << "-> output" << actualFormat
                       << "(device:" << deviceLabel << ")";
    }

    return true;
}

void OutputController::uninitOutput()
{
    m_pipeline->uninit();
}

void OutputController::pauseOutputImmediate(bool resetOutput)
{
    m_pipeline->pause();

    if(resetOutput) {
        m_pipeline->resetOutput();
    }
}
} // namespace Fooyin
