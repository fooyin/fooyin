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

#include <core/engine/visualisationservice.h>

#include "visualisationbackend.h"

#include <QPointer>

#include <utility>

namespace Fooyin {
class VisualisationSessionPrivate
{
public:
    VisualisationSessionPrivate(std::shared_ptr<VisualisationBackend> backend_, VisualisationService* service_)
        : backend{std::move(backend_)}
        , service{service_}
        , token{this->backend ? this->backend->registerSession() : 0}
    {
        if(this->service) {
            Q_EMIT this->service->sessionActivityChanged();
        }
    }

    ~VisualisationSessionPrivate()
    {
        if(backend && token != 0) {
            backend->unregisterSession(token);
        }
        if(service) {
            Q_EMIT service->sessionActivityChanged();
        }
    }

    std::shared_ptr<VisualisationBackend> backend;
    QPointer<VisualisationService> service;
    VisualisationBackend::SessionToken token{0};
    VisualisationSession::ChannelSelection channelSelection;
};

VisualisationSession::VisualisationSession(std::shared_ptr<VisualisationBackend> backend, VisualisationService* service)
    : p{std::make_unique<VisualisationSessionPrivate>(std::move(backend), service)}
{ }

VisualisationSession::~VisualisationSession() = default;

void VisualisationSession::requestBacklog(uint64_t durationMs)
{
    if(p->backend && p->token != 0) {
        p->backend->requestBacklog(p->token, durationMs);
    }
}

void VisualisationSession::setChannelSelection(ChannelSelection selection)
{
    p->channelSelection = selection;
}

uint64_t VisualisationSession::currentTimeMs() const
{
    return p->backend ? p->backend->currentTimeMs() : 0;
}

bool VisualisationSession::getPcmWindow(PcmWindow& out, uint64_t centerTimeMs, uint64_t durationMs) const
{
    return p->backend && p->backend->getPcmWindow(out, centerTimeMs, durationMs, p->channelSelection);
}

bool VisualisationSession::getPcmWindowEndingAt(PcmWindow& out, uint64_t endTimeMs, uint64_t durationMs) const
{
    return p->backend && p->backend->getPcmWindowEndingAt(out, endTimeMs, durationMs, p->channelSelection);
}

bool VisualisationSession::getSpectrumWindow(SpectrumWindow& out, uint64_t centerTimeMs, int fftSize,
                                             SpectrumWindowFunction windowFunction) const
{
    return p->backend && p->backend->getSpectrumWindow(out, centerTimeMs, fftSize, p->channelSelection, windowFunction);
}

bool VisualisationSession::getSpectrumWindowEndingAt(SpectrumWindow& out, uint64_t endTimeMs, int fftSize,
                                                     SpectrumWindowFunction windowFunction) const
{
    return p->backend
        && p->backend->getSpectrumWindowEndingAt(out, endTimeMs, fftSize, p->channelSelection, windowFunction);
}

class VisualisationServicePrivate
{
public:
    std::shared_ptr<VisualisationBackend> backend{std::make_shared<VisualisationBackend>()};
};

VisualisationService::VisualisationService(QObject* parent)
    : QObject{parent}
    , p{std::make_unique<VisualisationServicePrivate>()}
{ }

VisualisationService::~VisualisationService() = default;

VisualisationSessionPtr VisualisationService::createSession()
{
    return VisualisationSessionPtr{new VisualisationSession{p->backend, this}};
}

std::shared_ptr<VisualisationBackend> VisualisationService::backend() const
{
    return p->backend;
}

bool VisualisationService::hasActiveSessions() const
{
    return p->backend && p->backend->hasActiveSessions();
}
} // namespace Fooyin
