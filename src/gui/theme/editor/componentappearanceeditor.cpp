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

#include "componentappearanceeditor.h"

#include <QEvent>
#include <QPalette>

#include <ranges>

using namespace Qt::StringLiterals;

namespace Fooyin {
QJsonObject updateAppearanceProperty(QJsonObject object, QStringView group, const QString& key, const QJsonValue& value)
{
    if(group.isEmpty()) {
        if(value.isUndefined()) {
            object.remove(key);
        }
        else {
            object.insert(key, value);
        }
        return object;
    }

    const auto separator = group.indexOf(u'.');
    const auto section   = (separator < 0 ? group : group.first(separator)).toString();
    const auto rest      = separator < 0 ? QStringView{} : group.sliced(separator + 1);

    const auto child = updateAppearanceProperty(object.value(section).toObject(), rest, key, value);
    if(child.isEmpty()) {
        object.remove(section);
    }
    else {
        object.insert(section, child);
    }

    return object;
}

ComponentAppearanceEditor::ComponentAppearanceEditor(AppearancePropertyModel* model, QString key, QString title)
    : m_key{std::move(key)}
    , m_title{std::move(title)}
    , m_previewWidget{new QWidget()}
    , m_previewState{PreviewState::Normal}
    , m_propertyModel{model}
    , m_currentPropertyPage{nullptr}
{ }

ComponentAppearanceEditor::~ComponentAppearanceEditor() = default;

QString ComponentAppearanceEditor::key() const
{
    return m_key;
}

QString ComponentAppearanceEditor::title() const
{
    return m_title;
}

QWidget* ComponentAppearanceEditor::previewWidget() const
{
    return m_previewWidget;
}

QJsonObject ComponentAppearanceEditor::overrides() const
{
    return m_overrides;
}

bool ComponentAppearanceEditor::hasGroupOverrides(const QString& group) const
{
    auto object{m_overrides};

    const auto sections = group.split(u'.', Qt::SkipEmptyParts);
    for(const auto& section : sections) {
        object = object.value(section).toObject();
    }

    const auto keys = m_propertyKeys.find(group);
    return keys != m_propertyKeys.end()
        && std::ranges::any_of(keys->second, [&object](const QString& key) { return object.contains(key); });
}

void ComponentAppearanceEditor::setOverrides(const QJsonObject& overrides)
{
    m_overrides = overrides;
    refreshProperties();
    Q_EMIT changed();
}

AppearancePropertyPage* ComponentAppearanceEditor::currentPropertyPage() const
{
    return m_currentPropertyPage;
}

const std::vector<AppearancePart>& ComponentAppearanceEditor::parts() const
{
    return m_parts;
}

QString ComponentAppearanceEditor::validationError() const
{
    return m_validationError;
}

void ComponentAppearanceEditor::setPreviewPalette(const QPalette& palette, const QFont& font)
{
    updatePreviewPalette(palette, font);

    const auto children = m_previewWidget->findChildren<QWidget*>();
    for(auto* child : children) {
        if(child->isWindow() && child->testAttribute(Qt::WA_SetStyle)) {
            child->setPalette(palette);
            child->setFont(font);
        }
    }
}

PreviewState ComponentAppearanceEditor::previewState() const
{
    return m_previewState;
}

void ComponentAppearanceEditor::setPreviewState(PreviewState state)
{
    m_previewState = state;
    updatePreviewState(state);
}

bool ComponentAppearanceEditor::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::ChildPolished) {
        // NOLINTBEGIN
        if(auto* child = qobject_cast<QWidget*>(static_cast<QChildEvent*>(event)->child())) {
            preparePreview(child);
        }
        // NOLINTEND
    }
    return QObject::eventFilter(watched, event);
}

AppearancePropertyPage* ComponentAppearanceEditor::addPage(const QString& id)
{
    auto* page = m_pages.emplace(id, std::make_unique<AppearancePropertyPage>(m_propertyModel)).first->second.get();
    if(!m_currentPropertyPage) {
        m_currentPropertyPage = page;
    }
    QObject::connect(page, &AppearancePropertyPage::propertiesEdited, this, &ComponentAppearanceEditor::editProperties);
    return page;
}

AppearancePropertyPage* ComponentAppearanceEditor::addPartPage(const QString& id, const QString& title,
                                                               const QString& parent, PreviewState previewState)
{
    if(!id.isEmpty()) {
        addPart({.id = id, .title = title, .parentId = parent, .previewState = previewState});
    }
    return addPage(id);
}

void ComponentAppearanceEditor::addPart(AppearancePart part)
{
    m_parts.push_back(std::move(part));
}

void ComponentAppearanceEditor::setParts(std::vector<AppearancePart> parts)
{
    m_parts = std::move(parts);
}

QString ComponentAppearanceEditor::propertyPageId(const QString& id) const
{
    const auto part = std::ranges::find(m_parts, id, &AppearancePart::id);
    return part != m_parts.end() && !part->states.empty() ? part->states.front().pageId : id;
}

QString ComponentAppearanceEditor::previewPage(const QString& id, PreviewState previewState) const
{
    const auto part = std::ranges::find(m_parts, id, &AppearancePart::id);
    if(part == m_parts.end() || part->states.empty()) {
        return id;
    }

    const auto state = std::ranges::find(part->states, previewState, &AppearanceState::previewState);
    return state == part->states.end() ? part->states.front().pageId : state->pageId;
}

bool ComponentAppearanceEditor::showPage(const QString& id)
{
    const auto page = m_pages.find(id);
    if(page == m_pages.end()) {
        return false;
    }

    m_currentPropertyPage = page->second.get();
    return true;
}

void ComponentAppearanceEditor::preparePreview(QWidget* widget)
{
    widget->installEventFilter(this);

    if(const auto* parent = widget->parentWidget(); parent && parent->testAttribute(Qt::WA_SetStyle)) {
        if(widget->style() != parent->style()) {
            widget->setStyle(parent->style());
        }
        if(widget->isWindow()) {
            widget->setPalette(parent->palette());
            widget->setFont(parent->font());
        }
    }

    const auto children = widget->findChildren<QWidget*>(Qt::FindDirectChildrenOnly);
    for(auto* child : children) {
        preparePreview(child);
    }
}

void ComponentAppearanceEditor::setPreviewStyle(QWidget* widget, QStyle* style)
{
    widget->setStyle(style);

    const auto children = widget->findChildren<QWidget*>();
    for(auto* child : children) {
        child->setStyle(style);
    }
}

void ComponentAppearanceEditor::requestPreviewState(const QString& page)
{
    const auto parts = this->parts();
    for(const auto& part : parts) {
        for(const auto& state : part.states) {
            if(state.pageId == page) {
                Q_EMIT previewStateRequested(state.previewState);
                return;
            }
        }
        if(part.id == page) {
            Q_EMIT previewStateRequested(part.previewState);
            return;
        }
    }

    Q_EMIT previewStateRequested(PreviewState::Normal);
}

QPalette::ColorGroup ComponentAppearanceEditor::stateColourGroup(const QString& page) const
{
    const auto parts = this->parts();
    for(const auto& part : parts) {
        for(const auto& state : part.states) {
            if(state.pageId == page) {
                return state.previewState == PreviewState::Disabled ? QPalette::Disabled : QPalette::Active;
            }
        }
    }
    return QPalette::Active;
}

void ComponentAppearanceEditor::setValidationError(QString error)
{
    m_validationError = std::move(error);
}

AppearanceProperty* ComponentAppearanceEditor::addChoice(AppearancePropertyPage* page, const QString& label,
                                                         const QString& group, const QString& key,
                                                         const QString& defaultValue,
                                                         std::initializer_list<std::pair<QString, QString>> choices)
{
    return addProperty(page, {.label        = label,
                              .group        = group,
                              .key          = key,
                              .defaultValue = defaultValue,
                              .type         = AppearanceProperty::Type::Choice,
                              .choices      = choices});
}

AppearanceProperty* ComponentAppearanceEditor::addGroup(AppearancePropertyPage* page, const QString& label,
                                                        const QString& group, const QString& key)
{
    return addProperty(page, {.label        = label,
                              .group        = group,
                              .key          = key,
                              .defaultValue = QJsonValue::Undefined,
                              .type         = AppearanceProperty::Type::Group});
}

AppearanceProperty* ComponentAppearanceEditor::addBool(AppearancePropertyPage* page, const QString& label,
                                                       const QString& group, const QString& key, bool defaultValue)
{
    return addProperty(page, {.label        = label,
                              .group        = group,
                              .key          = key,
                              .defaultValue = defaultValue,
                              .type         = AppearanceProperty::Type::Boolean});
}

AppearanceProperty* ComponentAppearanceEditor::addNumber(AppearancePropertyPage* page, const QString& label,
                                                         const QString& group, const QString& key, double defaultValue,
                                                         double minimum, double maximum, int decimals)
{
    return addProperty(page, {.label        = label,
                              .group        = group,
                              .key          = key,
                              .defaultValue = defaultValue,
                              .type         = AppearanceProperty::Type::Number,
                              .minimum      = minimum,
                              .maximum      = maximum,
                              .decimals     = decimals,
                              .suffix       = tr(" px")});
}

AppearanceProperty* ComponentAppearanceEditor::addColour(AppearancePropertyPage* page, const QString& label,
                                                         const QString& group, const QString& key,
                                                         const ColourSource& defaultValue,
                                                         QPalette::ColorGroup colourGroup)
{
    return addProperty(page, {.label        = label,
                              .group        = group,
                              .key          = key,
                              .defaultValue = colourSourceToJson(defaultValue),
                              .type         = AppearanceProperty::Type::Colour,
                              .colourGroup  = colourGroup});
}

AppearanceProperty* ComponentAppearanceEditor::addBrush(AppearancePropertyPage* page, const QString& label,
                                                        const QString& group, const QString& key,
                                                        const BrushSource& defaultValue,
                                                        QPalette::ColorGroup colourGroup)
{
    return addProperty(page, {.label        = label,
                              .group        = group,
                              .key          = key,
                              .defaultValue = brushSourceToJson(defaultValue),
                              .type         = AppearanceProperty::Type::Brush,
                              .colourGroup  = colourGroup});
}

AppearanceProperty* ComponentAppearanceEditor::addProperty(AppearancePropertyPage* page, AppearanceProperty property)
{
    m_propertyKeys[property.group].push_back(property.key);

    const auto parts = this->parts();
    for(const auto& part : parts) {
        for(const auto& state : part.states) {
            auto bases{state.inheritanceGroups};
            if(bases.empty() && state.pageId != part.states.front().pageId) {
                bases.push_back(part.states.front().pageId);
            }
            if(state.pageId == property.group) {
                property.inheritanceGroups = bases;
            }
            if(std::ranges::find(bases, property.group) != bases.cend()) {
                property.stateBase = true;
            }
        }
    }

    return page->addProperty(std::move(property));
}

void ComponentAppearanceEditor::editProperties(const std::vector<AppearancePropertyEdit>& edits)
{
    auto overrides{m_overrides};
    for(const auto& [group, key, value] : edits) {
        overrides = updateAppearanceProperty(std::move(overrides), group, key, value);
    }
    setOverrides(overrides);
}

void ComponentAppearanceEditor::refreshProperties()
{
    for(const auto& page : m_pages | std::views::values) {
        page->setOverrides(m_overrides);
    }

    updatePreview();
}
} // namespace Fooyin
