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
#include "theme/inputappearance.h"

#include <memory>
#include <vector>

class QComboBox;
class QGroupBox;

namespace Fooyin {
class FooyinStyle;

class InputAppearanceEditor : public ComponentAppearanceEditor
{
    Q_OBJECT

public:
    InputAppearanceEditor(AppearancePropertyModel* propertyModel, InputType type);
    ~InputAppearanceEditor() override;

    [[nodiscard]] bool secondaryPreview() const;

    void selectPart(const QString& part, bool updatePreviewState) override;

    bool eventFilter(QObject* watched, QEvent* event) override;

protected:
    void updatePreviewPalette(const QPalette& palette, const QFont& font) override;
    void updatePreview() override;
    void updatePreviewState(PreviewState state) override;

private:
    InputType m_type;
    std::unique_ptr<FooyinStyle> m_previewStyle;
    QGroupBox* m_preview;
    QWidget* m_readOnlySample;
    std::vector<QWidget*> m_samples;
    QComboBox* m_previewArea;
    bool m_secondaryPreview;
};
} // namespace Fooyin
