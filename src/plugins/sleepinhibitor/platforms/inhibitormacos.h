/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
 * Copyright © 2026, Gustav Oechler <gustavoechler@gmail.com>
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

#include "inhibitor.h"

#include <QFuture>

#include <memory>
#include <optional>
#include <vector>

namespace Fooyin::SleepInhibitor {
class InhibitorMacOs : public InhibitorPrivate
{
    Q_OBJECT

public:
    explicit InhibitorMacOs(QObject* parent = nullptr);
    ~InhibitorMacOs() override;

    void inhibitSleep(InhibitionType type) override;
    void uninhibitSleep() override;

private:
    struct AssertionState;

    void setDesiredType(std::optional<InhibitionType> type);
    void startReconciliation();
    static void reconcile(const std::shared_ptr<AssertionState>& assertionState, InhibitorMacOs* inhibitor);

    std::shared_ptr<AssertionState> m_assertionState;
    std::vector<QFuture<void>> m_operations;
    std::optional<InhibitionType> m_desiredType;
};
} // namespace Fooyin::SleepInhibitor
