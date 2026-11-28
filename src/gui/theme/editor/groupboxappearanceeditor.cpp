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

#include "groupboxappearanceeditor.h"

#include "style/fooyinstyle.h"
#include "theme/groupboxappearance.h"

#include <gui/widgets/hintlabel.h>

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QStyleOptionGroupBox>
#include <QStylePainter>
#include <QVBoxLayout>

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
class GroupBoxPreview : public QGroupBox
{
public:
    GroupBoxPreview(const QString& title, ComponentAppearanceEditor* editor, QWidget* parent)
        : QGroupBox{title, parent}
        , m_editor{editor}
    { }

    [[nodiscard]] QString partAt(const QPoint& position) const
    {
        QStyleOptionGroupBox option;
        initStyleOption(&option);

        const auto label = style()->subControlRect(QStyle::CC_GroupBox, &option, QStyle::SC_GroupBoxLabel, this);
        const auto check = style()->subControlRect(QStyle::CC_GroupBox, &option, QStyle::SC_GroupBoxCheckBox, this);
        return label.united(check).contains(position) ? u"title"_s : u"frame"_s;
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QStyleOptionGroupBox option;
        initStyleOption(&option);

        const auto state = m_editor ? m_editor->previewState() : PreviewState::Normal;

        if(state != PreviewState::Normal) {
            option.state &= ~(QStyle::State_MouseOver | QStyle::State_Sunken | QStyle::State_HasFocus);
        }
        if(isCheckable()) {
            if(state == PreviewState::Hovered || state == PreviewState::Pressed) {
                option.state |= QStyle::State_MouseOver;
            }
            if(state == PreviewState::Pressed) {
                option.state |= QStyle::State_Sunken;
            }
            if(state == PreviewState::Focused) {
                option.state |= QStyle::State_HasFocus;
            }
        }

        QStylePainter painter{this};
        painter.drawComplexControl(QStyle::CC_GroupBox, option);
    }

private:
    QPointer<ComponentAppearanceEditor> m_editor;
};
} // namespace

GroupBoxAppearanceEditor::GroupBoxAppearanceEditor(AppearancePropertyModel* propertyModel)
    : ComponentAppearanceEditor{propertyModel, u"groupBox"_s, tr("Group box")}
    , m_previewStyle{std::make_unique<FooyinStyle>()}
    , m_preview{new QWidget(previewWidget())}
{
    setParts(
        {{.id = u"frame"_s, .title = tr("Frame"), .parentId = {}},
         {.id = u"title"_s, .title = tr("Title"), .parentId = {}},
         {.id       = u"colours"_s,
          .title    = tr("Colours"),
          .parentId = {},
          .states
          = {{.id = u"normal"_s, .title = tr("Normal"), .pageId = u"normal"_s, .previewState = PreviewState::Normal},
             {.id           = u"disabled"_s,
              .title        = tr("Disabled"),
              .pageId       = u"disabled"_s,
              .previewState = PreviewState::Disabled}}}});

    m_preview->setAutoFillBackground(true);

    auto* previewLayout = new QVBoxLayout(m_preview);
    previewLayout->setContentsMargins({});

    const std::array titles{tr("General"), tr("Enable options"), QString{}};

    for(size_t index{0}; index < titles.size(); ++index) {
        auto* box = new GroupBoxPreview(titles[index], this, m_preview);
        box->setCheckable(index == 1);
        box->setStyle(m_previewStyle.get());
        box->installEventFilter(this);

        auto* layout = new QVBoxLayout(box);
        if(index == 0) {
            layout->addWidget(new QLabel(tr("Content inside a group"), box));
            auto* edit = new QLineEdit(tr("Sample text"), box);
            edit->setStyle(m_previewStyle.get());
            layout->addWidget(edit);
        }
        else if(index == 1) {
            auto* check = new QCheckBox(tr("An option"), box);
            check->setStyle(m_previewStyle.get());
            layout->addWidget(check);
        }
        else {
            layout->addWidget(new QLabel(tr("A group without a title"), box));
        }
        m_boxes.push_back(box);
        previewLayout->addWidget(box);
    }

    auto* canvasLayout = new QVBoxLayout(previewWidget());
    canvasLayout->setContentsMargins({});
    canvasLayout->addStretch();
    canvasLayout->addWidget(m_preview);
    canvasLayout->addStretch();

    const GroupBoxAppearance defaults;

    auto* dimensions = addPage({});
    addNumber(dimensions, tr("Horizontal padding"), {}, u"horizontalPadding"_s, defaults.horizontalPadding, 0, 128, 0);
    auto* vertical
        = addNumber(dimensions, tr("Vertical padding"), {}, u"verticalPadding"_s, defaults.verticalPadding, 0, 128, 0);
    vertical->toolTip = tr("Space below content, and above content when the title is above the frame or absent");
    dimensions->addHint(new HintLabel(tr("Padding controls the space reserved around content. "
                                         "Widget layouts may add their own margins."),
                                      dimensions));

    auto* frame = addPage(u"frame"_s);
    addNumber(frame, tr("Corner radius"), {}, u"radius"_s, defaults.radius, 0, 128, 2);
    auto* border    = addNumber(frame, tr("Border width"), {}, u"borderWidth"_s, defaults.borderWidth, 0, 32, 2);
    border->toolTip = tr("Zero hides the frame border");
    auto* relief   = addNumber(frame, tr("Relief strength"), {}, u"reliefOpacity"_s, defaults.reliefOpacity, 0, 100, 0);
    relief->suffix = tr(" %");

    auto* title = addPage(u"title"_s);
    addChoice(title, tr("Placement"), u"title"_s, u"placement"_s, defaults.title.placement,
              {{tr("Inside frame"), u"inside"_s}, {tr("Above frame"), u"above"_s}});
    addChoice(title, tr("Alignment"), u"title"_s, u"alignment"_s, defaults.title.alignment,
              {{tr("Centred"), u"center"_s}, {tr("Leading"), u"leading"_s}, {tr("Trailing"), u"trailing"_s}});
    auto* offset    = addNumber(title, tr("Horizontal offset"), u"title"_s, u"horizontalMargin"_s,
                                defaults.title.horizontalMargin, -128, 128, 0);
    offset->toolTip = tr("Positive values inset the title from the frame.\n"
                         "Negative values inset the frame and content to keep the title visible.\n"
                         "Centred titles inset both sides of the frame.");
    addNumber(title, tr("Top inset"), u"title"_s, u"inset"_s, defaults.title.inset, 0, 128, 0);
    auto* gap    = addNumber(title, tr("Gap below title"), u"title"_s, u"gap"_s, defaults.title.gap, 0, 128, 0);
    gap->toolTip = tr("When the title is above the frame, this sets the space between the title and the frame.\n"
                      "Otherwise, it sets the space between the title area and the content.");

    auto* fontGroup    = addGroup(title, tr("Font"), u"title"_s, u"font"_s);
    fontGroup->toolTip = tr("Each setting inherits the group box’s font until overridden.\n"
                            "These settings apply only to the title.");
    auto* font{title};
    auto* size             = addNumber(font, tr("Point size"), u"title.font"_s, u"pointSize"_s, 0, 0, 128, 1);
    size->suffix           = tr(" pt");
    size->specialValueText = tr("Inherited");
    size->toolTip          = tr("Zero inherits the group box’s font size");
    auto* bold             = addBool(font, tr("Bold"), u"title.font"_s, u"weight"_s, false);
    bold->type             = AppearanceProperty::Type::FontBold;
    bold->defaultValue     = 0;
    bold->toolTip          = tr("Sets weight to 700 when enabled or 400 when disabled. "
                                "Partially checked inherits the group box’s font weight.");
    auto* weight           = addNumber(font, tr("Weight"), u"title.font"_s, u"weight"_s, 0, 0, 1000, 0);
    weight->suffix.clear();
    weight->specialValueText = tr("Inherited");
    weight->toolTip          = tr("Font weight from 1 to 1000. Zero inherits the group box’s font weight.");
    auto* italic             = addBool(font, tr("Italic"), u"title.font"_s, u"italic"_s, false);
    italic->type             = AppearanceProperty::Type::OptionalBoolean;
    italic->defaultValue     = QJsonValue::Null;
    italic->toolTip          = tr("Partially checked inherits the group box’s italic setting");

    static const std::array states{std::pair{u"normal"_s, defaults.normal},
                                   std::pair{u"disabled"_s, defaults.disabled}};

    for(const auto& [name, colours] : states) {
        auto* page       = addPage(name);
        const auto group = stateColourGroup(name);

        addBool(page, tr("Draw background"), name, u"backgroundVisible"_s, colours.backgroundVisible);
        addColour(page, tr("Background colour"), name, u"backgroundColour"_s, colours.backgroundColour, group);
        addColour(page, tr("Background tint"), name, u"backgroundTintColour"_s, colours.backgroundTintColour, group);

        auto* backgroundTint   = addNumber(page, tr("Background tint amount"), name, u"backgroundTintOpacity"_s,
                                           colours.backgroundTintOpacity, 0, 100, 2);
        backgroundTint->suffix = tr(" %");

        addColour(page, tr("Border colour"), name, u"borderColour"_s, colours.borderColour, group);
        addColour(page, tr("Border tint"), name, u"borderTintColour"_s, colours.borderTintColour, group);

        auto* borderTint   = addNumber(page, tr("Border tint amount"), name, u"borderTintOpacity"_s,
                                       colours.borderTintOpacity, 0, 100, 2);
        borderTint->suffix = tr(" %");

        addColour(page, tr("Title colour"), name, u"titleColour"_s, colours.titleColour, group);
    }

    preparePreview(m_preview);
}

GroupBoxAppearanceEditor::~GroupBoxAppearanceEditor()
{
    for(auto* box : m_boxes) {
        const auto children = box->findChildren<QWidget*>();
        for(auto* child : children) {
            child->setStyle(nullptr);
        }
        box->setStyle(nullptr);
    }
}

void GroupBoxAppearanceEditor::selectPart(const QString& part, bool updateState)
{
    const auto page = propertyPageId(part);
    if(!showPage(page)) {
        return;
    }
    if(updateState) {
        requestPreviewState(page);
    }
}

bool GroupBoxAppearanceEditor::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::MouseButtonPress) {
        const auto* target = qobject_cast<QWidget*>(watched);
        const auto box     = std::ranges::find_if(
            m_boxes, [target](auto* item) { return target && (target == item || item->isAncestorOf(target)); });
        if(box != m_boxes.cend()) {
            const auto* preview = static_cast<GroupBoxPreview*>(*box);
            Q_EMIT partSelected(
                previewState() == PreviewState::Disabled
                    ? u"disabled"_s
                    : preview->partAt(target->mapTo(*box, static_cast<QMouseEvent*>(event)->position().toPoint())));
        }
    }
    return ComponentAppearanceEditor::eventFilter(watched, event);
}

void GroupBoxAppearanceEditor::updatePreviewPalette(const QPalette& palette, const QFont& font)
{
    m_preview->setPalette(palette);
    m_preview->setFont(font);
}

void GroupBoxAppearanceEditor::updatePreview()
{
    const auto resolved = resolveGroupBoxAppearance(QJsonObject{{key(), overrides()}});
    setValidationError(resolved ? QString{} : tr("Group box appearance: %1").arg(resolved.error()));
    if(!resolved || m_previewStyle->appearance().groupBox == *resolved) {
        return;
    }

    auto style = std::make_unique<FooyinStyle>(ThemeAppearance{.groupBox = *resolved});
    for(auto* box : m_boxes) {
        box->setStyle(style.get());
        const auto children = box->findChildren<QWidget*>();
        for(auto* child : children) {
            child->setStyle(style.get());
        }
    }
    m_previewStyle = std::move(style);
}

void GroupBoxAppearanceEditor::updatePreviewState(PreviewState state)
{
    for(auto* box : m_boxes) {
        box->setEnabled(state != PreviewState::Disabled);
        box->update();
    }
}
} // namespace Fooyin
