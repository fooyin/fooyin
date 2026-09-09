/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "scrobblercache.h"

#include <QBasicTimer>
#include <QObject>

#include <map>
#include <optional>

namespace Fooyin::Scrobbler {
struct LovedItem
{
    QString key;
    Metadata metadata;
    bool loved{false};
    uint64_t revision{0};
};

class LovedCache : public QObject
{
    Q_OBJECT

public:
    explicit LovedCache(QString filepath, QObject* parent = nullptr);

    void set(Metadata metadata, bool loved);
    [[nodiscard]] bool contains(const Metadata& metadata) const;
    [[nodiscard]] std::optional<LovedItem> first() const;
    void remove(const QString& key, uint64_t revision);
    [[nodiscard]] int count() const;

    void writeCache();

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    void readCache();
    void scheduleWrite();
    static QString itemKey(const Metadata& metadata);

    QString m_filepath;
    std::map<QString, LovedItem> m_items;
    uint64_t m_revision{0};
    QBasicTimer m_writeTimer;
};
} // namespace Fooyin::Scrobbler
