/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include <utils/actions/proxyaction.h>

#include <utils/utils.h>

#include <QPointer>

namespace Fooyin {
struct ProxyAction::Private
{
    ProxyAction* self;

    QPointer<QAction> action{nullptr};
    bool showShortcut{false};
    QString toolTip;

    Attributes attributes;
    bool updating{false};

    explicit Private(ProxyAction* self)
        : self{self}
    { }

    void updateToolTipWithShortcut()
    {
        if(updating) {
            return;
        }
        updating = true;

        if(!showShortcut || self->shortcut().isEmpty()) {
            self->setToolTip(toolTip);
        }
        else {
            self->setToolTip(Utils::appendShortcut(toolTip, self->shortcut()));
        }

        updating = false;
    }

    void update(QAction* updateAction, bool initialise)
    {
        if(!updateAction) {
            return;
        }

        QObject::disconnect(self, &ProxyAction::changed, nullptr, nullptr);

        if(initialise) {
            self->setSeparator(updateAction->isSeparator());
            self->setMenuRole(updateAction->menuRole());
        }
        if(initialise || self->hasAttribute(UpdateIcon)) {
            self->setIcon(updateAction->icon());
            self->setIconText(updateAction->iconText());
            self->setIconVisibleInMenu(updateAction->isIconVisibleInMenu());
        }
        if(initialise || self->hasAttribute(UpdateText)) {
            self->setText(updateAction->text());
            toolTip = updateAction->toolTip();
            updateToolTipWithShortcut();
            self->setStatusTip(updateAction->statusTip());
            self->setWhatsThis(updateAction->whatsThis());
        }

        self->setCheckable(updateAction->isCheckable());

        if(!initialise) {
            if(self->isChecked() != updateAction->isChecked()) {
                if(action) {
                    QObject::disconnect(self, &ProxyAction::toggled, action, &QAction::setChecked);
                }
                self->setChecked(updateAction->isChecked());
                if(action) {
                    QObject::connect(self, &ProxyAction::toggled, action, &QAction::setChecked);
                }
            }
            self->setEnabled(updateAction->isEnabled());
            self->setVisible(updateAction->isVisible());
        }
        QObject::connect(self, &ProxyAction::changed, self, [this]() { updateToolTipWithShortcut(); });
    }

    void updateState()
    {
        if(action) {
            update(action, false);
        }
        else {
            if(self->hasAttribute(Hide)) {
                self->setVisible(false);
            }
            self->setEnabled(false);
        }
    }

    void actionChanged()
    {
        update(action, false);
    }

    void connectAction()
    {
        if(action) {
            QObject::connect(action, &QAction::changed, self, [this]() { actionChanged(); });
            QObject::connect(self, &ProxyAction::triggered, action, &QAction::triggered);
            QObject::connect(self, &ProxyAction::toggled, action, &QAction::setChecked);
        }
    }

    void disconnectAction() const
    {
        if(action) {
            QObject::disconnect(action, &QAction::changed, nullptr, nullptr);
            QObject::disconnect(self, &ProxyAction::triggered, action, &QAction::triggered);
            QObject::disconnect(self, &ProxyAction::toggled, action, &QAction::setChecked);
        }
    }
};

ProxyAction::ProxyAction(QObject* parent)
    : QAction{parent}
    , p{std::make_unique<Private>(this)}
{ }

ProxyAction::~ProxyAction() = default;

void ProxyAction::initialise(QAction* action)
{
    p->update(action, true);
}

QAction* ProxyAction::action() const
{
    return p->action;
}

bool ProxyAction::shortcutVisibleInToolTip() const
{
    return p->showShortcut;
}

void ProxyAction::setAction(QAction* action)
{
    if(p->action == action) {
        return;
    }

    p->disconnectAction();
    p->action = action;
    p->connectAction();
    p->updateState();

    emit currentActionChanged(action);
}

void ProxyAction::setShortcutVisibleInToolTip(bool visible)
{
    p->showShortcut = visible;
    p->updateToolTipWithShortcut();
}

void ProxyAction::setAttribute(Attribute attribute)
{
    p->attributes |= attribute;
    p->updateState();
}

void ProxyAction::removeAttribute(Attribute attribute)
{
    p->attributes &= ~attribute;
    p->updateState();
}

bool ProxyAction::hasAttribute(Attribute attribute)
{
    return p->attributes & attribute;
}

ProxyAction* ProxyAction::actionWithIcon(QAction* original, const QIcon& icon)
{
    auto* proxyAction = new ProxyAction(original);
    proxyAction->setAction(original);
    proxyAction->setIcon(icon);
    proxyAction->setAttribute(UpdateText);
    return proxyAction;
}
} // namespace Fooyin

#include "utils/actions/moc_proxyaction.cpp"
