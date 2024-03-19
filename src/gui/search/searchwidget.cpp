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

#include "searchwidget.h"

#include "searchcontroller.h"

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/actions/actioncontainer.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>
#include <utils/widgets/popuplineedit.h>

#include <QHBoxLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenu>
#include <QStyleOptionFrame>

constexpr auto DefaultPlaceholder = "Search library...";

namespace Fooyin {
SearchWidget::SearchWidget(SearchController* controller, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_searchController{controller}
    , m_settings{settings}
    , m_searchBox{new QLineEdit(this)}
{
    setObjectName(SearchWidget::name());

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(m_searchBox);

    m_searchBox->setPlaceholderText(QString::fromLatin1(DefaultPlaceholder));
    m_searchBox->setClearButtonEnabled(true);

    QObject::connect(m_searchBox, &QLineEdit::textChanged, this,
                     [this](const QString& search) { m_searchController->changeSearch(id(), search); });

    auto* selectReceiver = new QAction(Utils::iconFromTheme(Constants::Icons::Options), tr("Options"), this);
    QObject::connect(selectReceiver, &QAction::triggered, this, [this]() { showOptionsMenu(); });
    m_searchBox->addAction(selectReceiver, QLineEdit::TrailingPosition);

    m_settings->subscribe<Settings::Gui::IconTheme>(
        this, [selectReceiver]() { selectReceiver->setIcon(Utils::iconFromTheme(Constants::Icons::Options)); });
}

SearchWidget::~SearchWidget()
{
    m_searchController->removeConnectedWidgets(id());
}

QString SearchWidget::name() const
{
    return QStringLiteral("Search Bar");
}

QString SearchWidget::layoutName() const
{
    return QStringLiteral("SearchBar");
}

void SearchWidget::layoutEditingMenu(ActionContainer* menu)
{
    auto* manageConnections = new QAction(tr("Manage Connections"), this);
    QObject::connect(manageConnections, &QAction::triggered, this,
                     [this]() { m_searchController->setupWidgetConnections(id()); });
    menu->addAction(manageConnections);
}

void SearchWidget::saveLayoutData(QJsonObject& layout)
{
    const QString placeholderText = m_searchBox->placeholderText();
    if(!placeholderText.isEmpty() && placeholderText != QString::fromLatin1(DefaultPlaceholder)) {
        layout[QStringLiteral("Placeholder")] = placeholderText;
    }

    const auto connectedWidgets = m_searchController->connectedWidgets(id());

    if(connectedWidgets.empty()) {
        return;
    }

    QStringList widgetIds;
    std::ranges::transform(connectedWidgets, std::back_inserter(widgetIds), [](const Id& id) { return id.name(); });

    layout[QStringLiteral("Widgets")] = widgetIds.join(QStringLiteral("|"));
}

void SearchWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(QStringLiteral("Placeholder"))) {
        m_searchBox->setPlaceholderText(layout.value(QStringLiteral("Placeholder")).toString());
    }

    if(!layout.contains(QStringLiteral("Widgets"))) {
        return;
    }

    const QStringList widgetIds = layout.value(QStringLiteral("Widgets")).toString().split(QStringLiteral("|"));

    if(widgetIds.isEmpty()) {
        return;
    }

    IdSet connectedWidgets;

    for(const QString& id : widgetIds) {
        connectedWidgets.emplace(id.trimmed());
    }

    m_searchController->setConnectedWidgets(id(), connectedWidgets);
}

void SearchWidget::changePlaceholderText()
{
    auto* lineEdit = new PopupLineEdit(m_searchBox->placeholderText(), this);
    lineEdit->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(lineEdit, &PopupLineEdit::editingCancelled, lineEdit, &QWidget::close);
    QObject::connect(lineEdit, &PopupLineEdit::editingFinished, this, [this, lineEdit]() {
        const QString text = lineEdit->text();
        if(text != m_searchBox->placeholderText()) {
            m_searchBox->setPlaceholderText(!text.isEmpty() ? text : QString::fromLatin1(DefaultPlaceholder));
        }
        lineEdit->close();
    });

    lineEdit->setGeometry(m_searchBox->geometry());
    lineEdit->show();
    lineEdit->selectAll();
    lineEdit->setFocus(Qt::ActiveWindowFocusReason);
}

void SearchWidget::showOptionsMenu()
{
    auto* menu = new QMenu(tr("Options"), this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* changePlaceholder = new QAction(tr("Change Placeholder Text"), this);
    QObject::connect(changePlaceholder, &QAction::triggered, this, &SearchWidget::changePlaceholderText);
    menu->addAction(changePlaceholder);

    auto* manageConnections = new QAction(tr("Manage Connections"), this);
    QObject::connect(manageConnections, &QAction::triggered, this,
                     [this]() { m_searchController->setupWidgetConnections(id()); });
    menu->addAction(manageConnections);

    QStyleOptionFrame opt;
    opt.initFrom(m_searchBox);

    const QRect rect = m_searchBox->style()->subElementRect(QStyle::SE_LineEditContents, &opt, m_searchBox);
    const QPoint pos{rect.right() - 5, rect.center().y()};

    menu->popup(m_searchBox->mapToGlobal(pos));
}
} // namespace Fooyin

#include "moc_searchwidget.cpp"
