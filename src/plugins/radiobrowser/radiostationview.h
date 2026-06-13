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

#include <gui/widgets/expandedtreeview.h>

class QPushButton;
class QResizeEvent;

namespace Fooyin {
class AutoHeaderView;

namespace RadioBrowser {
class RadioStationView : public ExpandedTreeView
{
    Q_OBJECT

public:
    explicit RadioStationView(QWidget* parent = nullptr);

    [[nodiscard]] AutoHeaderView* stationHeader() const;

    void clearSort();
    void finaliseView(const QByteArray& headerState);
    void resetColumnsToDefault();
    void setEmptyText(const QString& text);
    void setFailureText(const QString& text);
    void setLoading(bool loading);
    void setLoadingText(const QString& text);

Q_SIGNALS:
    void displayChanged();
    void retryRequested();

protected:
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateRetryButton();

    AutoHeaderView* m_header;
    QPushButton* m_retryButton;
    QString m_emptyText;
    QString m_failureText;
    QString m_loadingText;
    bool m_loading;
    bool m_failed;
};
} // namespace RadioBrowser
} // namespace Fooyin
