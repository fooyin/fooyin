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

#include "autoplaylistruleswidget.h"

#include "autoplaylistfieldcombobox.h"
#include "autoplaylistrulegroupwidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <limits>

using namespace Qt::StringLiterals;

namespace Fooyin {
AutoPlaylistRulesWidget::AutoPlaylistRulesWidget(QWidget* parent)
    : QWidget{parent}
    , m_group{new AutoPlaylistRuleGroupWidget(true, this)}
    , m_sortField{new AutoPlaylistFieldComboBox(this)}
    , m_sortDirection{new QComboBox(this)}
    , m_randomOrder{new QCheckBox(tr("Randomise order"), this)}
    , m_limitWidget{new QWidget(this)}
    , m_limitEnabled{new QCheckBox(tr("Limit number of tracks") + u":"_s, this)}
    , m_limit{new QSpinBox(this)}
{
    m_sortField->insertItem(0, tr("None"), QString{});
    m_sortField->setCurrentIndex(0);
    m_sortField->lineEdit()->setPlaceholderText(tr("None"));
    m_sortField->lineEdit()->setClearButtonEnabled(true);
    m_sortField->setToolTip(tr("Choose a field or enter a tag name. Use ‘Metadata tag…’ to read a tag directly.\n"
                               "Clear to disable sorting."));

    m_sortDirection->addItem(tr("Ascending"));
    m_sortDirection->addItem(tr("Descending"));
    m_sortDirection->setEnabled(false);

    m_randomOrder->setToolTip(tr("Randomise matching tracks before applying the track limit."));

    m_limit->setRange(1, std::numeric_limits<int>::max());
    m_limit->setValue(25);
    m_limit->setEnabled(false);

    auto* sorting = new QHBoxLayout();
    sorting->addWidget(m_sortField, 1);
    sorting->addWidget(m_sortDirection);

    auto* limit = new QHBoxLayout(m_limitWidget);
    limit->setContentsMargins({});
    limit->addWidget(m_limitEnabled);
    limit->addWidget(m_limit);
    limit->addStretch();

    auto* options = new QFormLayout();
    options->addRow(tr("Sort by:"), sorting);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_group, 1);
    layout->addLayout(options);

    QObject::connect(m_group, &AutoPlaylistRuleGroupWidget::changed, this, &AutoPlaylistRulesWidget::changed);
    QObject::connect(m_sortField, &AutoPlaylistFieldComboBox::fieldChanged, this, [this]() {
        m_sortDirection->setEnabled(!m_randomOrder->isChecked() && !m_sortField->field().isEmpty());
        Q_EMIT changed();
    });
    QObject::connect(m_sortDirection, &QComboBox::currentIndexChanged, this, &AutoPlaylistRulesWidget::changed);
    QObject::connect(m_randomOrder, &QCheckBox::toggled, this, [this](bool enabled) {
        m_sortField->setEnabled(!enabled);
        m_sortDirection->setEnabled(!enabled && !m_sortField->field().isEmpty());
        Q_EMIT changed();
    });
    QObject::connect(m_limitEnabled, &QCheckBox::toggled, this, [this](bool enabled) {
        m_limit->setEnabled(enabled);
        Q_EMIT changed();
    });
    QObject::connect(m_limit, &QSpinBox::valueChanged, this, &AutoPlaylistRulesWidget::changed);
}

AutoPlaylistRules AutoPlaylistRulesWidget::rules() const
{
    AutoPlaylistRules rules;

    rules.group      = m_group->group();
    rules.allTracks  = m_group->allTracks();
    rules.sortField  = m_sortField->field();
    rules.sortSource = m_sortField->source();
    rules.descending = m_sortDirection->currentIndex() == 1;
    rules.randomSort = m_randomOrder->isChecked();
    rules.limit      = m_limitEnabled->isChecked() ? m_limit->value() : 0;

    return rules;
}

QString AutoPlaylistRulesWidget::error() const
{
    return m_group->error();
}

QWidget* AutoPlaylistRulesWidget::limitWidget() const
{
    return m_limitWidget;
}

QWidget* AutoPlaylistRulesWidget::randomWidget() const
{
    return m_randomOrder;
}

void AutoPlaylistRulesWidget::setRules(const AutoPlaylistRules& rules)
{
    const QSignalBlocker blocker{this};

    m_group->setGroup(rules.group, rules.allTracks);
    m_sortField->setField(rules.sortField, rules.sortSource);
    m_sortDirection->setEnabled(!rules.randomSort && !rules.sortField.isEmpty());
    m_sortDirection->setCurrentIndex(rules.descending ? 1 : 0);
    m_randomOrder->setChecked(rules.randomSort);
    m_limitEnabled->setChecked(rules.limit > 0);
    m_limit->setValue(rules.limit > 0 ? rules.limit : 25);
}

QString AutoPlaylistRulesWidget::comparisonText(Expr::Type comparison, bool negated, bool relative)
{
    switch(comparison) {
        case Expr::Contains:
            //: Independent rule comparison label.
            return negated ? tr("does not contain")
                           //: Independent rule comparison label.
                           : tr("contains");
        case Expr::ContainsAny:
            //: Independent rule comparison label.
            return negated ? tr("does not contain any of")
                           //: Independent rule comparison label.
                           : tr("contains any of");
        case Expr::ContainsWord:
            //: Independent rule comparison label.
            return negated ? tr("does not contain word")
                           //: Independent rule comparison label.
                           : tr("contains word");
        case Expr::StartsWith:
            //: Independent rule comparison label.
            return negated ? tr("does not start with")
                           //: Independent rule comparison label.
                           : tr("starts with");
        case Expr::EndsWith:
            //: Independent rule comparison label.
            return negated ? tr("does not end with")
                           //: Independent rule comparison label.
                           : tr("ends with");
        case Expr::Equals:
            //: Independent rule comparison label.
            return negated ? tr("is not")
                           //: Independent rule comparison label.
                           : tr("is");
        case Expr::In:
            //: Independent rule comparison label.
            return negated ? tr("is not one of")
                           //: Independent rule comparison label.
                           : tr("is one of");
        case Expr::Greater:
            //: Independent rule comparison label.
            return tr("is greater than");
        case Expr::GreaterEqual:
            //: Independent rule comparison label.
            return tr("is at least");
        case Expr::Less:
            //: Independent rule comparison label.
            return tr("is less than");
        case Expr::LessEqual:
            //: Independent rule comparison label.
            return tr("is at most");
        case Expr::Present:
            //: Independent rule comparison label.
            return tr("is present");
        case Expr::Missing:
            //: Independent rule comparison label.
            return tr("is missing");
        case Expr::Before:
            //: Independent rule comparison label.
            return negated ? tr("is not before")
                           //: Independent rule comparison label.
                           : tr("is before");
        case Expr::After:
            //: Independent rule comparison label.
            return negated ? tr("is not after")
                           //: Independent rule comparison label.
                           : tr("is after");
        case Expr::Since:
            //: Independent rule comparison label.
            return negated ? tr("is not on or after")
                           //: Independent rule comparison label.
                           : tr("is on or after");
        case Expr::During:
            if(relative) {
                //: Independent rule comparison label.
                return negated ? tr("is not in the last")
                               //: Independent rule comparison label.
                               : tr("is in the last");
            }
            //: Independent rule comparison label.
            return negated ? tr("is not on")
                           //: Independent rule comparison label.
                           : tr("is on");
        default:
            return {};
    }
}

} // namespace Fooyin

#include "moc_autoplaylistruleswidget.cpp"
