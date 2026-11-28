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

#include "componentappearanceeditor.h"

#include <QDialog>
#include <QJsonObject>

#include <memory>
#include <vector>

class QComboBox;
class QDialogButtonBox;
class QDockWidget;
class QHideEvent;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QMainWindow;
class QPushButton;
class QShowEvent;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace Fooyin {
class AppearancePropertyPanel;
class FooyinStyle;
class HintLabel;

class ThemeAppearanceDialog : public QDialog
{
    Q_OBJECT

public:
    ThemeAppearanceDialog(QJsonObject appearance, const QPalette& palette, const QFont& font,
                          QWidget* parent = nullptr);
    ~ThemeAppearanceDialog() override;

    [[nodiscard]] QJsonObject appearance() const;

    void done(int result) override;

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void addGeneralPage(const QPalette& palette, const QFont& font);
    QTreeWidgetItem* addComponent(std::unique_ptr<ComponentAppearanceEditor> editor, const QPalette& palette,
                                  const QFont& font, QTreeWidgetItem* parent = nullptr);

    void selectItem(QTreeWidgetItem* item);
    void selectPreviewPart(ComponentAppearanceEditor* editor, const QString& part);
    void filterComponents(const QString& text);

    void updateValidation();
    void updateTransitions();
    void selectState();
    void updateStateOverrides();
    void resetPanelLayout();

    QJsonObject m_appearance;
    QMainWindow* m_workspace;
    QDockWidget* m_componentsDock;
    QDockWidget* m_propertiesDock;
    QTreeWidget* m_components;
    QLineEdit* m_componentFilter;
    QStackedWidget* m_previews;
    AppearancePropertyModel* m_propertyModel;
    AppearancePropertyPanel* m_properties;
    QWidget* m_generalPreview;
    AppearancePropertyPage* m_generalProperties;
    QLabel* m_propertyTitle;
    QWidget* m_stateControls;
    QComboBox* m_propertyState;
    QPushButton* m_resetPart;
    QPushButton* m_resetComponent;
    HintLabel* m_error;
    QDialogButtonBox* m_buttons;
    ComponentAppearanceEditor* m_activeEditor;
    AppearancePart m_activePart;
    QString m_selectedState;
    bool m_selectingPreview;
    std::unique_ptr<FooyinStyle> m_generalStyle;
    // Editors release their property pages and detach preview styles before QDialog destroys the widgets
    std::vector<std::unique_ptr<ComponentAppearanceEditor>> m_editors;
};
} // namespace Fooyin
