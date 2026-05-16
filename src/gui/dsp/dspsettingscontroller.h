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

#pragma once

#include <core/engine/enginedefs.h>

#include <QObject>

#include <optional>
#include <vector>

namespace Fooyin {
class DspChainStore;
class DspLayoutEditor;
class DspSettingsDialog;
class DspSettingsProvider;
class DspSettingsRegistry;
class FyWidget;

class DspSettingsController : public QObject
{
    Q_OBJECT

public:
    DspSettingsController(DspChainStore* chainStore, DspSettingsRegistry* registry, QObject* parent = nullptr);

    [[nodiscard]] QString displayName(const QString& dspId) const;
    [[nodiscard]] bool hasDsp(const QString& dspId) const;
    [[nodiscard]] FyWidget* createLayoutWidget(const QString& dspId, QWidget* parent = nullptr);
    [[nodiscard]] DspLayoutEditor* createLayoutEditor(const QString& dspId, QWidget* parent = nullptr) const;
    [[nodiscard]] DspSettingsDialog* createEditor(const QString& dspId, QWidget* parent) const;
    void showDialog(const QString& dspId, QWidget* parent = nullptr);

    struct Target
    {
        Engine::DspChainScope scope{Engine::DspChainScope::Master};
        uint64_t instanceId{0};
        QString name;
        QString label;
        QByteArray settings;
        bool enabled{true};
        bool supportsLive{false};
        Engine::DspChains originalChain;
    };

    [[nodiscard]] std::vector<Target> targetsFor(const QString& dspId) const;
    [[nodiscard]] std::optional<Target> targetFor(const QString& dspId) const;
    [[nodiscard]] std::optional<Target> targetForInstance(const QString& dspId, uint64_t instanceId) const;

    void bindEditor(DspSettingsDialog* editor, const Target& target, bool persistPreview);
    bool updateDspSettings(Engine::DspChainScope scope, uint64_t instanceId, const QByteArray& settings, bool persist);
    bool setDspEnabled(Engine::DspChainScope scope, uint64_t instanceId, bool enabled);

Q_SIGNALS:
    void dspInstancesChanged();

private:
    [[nodiscard]] DspSettingsProvider* providerFor(const QString& dspId) const;

    DspChainStore* m_chainStore;
    DspSettingsRegistry* m_registry;
};
} // namespace Fooyin
