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

#include "autoplaylistrulegroupwidget.h"

#include "autoplaylistrulerowwidget.h"

#include <gui/guiconstants.h>
#include <gui/iconloader.h>

#include <QComboBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <type_traits>

namespace Fooyin {
AutoPlaylistRuleGroupWidget::AutoPlaylistRuleGroupWidget(bool root, QWidget* parent)
    : QFrame{parent}
    , m_root{root}
    , m_match{new QComboBox(this)}
    , m_expand{root ? nullptr : new QToolButton(this)}
    , m_body{new QWidget(this)}
    , m_ruleLayout{new QVBoxLayout(m_body)}
{
    m_match->addItem(tr("Match all rules"));
    m_match->addItem(tr("Match any rule"));
    m_match->addItem(tr("Match an odd number of rules"));

    m_match->setItemData(AutoPlaylistRuleGroup::All, tr("Uses AND: every rule or nested group must match."),
                         Qt::ToolTipRole);
    m_match->setItemData(AutoPlaylistRuleGroup::Any, tr("Uses OR: at least one rule or nested group must match."),
                         Qt::ToolTipRole);
    m_match->setItemData(AutoPlaylistRuleGroup::Odd,
                         tr("Uses XOR: one, three, five, or another odd number of rules must match. "
                            "Each nested group counts as one rule."),
                         Qt::ToolTipRole);

    if(root) {
        m_match->addItem(tr("Include all tracks"));
        m_match->setItemData(m_match->count() - 1, tr("Includes every track in the library without applying rules."),
                             Qt::ToolTipRole);
    }

    m_match->setToolTip(m_match->currentData(Qt::ToolTipRole).toString());

    auto* addRule = new QPushButton(tr("Add rule"), this);
    addRule->setIcon(Gui::iconFromTheme(Constants::Icons::Add));
    auto* addGroup = new QPushButton(tr("Add group"), this);
    addGroup->setIcon(Gui::iconFromTheme(Constants::Icons::Add));

    auto* header = new QHBoxLayout();
    if(m_expand) {
        m_expand->setCheckable(true);
        m_expand->setChecked(true);
        m_expand->setArrowType(Qt::DownArrow);
        m_expand->setToolTip(tr("Collapse group"));
        QObject::connect(m_expand, &QToolButton::toggled, this, [this](bool expanded) {
            m_body->setVisible(expanded);
            m_expand->setArrowType(expanded ? Qt::DownArrow : (isRightToLeft() ? Qt::LeftArrow : Qt::RightArrow));
            m_expand->setToolTip(expanded ? tr("Collapse group") : tr("Expand group"));
        });
        header->addWidget(m_expand);
    }
    header->addWidget(m_match, 1);
    header->addWidget(addRule);
    header->addWidget(addGroup);

    if(!root) {
        setFrameShape(QFrame::StyledPanel);

        auto* remove = new QToolButton(this);
        remove->setIcon(Gui::iconFromTheme(Constants::Icons::Remove));
        remove->setToolTip(tr("Remove group"));
        QObject::connect(remove, &QToolButton::clicked, this, &AutoPlaylistRuleGroupWidget::removeRequested);
        header->addWidget(remove);
    }

    m_ruleLayout->setContentsMargins({});
    m_ruleLayout->setAlignment(Qt::AlignTop);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(header);

    if(root) {
        layout->setContentsMargins({});
        auto* scroll = new QScrollArea(this);
        scroll->setWidget(m_body);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setMinimumHeight(140);
        layout->addWidget(scroll, 1);
    }
    else {
        const auto margins = layout->contentsMargins();
        layout->setContentsMargins(0, margins.top(), 0, margins.bottom());
        layout->addWidget(m_body);
    }

    QObject::connect(m_match, &QComboBox::currentIndexChanged, this, [this, addRule, addGroup]() {
        m_match->setToolTip(m_match->currentData(Qt::ToolTipRole).toString());

        const bool editable = !allTracks();
        m_body->setEnabled(editable);
        addRule->setEnabled(editable);
        addGroup->setEnabled(editable);
        Q_EMIT changed();
    });
    QObject::connect(addRule, &QPushButton::clicked, this, [this]() {
        addItem(AutoPlaylistRule{});
        Q_EMIT changed();
    });
    QObject::connect(addGroup, &QPushButton::clicked, this, [this]() {
        AutoPlaylistRuleGroup nested;
        nested.match = m_match->currentIndex() == AutoPlaylistRuleGroup::All ? AutoPlaylistRuleGroup::Any
                                                                             : AutoPlaylistRuleGroup::All;
        nested.rules.emplace_back(AutoPlaylistRule{});
        addItem(nested);
        Q_EMIT changed();
    });

    addItem(AutoPlaylistRule{});
}

AutoPlaylistRuleGroup AutoPlaylistRuleGroupWidget::group() const
{
    AutoPlaylistRuleGroup group;
    group.match
        = allTracks() ? AutoPlaylistRuleGroup::All : static_cast<AutoPlaylistRuleGroup::Match>(m_match->currentIndex());
    for(const auto& item : m_items) {
        group.rules.push_back(std::visit(
            [](const auto* widget) -> AutoPlaylistRuleGroup::Item {
                if constexpr(std::is_same_v<std::decay_t<decltype(*widget)>, AutoPlaylistRuleRowWidget>) {
                    return widget->rule();
                }
                else {
                    return widget->group();
                }
            },
            item));
    }
    return group;
}

bool AutoPlaylistRuleGroupWidget::allTracks() const
{
    return m_root && m_match->currentIndex() == m_match->count() - 1;
}

QString AutoPlaylistRuleGroupWidget::error(const QString& path) const
{
    if(allTracks()) {
        return {};
    }

    if(m_items.empty()) {
        if(m_root) {
            return tr("Add a rule or group, or choose ‘Include all tracks’.");
        }
        //: %1 is a group number, including its parent groups, e.g. 2.1.
        return tr("Add a rule or group to group %1.").arg(path);
    }

    for(size_t i{0}; i < m_items.size(); ++i) {
        const QString number = path.isEmpty() ? QString::number(i + 1) : path + u'.' + QString::number(i + 1);
        if(const auto* nested = std::get_if<AutoPlaylistRuleGroupWidget*>(&m_items[i])) {
            if(const auto message = (*nested)->error(number); !message.isEmpty()) {
                return message;
            }
            continue;
        }

        const auto rule = std::get<AutoPlaylistRuleRowWidget*>(m_items[i])->rule();
        if(rule.field.isEmpty()) {
            //: %1 is a rule number, including its parent groups, e.g. 2.1.
            return tr("Enter a tag name for rule %1.").arg(number);
        }

        const auto type = AutoPlaylistQueryBuilder::fieldType(rule.field, rule.source);
        if(type == AutoPlaylistFieldType::Date && rule.comparison == Expr::Between && rule.date > rule.endDate) {
            //: %1 is a rule number, including its parent groups, e.g. 2.1.
            return tr("The start date must not be after the end date for rule %1.").arg(number);
        }
        if(rule.comparison == Expr::In || rule.comparison == Expr::ContainsAny) {
            if(rule.values.empty() || !std::ranges::all_of(rule.values, [type](const QString& value) {
                   return AutoPlaylistQueryBuilder::isListValueValid(value, type);
               })) {
                //: %1 is a rule number, including its parent groups, e.g. 2.1.
                return tr("Enter at least one valid value for rule %1.").arg(number);
            }
            continue;
        }
        if((type == AutoPlaylistFieldType::Text || type == AutoPlaylistFieldType::Metadata) && rule.value.isEmpty()
           && rule.comparison != Expr::Present && rule.comparison != Expr::Missing) {
            //: %1 is a rule number, including its parent groups, e.g. 2.1.
            return tr("Enter a value for rule %1.").arg(number);
        }
        if(type == AutoPlaylistFieldType::Metadata
           && (rule.comparison == Expr::Greater || rule.comparison == Expr::GreaterEqual
               || rule.comparison == Expr::Less || rule.comparison == Expr::LessEqual)) {
            bool valid{false};
            const double value = rule.value.toDouble(&valid);
            if(!valid || !std::isfinite(value)) {
                //: %1 is a rule number, including its parent groups, e.g. 2.1.
                return tr("Enter a number for rule %1.").arg(number);
            }
        }
    }

    return {};
}

void AutoPlaylistRuleGroupWidget::setGroup(const AutoPlaylistRuleGroup& group, bool allTracks)
{
    const QSignalBlocker blocker{this};

    while(const auto* item = m_ruleLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    m_items.clear();

    for(const auto& item : group.rules) {
        addItem(item);
    }
    m_match->setCurrentIndex(m_root && allTracks ? m_match->count() - 1 : group.match);
}

void AutoPlaylistRuleGroupWidget::addItem(const AutoPlaylistRuleGroup::Item& item)
{
    if(const auto* rule = std::get_if<AutoPlaylistRule>(&item)) {
        auto* row = new AutoPlaylistRuleRowWidget(*rule, m_body);
        QObject::connect(row, &AutoPlaylistRuleRowWidget::changed, this, &AutoPlaylistRuleGroupWidget::changed);
        QObject::connect(row, &AutoPlaylistRuleRowWidget::removeRequested, this, [this, row]() { removeItem(row); });
        m_items.emplace_back(row);
        m_ruleLayout->addWidget(row);
    }
    else {
        auto* container = new QWidget(m_body);
        auto* layout    = new QHBoxLayout(container);
        layout->setContentsMargins({});
        layout->addSpacing(style()->pixelMetric(QStyle::PM_TreeViewIndentation, nullptr, this));

        auto* nested = new AutoPlaylistRuleGroupWidget(false, container);
        nested->setGroup(std::get<AutoPlaylistRuleGroup>(item));
        QObject::connect(nested, &AutoPlaylistRuleGroupWidget::changed, this, &AutoPlaylistRuleGroupWidget::changed);
        QObject::connect(nested, &AutoPlaylistRuleGroupWidget::removeRequested, this,
                         [this, nested]() { removeItem(nested); });
        m_items.emplace_back(nested);
        layout->addWidget(nested);

        m_ruleLayout->addWidget(container);
    }

    if(m_expand) {
        m_expand->setChecked(true);
    }
}

void AutoPlaylistRuleGroupWidget::removeItem(QWidget* item)
{
    std::erase_if(m_items, [item](const auto& entry) {
        return std::visit([item](const auto* widget) { return widget == item; }, entry);
    });

    auto* container = item->parentWidget() == m_body ? item : item->parentWidget();
    m_ruleLayout->removeWidget(container);
    container->hide();
    container->deleteLater();

    Q_EMIT changed();
}
} // namespace Fooyin
