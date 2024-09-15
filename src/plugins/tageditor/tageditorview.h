/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <gui/widgets/extendabletableview.h>

namespace Fooyin {
class StarDelegate;
class WidgetContext;

namespace TagEditor {
class TagEditorView : public ExtendableTableView
{
    Q_OBJECT

public:
    explicit TagEditorView(ActionManager* actionManager, QWidget* parent = nullptr);

    void setTagEditTriggers(EditTrigger triggers);
    void setupActions();
    void setRatingRow(int row);

    [[nodiscard]] int sizeHintForRow(int row) const override;

protected:
    void setupContextActions(QMenu* menu, const QPoint& pos) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void copySelection();
    void pasteSelection(bool match);
    void ratingHoverIn(const QModelIndex& index, const QPoint& pos);
    void ratingHoverOut();

    ActionManager* m_actionManager;

    EditTrigger m_editTrigger;
    WidgetContext* m_context;
    QAction* m_copyAction;
    QAction* m_pasteAction;
    QAction* m_pasteFields;

    int m_ratingRow;
    StarDelegate* m_starDelegate;
};
} // namespace TagEditor
} // namespace Fooyin
