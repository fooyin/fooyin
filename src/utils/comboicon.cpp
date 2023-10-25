/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <utils/comboicon.h>

#include <utils/clickablelabel.h>
#include <utils/utils.h>

#include <QApplication>
#include <QEvent>
#include <QVBoxLayout>

constexpr int IconSize = 128;

namespace Fy::Utils {
struct Icon
{
    QPixmap icon;
    QPixmap iconActive;
    QPixmap iconDisabled;
};

using PathIconPair      = std::pair<QString, Icon>;
using PathIconContainer = std::vector<PathIconPair>;

struct ComboIcon::Private
{
    ComboIcon* self;

    ClickableLabel* label;
    Attributes attributes;
    int currentIndex{0};
    PathIconContainer icons;

    Private(ComboIcon* self, Attributes attributes)
        : self{self}
        , label{new ClickableLabel(self)}
        , attributes{attributes}
    { }

    void labelClicked()
    {
        if(hasAttribute(AutoShift)) {
            if(hasAttribute(Active)) {
                ++currentIndex;
            }
            if(currentIndex >= static_cast<int>(icons.size())) {
                currentIndex = 0;
                removeAttribute(Active);
                label->setPixmap(icons.at(currentIndex).second.icon);
            }
            else {
                addAttribute(Active);
                label->setPixmap(icons.at(currentIndex).second.iconActive);
            }
        }
        QMetaObject::invokeMethod(self, "clicked", Q_ARG(const QString&, icons.at(currentIndex).first));
    }

    [[nodiscard]] bool hasAttribute(Attribute attribute) const
    {
        return (attributes & attribute);
    }

    void addAttribute(Attribute attribute)
    {
        attributes |= attribute;
    }

    void removeAttribute(Attribute attribute)
    {
        attributes &= ~attribute;
    }
};

ComboIcon::ComboIcon(const QString& path, Attributes attributes, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<Private>(this, attributes)}
{
    p->addAttribute(Enabled);
    addIcon(path);

    if(!p->icons.empty()) {
        p->label->setPixmap(p->icons.front().second.icon);
    }

    p->label->setScaledContents(true);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(p->label);
    layout->setSpacing(10);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);

    QObject::connect(p->label, &ClickableLabel::clicked, this, [this]() { p->labelClicked(); });
    QObject::connect(p->label, &ClickableLabel::entered, this, &ComboIcon::entered);
    QObject::connect(p->label, &ClickableLabel::mouseLeft, this, &ComboIcon::mouseLeft);
}

ComboIcon::ComboIcon(const QString& path, QWidget* parent)
    : ComboIcon{path, {}, parent}
{ }

ComboIcon::~ComboIcon() = default;

void ComboIcon::addIcon(const QString& path, const QPixmap& icon)
{
    const QPalette palette = QApplication::palette();
    Icon ico;
    ico.icon = icon;

    if(p->hasAttribute(HasActiveIcon)) {
        ico.iconActive = Utils::changePixmapColour(icon, palette.highlight().color());
    }
    if(p->hasAttribute(HasDisabledIcon)) {
        ico.iconDisabled = Utils::changePixmapColour(icon, palette.color(QPalette::Disabled, QPalette::Base));
    }

    p->icons.emplace_back(path, ico);
}

void ComboIcon::addIcon(const QString& path)
{
    const auto icon   = QIcon::fromTheme(path);
    const auto pixmap = icon.pixmap(IconSize);
    addIcon(path, pixmap);
}

void ComboIcon::setIcon(const QString& path, bool active)
{
    if(path.isEmpty()) {
        return;
    }

    if(active) {
        p->addAttribute(Active);
    }
    else {
        p->removeAttribute(Active);
    }

    auto it = std::ranges::find_if(std::as_const(p->icons),
                                   [path](const PathIconPair& icon) { return icon.first == path; });

    if(it == p->icons.cend()) {
        return;
    }

    p->currentIndex = static_cast<int>(std::distance(p->icons.cbegin(), it));

    if(p->hasAttribute(HasActiveIcon) && p->hasAttribute(Active)) {
        p->label->setPixmap(p->icons.at(p->currentIndex).second.iconActive);
    }
    else {
        p->label->setPixmap(p->icons.at(p->currentIndex).second.icon);
    }
}

void ComboIcon::setIconEnabled(bool enable)
{
    if(enable) {
        p->addAttribute(Enabled);
    }
    else {
        p->removeAttribute(Enabled);
    }

    if(p->hasAttribute(Enabled)) {
        if(p->hasAttribute(HasActiveIcon) && p->hasAttribute(Active)) {
            p->label->setPixmap(p->icons.at(p->currentIndex).second.iconActive);
            return;
        }
        p->label->setPixmap(p->icons.at(p->currentIndex).second.icon);
        return;
    }

    p->label->setPixmap(p->icons.at(p->currentIndex).second.iconDisabled);
}

void ComboIcon::updateIcons()
{
    const PathIconContainer prevIcons = p->icons;
    p->icons.clear();

    for(const auto& [path, _] : prevIcons) {
        addIcon(path);
    }

    setIconEnabled(p->hasAttribute(Enabled));
}

void ComboIcon::changeEvent(QEvent* event)
{
    if(event->type() == QEvent::EnabledChange && p->hasAttribute(HasDisabledIcon)) {
        setIconEnabled(isEnabled());
    }
    QWidget::changeEvent(event);
}
} // namespace Fy::Utils
