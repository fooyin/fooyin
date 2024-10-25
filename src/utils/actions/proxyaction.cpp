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

#include <utils/actions/proxyaction.h>

#include <utils/stringutils.h>

#include <QPointer>

namespace Fooyin {
class ProxyActionPrivate
{
public:
    explicit ProxyActionPrivate(ProxyAction* self)
        : m_self{self}
    { }

    void updateToolTipWithShortcut();
    void update(QAction* updateAction, bool initialise);
    void updateState();

    void actionChanged();
    void connectAction();
    void disconnectAction() const;

    ProxyAction* m_self;

    QPointer<QAction> m_action{nullptr};
    bool m_showShortcut{false};
    QString m_toolTip;

    ProxyAction::Attributes m_attributes;
    bool m_updating{false};
};

void ProxyActionPrivate::updateToolTipWithShortcut()
{
    if(m_updating) {
        return;
    }
    m_updating = true;

    if(!m_showShortcut || m_self->shortcut().isEmpty()) {
        m_self->setToolTip(m_toolTip);
    }
    else {
        m_self->setToolTip(Utils::appendShortcut(m_toolTip, m_self->shortcut()));
    }

    m_updating = false;
}

void ProxyActionPrivate::update(QAction* updateAction, bool initialise)
{
    if(!updateAction) {
        return;
    }

    QObject::disconnect(m_self, &ProxyAction::changed, nullptr, nullptr);

    if(initialise) {
        m_self->setSeparator(updateAction->isSeparator());
        m_self->setMenuRole(updateAction->menuRole());
    }
    if(initialise || m_self->hasAttribute(ProxyAction::UpdateIcon)) {
        m_self->setIcon(updateAction->icon());
        m_self->setIconText(updateAction->iconText());
        m_self->setIconVisibleInMenu(updateAction->isIconVisibleInMenu());
    }
    if(initialise || m_self->hasAttribute(ProxyAction::UpdateText)) {
        m_self->setText(updateAction->text());
        m_self->setIconText(updateAction->text());
        m_toolTip = updateAction->toolTip();
        updateToolTipWithShortcut();
        m_self->setStatusTip(updateAction->statusTip());
        m_self->setWhatsThis(updateAction->whatsThis());
    }

    m_self->setCheckable(updateAction->isCheckable());

    if(!initialise) {
        if(m_self->isChecked() != updateAction->isChecked()) {
            if(m_action) {
                QObject::disconnect(m_self, &ProxyAction::toggled, m_action, &QAction::setChecked);
            }
            m_self->setChecked(updateAction->isChecked());
            if(m_action) {
                QObject::connect(m_self, &ProxyAction::toggled, m_action, &QAction::setChecked);
            }
        }
        m_self->setEnabled(updateAction->isEnabled());
        m_self->setVisible(updateAction->isVisible());
    }
    QObject::connect(m_self, &ProxyAction::changed, m_self, [this]() { updateToolTipWithShortcut(); });
}

void ProxyActionPrivate::updateState()
{
    if(m_action) {
        update(m_action, false);
    }
    else {
        if(m_self->hasAttribute(ProxyAction::Hide)) {
            m_self->setVisible(false);
        }
        m_self->setEnabled(false);
    }
}

void ProxyActionPrivate::actionChanged()
{
    update(m_action, false);
}

void ProxyActionPrivate::connectAction()
{
    if(m_action) {
        QObject::connect(m_action, &QAction::changed, m_self, [this]() { actionChanged(); });
        QObject::connect(m_self, &ProxyAction::triggered, m_action, &QAction::triggered);
        QObject::connect(m_self, &ProxyAction::toggled, m_action, &QAction::setChecked);
    }
}

void ProxyActionPrivate::disconnectAction() const
{
    if(m_action) {
        QObject::disconnect(m_action, &QAction::changed, nullptr, nullptr);
        QObject::disconnect(m_self, &ProxyAction::triggered, m_action, &QAction::triggered);
        QObject::disconnect(m_self, &ProxyAction::toggled, m_action, &QAction::setChecked);
    }
}

ProxyAction::ProxyAction(QObject* parent)
    : QAction{parent}
    , p{std::make_unique<ProxyActionPrivate>(this)}
{ }

ProxyAction::~ProxyAction() = default;

void ProxyAction::initialise(QAction* action)
{
    p->update(action, true);
}

QAction* ProxyAction::action() const
{
    return p->m_action;
}

bool ProxyAction::shortcutVisibleInToolTip() const
{
    return p->m_showShortcut;
}

void ProxyAction::setAction(QAction* action)
{
    if(p->m_action == action) {
        return;
    }

    p->disconnectAction();
    p->m_action = action;
    p->connectAction();
    p->updateState();

    emit currentActionChanged(action);
}

void ProxyAction::setShortcutVisibleInToolTip(bool visible)
{
    p->m_showShortcut = visible;
    p->updateToolTipWithShortcut();
}

void ProxyAction::setAttribute(Attribute attribute)
{
    p->m_attributes |= attribute;
    p->updateState();
}

void ProxyAction::removeAttribute(Attribute attribute)
{
    p->m_attributes &= ~attribute;
    p->updateState();
}

bool ProxyAction::hasAttribute(Attribute attribute)
{
    return p->m_attributes & attribute;
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
