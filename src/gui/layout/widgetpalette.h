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

#include <QTreeWidget>
#include <QWidget>

class QLineEdit;

namespace Fooyin {
class SettingsManager;
class WidgetProvider;

class PaletteTree : public QTreeWidget
{
    Q_OBJECT

public:
    explicit PaletteTree(QWidget* parent);

Q_SIGNALS:
    void widgetDragRequested(const QString& key);

protected:
    void startDrag(Qt::DropActions /*supportedActions*/) override;
};

class WidgetPalette : public QWidget
{
    Q_OBJECT

public:
    WidgetPalette(WidgetProvider* provider, SettingsManager* settings, QWidget* parent = nullptr);

    void refresh();

Q_SIGNALS:
    void widgetDragRequested(const QString& key);

protected:
    void showEvent(QShowEvent* event) override;

private:
    WidgetProvider* m_provider;
    SettingsManager* m_settings;

    QLineEdit* m_search;
    PaletteTree* m_entries;
};
} // namespace Fooyin
