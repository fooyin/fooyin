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

#include "appearancepropertypage.h"

#include <gui/brushsource.h>

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QWidget>

#include <initializer_list>
#include <map>
#include <memory>
#include <vector>

namespace Fooyin {
enum class PreviewState : uint8_t
{
    Normal = 0,
    Hovered,
    Pressed,
    Focused,
    Disabled,
    Checked,
    DefaultButton,
    ReadOnly,
};

struct AppearanceState
{
    QString id;
    QString title;
    QString pageId;
    PreviewState previewState{PreviewState::Normal};
    std::vector<QString> inheritanceGroups{};
};

struct AppearancePart
{
    QString id;
    QString title;
    QString parentId;
    std::vector<AppearanceState> states{};
    PreviewState previewState{PreviewState::Normal};
};

QJsonObject updateAppearanceProperty(QJsonObject object, QStringView group, const QString& key,
                                     const QJsonValue& value);

class ComponentAppearanceEditor : public QObject
{
    Q_OBJECT

public:
    ComponentAppearanceEditor(AppearancePropertyModel* model, QString key, QString title);
    ~ComponentAppearanceEditor() override;

    [[nodiscard]] QString key() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] QWidget* previewWidget() const;

    [[nodiscard]] QJsonObject overrides() const;
    [[nodiscard]] bool hasGroupOverrides(const QString& group) const;
    void setOverrides(const QJsonObject& overrides);

    [[nodiscard]] AppearancePropertyPage* currentPropertyPage() const;
    [[nodiscard]] const std::vector<AppearancePart>& parts() const;
    [[nodiscard]] QString validationError() const;

    virtual void selectPart(const QString& part, bool updatePreviewState) = 0;
    void setPreviewPalette(const QPalette& palette, const QFont& font);

    [[nodiscard]] PreviewState previewState() const;
    void setPreviewState(PreviewState state);

    bool eventFilter(QObject* watched, QEvent* event) override;

Q_SIGNALS:
    void changed();
    void partSelected(const QString& part);
    void previewStateRequested(Fooyin::PreviewState state);

protected:
    AppearancePropertyPage* addPage(const QString& id);
    AppearancePropertyPage* addPartPage(const QString& id, const QString& title, const QString& parent = {},
                                        PreviewState previewState = PreviewState::Normal);

    void addPart(AppearancePart part);
    void setParts(std::vector<AppearancePart> parts);

    [[nodiscard]] QString propertyPageId(const QString& part) const;
    [[nodiscard]] QString previewPage(const QString& part, PreviewState state) const;
    bool showPage(const QString& id);
    void preparePreview(QWidget* widget);
    void setPreviewStyle(QWidget* widget, QStyle* style);
    void requestPreviewState(const QString& page);

    [[nodiscard]] QPalette::ColorGroup stateColourGroup(const QString& page) const;

    void setValidationError(QString error);

    AppearanceProperty* addChoice(AppearancePropertyPage* page, const QString& label, const QString& group,
                                  const QString& key, const QString& defaultValue,
                                  std::initializer_list<std::pair<QString, QString>> choices);
    AppearanceProperty* addGroup(AppearancePropertyPage* page, const QString& label, const QString& group,
                                 const QString& key);
    AppearanceProperty* addBool(AppearancePropertyPage* page, const QString& label, const QString& group,
                                const QString& key, bool defaultValue);
    AppearanceProperty* addNumber(AppearancePropertyPage* page, const QString& label, const QString& group,
                                  const QString& key, double defaultValue, double minimum, double maximum,
                                  int decimals);

    AppearanceProperty* addColour(AppearancePropertyPage* page, const QString& label, const QString& group,
                                  const QString& key, const ColourSource& defaultValue,
                                  QPalette::ColorGroup colourGroup = QPalette::Active);

    AppearanceProperty* addBrush(AppearancePropertyPage* page, const QString& label, const QString& group,
                                 const QString& key, const BrushSource& defaultValue,
                                 QPalette::ColorGroup colourGroup = QPalette::Active);

    virtual void updatePreviewPalette(const QPalette& palette, const QFont& font) = 0;
    virtual void updatePreview()                                                  = 0;
    virtual void updatePreviewState(PreviewState state)                           = 0;

private:
    AppearanceProperty* addProperty(AppearancePropertyPage* page, AppearanceProperty property);
    void editProperties(const std::vector<AppearancePropertyEdit>& edits);
    void refreshProperties();

    QString m_key;
    QString m_title;
    QPointer<QWidget> m_previewWidget;
    QJsonObject m_overrides;
    QString m_validationError;
    PreviewState m_previewState;
    AppearancePropertyModel* m_propertyModel;
    AppearancePropertyPage* m_currentPropertyPage;
    std::map<QString, std::unique_ptr<AppearancePropertyPage>> m_pages;
    std::vector<AppearancePart> m_parts;
    std::map<QString, std::vector<QString>> m_propertyKeys;
};
} // namespace Fooyin
