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

#include "settingsdialog.h"

#include "settings/settingsmodel.h"
#include "settingspage.h"
#include "utils/scrollarea.h"
#include "utils/simpletreeview.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QSize>
#include <QStackedLayout>
#include <QTreeView>

namespace Fy::Utils {
struct SettingsDialog::Private : QObject
{
    SettingsDialog* self;
    SettingsModel* model;
    SimpleTreeView* categoryTree;
    QStackedLayout* stackedLayout;

    PageList pages;
    std::set<SettingsPage*> visitedPages;

    Id currentCategory;
    Id currentPage;

    Private(SettingsDialog* self, PageList pageList)
        : self{self}
        , model{new SettingsModel(self)}
        , categoryTree{new SimpleTreeView(self)}
        , stackedLayout{new QStackedLayout()}
        , pages{std::move(pageList)}
    {
        stackedLayout->setContentsMargins(0, 0, 0, 0);

        model->setPages(pages);

        categoryTree->setHeaderHidden(true);
        categoryTree->setModel(model);
        categoryTree->setFocus();
        categoryTree->expandAll();

        auto* buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Reset | QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel, self);
        buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);

        auto* mainGridLayout = new QGridLayout(self);
        mainGridLayout->addWidget(categoryTree, 0, 0);
        mainGridLayout->addLayout(stackedLayout, 0, 1);
        mainGridLayout->addWidget(buttonBox, 1, 0, 1, 2);
        mainGridLayout->setColumnStretch(1, 4);
        mainGridLayout->setSizeConstraint(QLayout::SetMinimumSize);

        connect(categoryTree->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
                &SettingsDialog::Private::currentChanged);
        connect(buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this,
                &SettingsDialog::Private::apply);
        connect(buttonBox->button(QDialogButtonBox::Reset), &QAbstractButton::clicked, this,
                &SettingsDialog::Private::reset);
        connect(buttonBox, &QDialogButtonBox::accepted, self, &SettingsDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, self, &SettingsDialog::reject);
    }

    void apply()
    {
        for(const auto& page : visitedPages) {
            page->apply();
        }
    }

    void reset()
    {
        if(auto page = findPage(currentPage)) {
            page->reset();
        }
    }

    void showCategory(const QModelIndex& index)
    {
        auto* category = index.data(SettingsItem::Data).value<SettingsCategory*>();
        if(!category) {
            return;
        }

        checkCategoryWidget(category);

        currentCategory = category->id;

        const int currentTabIndex = category->tabWidget->currentIndex();
        if(currentTabIndex != -1) {
            auto* page  = category->pages.at(currentTabIndex);
            currentPage = page->id();
            visitedPages.emplace(page);
        }
        stackedLayout->setCurrentIndex(category->index);

        self->setWindowTitle(tr("Settings: ") + category->name);
    }

    void checkCategoryWidget(SettingsCategory* category)
    {
        if(category->tabWidget) {
            return;
        }

        auto* tabWidget = new QTabWidget();
        tabWidget->setTabBarAutoHide(true);
        tabWidget->setDocumentMode(true);

        const auto addPageToTabWidget = [tabWidget](const auto& page) {
            if(QWidget* widget = page->widget()) {
                auto* scrollArea = new ScrollArea(tabWidget);
                scrollArea->setWidget(widget);
                tabWidget->addTab(scrollArea, page->name());
            }
        };

        std::ranges::for_each(category->pages, addPageToTabWidget);

        connect(tabWidget, &QTabWidget::currentChanged, this, &SettingsDialog::Private::currentTabChanged);

        category->tabWidget = tabWidget;
        category->index     = stackedLayout->addWidget(tabWidget);
    }

    void currentChanged(const QModelIndex& current)
    {
        if(current.isValid()) {
            showCategory(current);
        }
        else {
            stackedLayout->setCurrentIndex(0);
        }
    }

    void currentTabChanged(int index)
    {
        if(index < 0) {
            return;
        }

        const QModelIndex modelIndex = categoryTree->currentIndex();
        if(!modelIndex.isValid()) {
            return;
        }

        auto* category     = modelIndex.data(SettingsItem::Data).value<SettingsCategory*>();
        SettingsPage* page = category->pages.at(index);
        currentPage        = page->id();
        visitedPages.emplace(page);
    }

    SettingsPage* findPage(const Id& id)
    {
        auto it = std::ranges::find_if(std::as_const(pages), [&id](SettingsPage* page) {
            return page->id() == id;
        });
        if(it != pages.cend()) {
            return *it;
        }
        return nullptr;
    }
};

SettingsDialog::SettingsDialog(const PageList& pages, QWidget* parent)
    : QDialog{parent}
    , p{std::make_unique<Private>(this, pages)}
{
    setWindowTitle(tr("Settings"));
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::openSettings()
{
    open();
}

void SettingsDialog::openPage(const Id& id)
{
    if(!id.isValid()) {
        qWarning() << "Invalid page id: " << id.name();
        return;
    }

    auto category = p->model->categoryForPage(id);
    if(!category) {
        qWarning() << "Page not found: " << id.name();
        return;
    }

    const QModelIndex categoryIndex = p->model->indexForCategory(category->id);
    p->categoryTree->setCurrentIndex(categoryIndex);

    if(auto* widget = category->tabWidget) {
        const int pageIndex = category->findPageById(id);
        if(pageIndex >= 0) {
            widget->setCurrentIndex(pageIndex);
        }
    }
}

void SettingsDialog::done(int value)
{
    QDialog::done(value);
}

void SettingsDialog::accept()
{
    p->apply();
    for(const auto& page : p->pages) {
        page->finish();
    }
    done(Accepted);
}

void SettingsDialog::reject()
{
    for(const auto& page : p->pages) {
        page->finish();
    }
    done(Rejected);
}
} // namespace Fy::Utils
