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

#include "autoplaylistrulerowwidget.h"

#include "autoplaylistfieldcombobox.h"
#include "autoplaylistruleswidget.h"

#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <gui/widgets/hintlabel.h>
#include <utils/stareditor.h>

#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <array>
#include <limits>

using namespace Qt::StringLiterals;

constexpr auto Negated      = 1U << 8U;
constexpr auto RelativeDate = 1U << 9U;

namespace Fooyin {
class RuleComparisonComboBox : public QComboBox
{
public:
    explicit RuleComparisonComboBox(QWidget* parent)
        : QComboBox{parent}
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        const QFontMetrics fm{fontMetrics()};
        int width = std::max(fm.boundingRect(AutoPlaylistRuleRowWidget::tr("Yes")).width(),
                             fm.boundingRect(AutoPlaylistRuleRowWidget::tr("No")).width());

        // Metadata and date rules cover every non-boolean operator
        for(const auto type : {AutoPlaylistFieldType::Metadata, AutoPlaylistFieldType::Date}) {
            const auto comparisons = AutoPlaylistQueryBuilder::comparisons(type);
            for(const auto comparison : comparisons) {
                for(const bool negated : {false, true}) {
                    const auto label = AutoPlaylistRulesWidget::comparisonText(comparison, negated, false);
                    width            = std::max(width, fm.boundingRect(label).width());
                    if(comparison == Expr::During) {
                        const auto relativeLabel = AutoPlaylistRulesWidget::comparisonText(comparison, negated, true);
                        width                    = std::max(width, fm.boundingRect(relativeLabel).width());
                    }
                }
            }
        }

        QStyleOptionComboBox option;
        initStyleOption(&option);

        auto size = QComboBox::sizeHint();
        size.setWidth(style()->sizeFromContents(QStyle::CT_ComboBox, &option, {width, 0}, this).width());
        return size;
    }

    [[nodiscard]] QSize minimumSizeHint() const override
    {
        return sizeHint();
    }
};

class RuleRatingEditor : public StarEditor
{
public:
    explicit RuleRatingEditor(QWidget* parent)
        : StarEditor{Qt::AlignLeft, parent}
    {
        setFocusPolicy(Qt::StrongFocus);
        QObject::connect(this, &StarEditor::editingFinished, this, [this]() { setValue(rating().rating()); });
    }

    [[nodiscard]] float value() const
    {
        return m_value;
    }

    void setValue(float value)
    {
        m_value = value;
        setRating(StarRating{value, 5});
        update();
    }

protected:
    void leaveEvent(QEvent* event) override
    {
        setValue(m_value);
        StarEditor::leaveEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if(event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
            setValue(std::clamp(m_value + (event->key() == Qt::Key_Left ? -0.1F : 0.1F), 0.0F, 1.0F));
            Q_EMIT editingFinished();
            event->accept();
            return;
        }

        if(event->key() == Qt::Key_Escape) {
            setValue(m_value);
        }

        StarEditor::keyPressEvent(event);
    }

private:
    float m_value{0};
};

AutoPlaylistRuleRowWidget::AutoPlaylistRuleRowWidget(const AutoPlaylistRule& rule, QWidget* parent)
    : QWidget{parent}
    , m_field{new AutoPlaylistFieldComboBox(this)}
    , m_comparison{new RuleComparisonComboBox(this)}
    , m_values{new QStackedWidget(this)}
    , m_text{new QLineEdit(this)}
    , m_number{new QSpinBox(this)}
    , m_rating{new RuleRatingEditor(this)}
    , m_minutes{new QSpinBox(this)}
    , m_seconds{new QSpinBox(this)}
    , m_date{new QDateEdit(QDate::currentDate(), this)} //: Separates the two dates in a ‘between’ rule.
    , m_dateSeparator{new QLabel(tr("and"), this)}
    , m_endDate{new QDateEdit(QDate::currentDate(), this)}
    , m_period{new QSpinBox(this)}
    , m_unit{new QComboBox(this)}
    , m_listSummary{new QLineEdit(this)}
    , m_listValues{rule.values}
{
    m_text->setClearButtonEnabled(true);

    m_number->setRange(0, std::numeric_limits<int>::max());
    m_period->setRange(1, std::numeric_limits<int>::max() - 1);
    m_minutes->setRange(0, std::numeric_limits<int>::max() / 60);
    m_seconds->setRange(0, 59);

    m_minutes->setSuffix(u" min"_s);
    m_seconds->setSuffix(u" s"_s);

    for(auto* date : {m_date, m_endDate}) {
        date->setDateRange(QDate{1, 1, 1}, QDate{9999, 12, 31});
        date->setCalendarPopup(true);
    }

    for(size_t i{0}; i < AutoPlaylistQueryBuilder::dateUnits().size(); ++i) {
        m_unit->addItem(QString{});
    }

    auto* duration       = new QWidget(this);
    auto* durationLayout = new QHBoxLayout(duration);
    durationLayout->setContentsMargins({});
    durationLayout->addWidget(m_minutes);
    durationLayout->addWidget(m_seconds);

    auto* dates      = new QWidget(this);
    auto* dateLayout = new QHBoxLayout(dates);
    dateLayout->setContentsMargins({});
    dateLayout->addWidget(m_date, 1);
    dateLayout->addWidget(m_dateSeparator);
    dateLayout->addWidget(m_endDate, 1);

    auto* period       = new QWidget(this);
    auto* periodLayout = new QHBoxLayout(period);
    periodLayout->setContentsMargins({});

    periodLayout->addWidget(m_period);
    periodLayout->addWidget(m_unit);

    m_listSummary->setReadOnly(true);
    m_listSummary->setPlaceholderText(tr("No values"));

    auto* list       = new QWidget(this);
    auto* listLayout = new QHBoxLayout(list);
    listLayout->setContentsMargins({});

    auto* editList = new QPushButton(tr("Edit…"), list);
    editList->setAutoDefault(false);
    editList->setToolTip(tr("Edit values"));

    listLayout->addWidget(m_listSummary, 1);
    listLayout->addWidget(editList);

    m_values->addWidget(m_text);
    m_values->addWidget(m_number);
    m_values->addWidget(m_rating);
    m_values->addWidget(duration);
    m_values->addWidget(dates);
    m_values->addWidget(period);
    m_values->addWidget(new QWidget(this));
    m_values->addWidget(list);
    m_values->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* remove = new QToolButton(this);
    remove->setIcon(Gui::iconFromTheme(Constants::Icons::Remove));
    remove->setToolTip(tr("Remove rule"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});

    layout->addWidget(m_field);
    layout->addWidget(m_comparison);
    layout->addWidget(m_values, 1);
    layout->addWidget(remove);

    m_field->setField(rule.field, rule.source);

    updateComparisons();

    m_comparison->setCurrentIndex(m_comparison->findData(rule.comparison | (rule.negated ? Negated : 0)
                                                         | (rule.relativeDate ? RelativeDate : 0)));
    m_text->setText(rule.value);
    m_number->setValue(rule.value.toInt());
    m_rating->setValue(rule.value.toFloat() / 5.0F);

    const int durationSeconds = rule.value.toInt();
    m_minutes->setValue(durationSeconds / 60);
    m_seconds->setMaximum(m_minutes->value() == m_minutes->maximum() ? std::numeric_limits<int>::max() % 60 : 59);
    m_seconds->setValue(durationSeconds % 60);

    m_date->setDate(rule.date);
    m_endDate->setDate(rule.endDate);
    m_period->setValue(rule.period);
    m_unit->setCurrentIndex(rule.unit);

    updateDateUnits();
    updateListSummary();
    updateValue();

    for(const auto* spin : {m_number, m_seconds}) {
        QObject::connect(spin, &QSpinBox::valueChanged, this, &AutoPlaylistRuleRowWidget::changed);
    }

    QObject::connect(m_field, &AutoPlaylistFieldComboBox::fieldChanged, this, [this]() {
        updateComparisons();
        updateValue();
        Q_EMIT changed();
    });
    QObject::connect(m_comparison, &QComboBox::currentIndexChanged, this, [this]() {
        updateValue();
        Q_EMIT changed();
    });
    QObject::connect(m_text, &QLineEdit::textChanged, this, &AutoPlaylistRuleRowWidget::changed);

    QObject::connect(m_minutes, &QSpinBox::valueChanged, this, [this](int minutes) {
        m_seconds->setMaximum(minutes == m_minutes->maximum() ? std::numeric_limits<int>::max() % 60 : 59);
        Q_EMIT changed();
    });
    QObject::connect(m_period, &QSpinBox::valueChanged, this, [this]() {
        updateDateUnits();
        Q_EMIT changed();
    });
    QObject::connect(m_rating, &StarEditor::editingFinished, this, &AutoPlaylistRuleRowWidget::changed);
    QObject::connect(m_date, &QDateEdit::dateChanged, this, &AutoPlaylistRuleRowWidget::changed);
    QObject::connect(m_endDate, &QDateEdit::dateChanged, this, &AutoPlaylistRuleRowWidget::changed);
    QObject::connect(m_unit, &QComboBox::currentIndexChanged, this, &AutoPlaylistRuleRowWidget::changed);
    QObject::connect(remove, &QToolButton::clicked, this, &AutoPlaylistRuleRowWidget::removeRequested);
    QObject::connect(editList, &QPushButton::clicked, this, &AutoPlaylistRuleRowWidget::editList);
}

AutoPlaylistRule AutoPlaylistRuleRowWidget::rule() const
{
    const auto comparison = m_comparison->currentData().toUInt();

    AutoPlaylistRule rule;
    rule.field        = m_field->field();
    rule.source       = m_field->source();
    rule.comparison   = static_cast<Expr::Type>(comparison & 255U);
    rule.negated      = (comparison & Negated) != 0U;
    rule.relativeDate = (comparison & RelativeDate) != 0U;
    rule.date         = m_date->date();
    rule.endDate      = m_endDate->date();
    rule.period       = m_period->value();
    rule.unit         = m_unit->currentIndex();

    if(rule.comparison == Expr::In || rule.comparison == Expr::ContainsAny) {
        rule.values = m_listValues;
        return rule;
    }

    switch(AutoPlaylistQueryBuilder::fieldType(rule.field, rule.source)) {
        case AutoPlaylistFieldType::Text:
        case AutoPlaylistFieldType::Metadata:
            rule.value = m_text->text();
            break;
        case AutoPlaylistFieldType::Number:
            rule.value = QString::number(m_number->value());
            break;
        case AutoPlaylistFieldType::Rating:
            rule.value = QString::number(m_rating->value() * 5.0F);
            break;
        case AutoPlaylistFieldType::Duration:
            rule.value = QString::number((m_minutes->value() * 60LL) + m_seconds->value());
            break;
        case AutoPlaylistFieldType::Date:
            break;
        case AutoPlaylistFieldType::Boolean:
            rule.value = u"1"_s;
            break;
    }

    return rule;
}

void AutoPlaylistRuleRowWidget::updateDateUnits()
{
    const QSignalBlocker blocker{m_unit};

    const int count  = m_period->value();
    const auto units = AutoPlaylistQueryBuilder::dateUnits();
    for(int i{0}; std::cmp_less(i, units.size()); ++i) {
        const char* source = count == 1 ? units[i].singular : units[i].plural;
        m_unit->setItemText(i, AutoPlaylistRulesWidget::tr(source, nullptr, count));
    }
}

void AutoPlaylistRuleRowWidget::updateComparisons()
{
    const QSignalBlocker blocker{m_comparison};

    const int previous = m_comparison->currentData().toInt();
    m_comparison->clear();

    const auto type = AutoPlaylistQueryBuilder::fieldType(m_field->field(), m_field->source());

    const auto add = [this, type](Expr::Type comparison, bool negated, bool relative = false) {
        const QString label = type == AutoPlaylistFieldType::Boolean
                                ? (negated ? tr("No") : tr("Yes"))
                                : AutoPlaylistRulesWidget::comparisonText(comparison, negated, relative);
        m_comparison->addItem(label, comparison | (negated ? Negated : 0) | (relative ? RelativeDate : 0));
    };

    for(const auto comparison : AutoPlaylistQueryBuilder::comparisons(type)) {
        add(comparison, false);

        if(comparison == Expr::Equals || comparison == Expr::Contains || comparison == Expr::StartsWith
           || comparison == Expr::EndsWith || comparison == Expr::In || comparison == Expr::ContainsAny
           || comparison == Expr::ContainsWord
           || (type == AutoPlaylistFieldType::Date && comparison != Expr::Missing && comparison != Expr::Present)) {
            add(comparison, true);
        }
        if(comparison == Expr::During) {
            add(comparison, false, true);
            add(comparison, true, true);
        }
    }

    const int index = m_comparison->findData(previous);
    m_comparison->setCurrentIndex(index >= 0 ? index : 0);
}

void AutoPlaylistRuleRowWidget::updateValue()
{
    const auto comparison = m_comparison->currentData().toUInt();
    const auto type       = static_cast<Expr::Type>(comparison & 255U);

    m_dateSeparator->setVisible(type == Expr::Between);
    m_endDate->setVisible(type == Expr::Between);

    if(type == Expr::Missing || type == Expr::Present) {
        m_values->setCurrentIndex(6);
        return;
    }
    if(type == Expr::In || type == Expr::ContainsAny) {
        m_values->setCurrentIndex(7);
        return;
    }

    switch(AutoPlaylistQueryBuilder::fieldType(m_field->field(), m_field->source())) {
        case AutoPlaylistFieldType::Text:
        case AutoPlaylistFieldType::Metadata:
            m_values->setCurrentIndex(0);
            break;
        case AutoPlaylistFieldType::Number:
            m_values->setCurrentIndex(1);
            break;
        case AutoPlaylistFieldType::Rating:
            m_values->setCurrentIndex(2);
            break;
        case AutoPlaylistFieldType::Duration:
            m_values->setCurrentIndex(3);
            break;
        case AutoPlaylistFieldType::Date:
            m_values->setCurrentIndex(comparison & RelativeDate ? 5 : 4);
            break;
        case AutoPlaylistFieldType::Boolean:
            m_values->setCurrentIndex(6);
            break;
    }
}

void AutoPlaylistRuleRowWidget::editList()
{
    QDialog dialog{this};
    dialog.setWindowTitle(tr("Edit Values"));

    auto* editor = new QPlainTextEdit(&dialog);
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor->setTabChangesFocus(true);

    QStringList lines;
    for(const auto& value : m_listValues) {
        lines.push_back(value);
    }
    editor->setPlainText(lines.join(u'\n'));

    const auto type = AutoPlaylistQueryBuilder::fieldType(m_field->field(), m_field->source());

    const QString hint = tr("Enter at least one value.");
    QString help       = tr("Empty lines are ignored.");
    QString error      = tr("Enter a valid value on each line.");

    if(type == AutoPlaylistFieldType::Number || type == AutoPlaylistFieldType::Duration) {
        help = type == AutoPlaylistFieldType::Duration ? tr("Values are durations in seconds. Empty lines are ignored.")
                                                       : tr("Enter whole numbers. Empty lines are ignored.");
        error = type == AutoPlaylistFieldType::Duration
                  ? tr("Enter whole numbers of seconds, from 0 to %1.").arg(std::numeric_limits<int>::max())
                  : tr("Enter whole numbers from 0 to %1.").arg(std::numeric_limits<int>::max());
    }
    else if(type == AutoPlaylistFieldType::Rating) {
        help  = tr("Half-star ratings are supported. Empty lines are ignored.");
        error = tr("Enter ratings from 0 to 5 in half-star steps.");
    }

    auto* messages = new QStackedWidget(&dialog);
    messages->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    for(const auto& [text, messageType] :
        std::array{std::pair{help, HintLabel::Type::Information}, std::pair{hint, HintLabel::Type::Information},
                   std::pair{error, HintLabel::Type::Warning}}) {
        auto* label = new HintLabel(text, messageType, messages);
        label->setTextFormat(Qt::PlainText);
        messages->addWidget(label);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);

    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("Values (one per line)") + u":"_s, &dialog));
    layout->addWidget(editor, 1);
    layout->addWidget(messages);
    layout->addWidget(buttons);

    const auto values = [editor]() -> std::vector<QString> {
        const QStringList entries = editor->toPlainText().split(u'\n', Qt::SkipEmptyParts);
        return {entries.begin(), entries.end()};
    };

    const auto validate = [values, type, messages, buttons](bool showEmptyError) {
        const auto entries = values();
        const bool valid   = !entries.empty() && std::ranges::all_of(entries, [type](const QString& value) {
            return AutoPlaylistQueryBuilder::isListValueValid(value, type);
        });
        messages->setCurrentIndex(entries.empty() ? (showEmptyError ? 1 : 0) : (valid ? 0 : 2));
        buttons->button(QDialogButtonBox::Ok)->setEnabled(valid);
    };

    QObject::connect(editor, &QPlainTextEdit::textChanged, &dialog, [validate]() { validate(true); });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    validate(false);
    dialog.resize(420, 320);
    editor->setFocus();

    if(dialog.exec() == QDialog::Accepted) {
        m_listValues = values();
        updateListSummary();
        Q_EMIT changed();
    }
}

void AutoPlaylistRuleRowWidget::updateListSummary()
{
    QStringList values;
    for(const auto& value : m_listValues) {
        values.push_back(value);
    }
    m_listSummary->setText(values.join(u", "_s));
    m_listSummary->setCursorPosition(0);
    m_listSummary->setToolTip(values.join(u'\n'));
}
} // namespace Fooyin
