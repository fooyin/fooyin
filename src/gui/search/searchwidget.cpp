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

#include <QHBoxLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenu>
#include <QStyleOptionFrame>

constexpr auto Placeholder = "Search library...";

using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
struct SearchWidget::Private
{
    SearchWidget* self;

    SearchController* searchController;
    SettingsManager* settings;

    QLineEdit* searchBox;

    Private(SearchWidget* self_, SearchController* controller_, SettingsManager* settings_)
        : self{self_}
        , searchController{controller_}
        , settings{settings_}
        , searchBox{new QLineEdit(self)}
    {
        auto* layout = new QHBoxLayout(self);
        layout->setContentsMargins(0, 0, 0, 0);

        layout->addWidget(searchBox);

        searchBox->setPlaceholderText(Placeholder);
        searchBox->setClearButtonEnabled(true);
    }

    void setupConnections() const
    {
        searchController->setupWidgetConnections(self->id());
    }

    QPoint optionsMenuPoint() const
    {
        QStyleOptionFrame opt;
        opt.initFrom(searchBox);

        const QRect rect = searchBox->style()->subElementRect(QStyle::SE_LineEditContents, &opt, searchBox);
        const QPoint pos{rect.right() - 5, rect.center().y()};

        return searchBox->mapToGlobal(pos);
    }

    void showOptionsMenu()
    {
        auto* menu = new QMenu(tr("Options"), self);
        menu->setAttribute(Qt::WA_DeleteOnClose);

        auto* manageConnections = new QAction(tr("Manage Connections"), self);
        QObject::connect(manageConnections, &QAction::triggered, self, [this]() { setupConnections(); });
        menu->addAction(manageConnections);

        menu->popup(optionsMenuPoint());
    }
};

SearchWidget::SearchWidget(SearchController* controller, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, controller, settings)}
{
    setObjectName(SearchWidget::name());

    QObject::connect(p->searchBox, &QLineEdit::textChanged, this,
                     [this](const QString& search) { p->searchController->changeSearch(id(), search); });

    auto* selectReceiver = new QAction(QIcon::fromTheme(Constants::Icons::Options), tr("Options"), this);
    QObject::connect(selectReceiver, &QAction::triggered, this, [this]() { p->showOptionsMenu(); });
    p->searchBox->addAction(selectReceiver, QLineEdit::TrailingPosition);

    settings->subscribe<Settings::Gui::IconTheme>(
        this, [selectReceiver]() { selectReceiver->setIcon(QIcon::fromTheme(Constants::Icons::Options)); });
}

SearchWidget::~SearchWidget()
{
    p->searchController->removeConnectedWidgets(id());
}

QString SearchWidget::name() const
{
    return u"Search Bar"_s;
}

QString SearchWidget::layoutName() const
{
    return u"SearchBar"_s;
}

void SearchWidget::layoutEditingMenu(ActionContainer* menu)
{
    auto* manageConnections = new QAction(tr("Manage Connections"), this);
    QObject::connect(manageConnections, &QAction::triggered, this, [this]() { p->setupConnections(); });
    menu->addAction(manageConnections);
}

void SearchWidget::saveLayoutData(QJsonObject& layout)
{
    const auto connectedWidgets = p->searchController->connectedWidgets(id());

    if(connectedWidgets.empty()) {
        return;
    }

    QStringList widgetIds;
    std::ranges::transform(connectedWidgets, std::back_inserter(widgetIds), [](const Id& id) { return id.name(); });

    layout["Widgets"_L1] = widgetIds.join("|"_L1);
}

void SearchWidget::loadLayoutData(const QJsonObject& layout)
{
    if(!layout.contains("Widgets"_L1)) {
        return;
    }

    const QStringList widgetIds = layout["Widgets"_L1].toString().split("|");

    if(widgetIds.isEmpty()) {
        return;
    }

    IdSet connectedWidgets;

    for(const QString& id : widgetIds) {
        connectedWidgets.emplace(id.trimmed());
    }

    p->searchController->setConnectedWidgets(id(), connectedWidgets);
}
} // namespace Fooyin

#include "moc_searchwidget.cpp"
