/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "fyutils_export.h"

#include <QHeaderView>

namespace Fooyin {
class FYUTILS_EXPORT AutoHeaderView : public QHeaderView
{
    Q_OBJECT

public:
    enum Role
    {
        SectionAlignment = Qt::UserRole + 100
    };

    explicit AutoHeaderView(Qt::Orientation orientation, QWidget* parent = nullptr);
    ~AutoHeaderView() override;

    void setModel(QAbstractItemModel* model) override;

    void resetSections();
    void resetSectionPositions();

    void hideHeaderSection(int logical);
    void showHeaderSection(int logical);
    void setHeaderSectionHidden(int logical, bool hidden);

    void setHeaderSectionWidth(int logical, double width);
    void setHeaderSectionWidths(const std::map<int, double>& widths);
    void setHeaderSectionAlignment(int logical, Qt::Alignment alignment);

    [[nodiscard]] bool isStretchEnabled() const;
    void setStretchEnabled(bool enabled);

    void addHeaderContextMenu(QMenu* menu, const QPoint& pos = {});
    void addHeaderAlignmentMenu(QMenu* menu, const QPoint& pos = {});

    [[nodiscard]] QByteArray saveHeaderState() const;
    void restoreHeaderState(const QByteArray& data);

signals:
    void stretchChanged(bool enabled);
    void stateRestored();
    void leftClicked(int section, const QPoint& pos);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
