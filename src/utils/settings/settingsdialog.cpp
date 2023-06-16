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

#include "settingspage.h"
#include "utils/scrollarea.h"
#include "utils/simplelistview.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QSize>
#include <QStackedLayout>

namespace Fy::Utils {
SettingsDialog::SettingsDialog(PageList pages, QWidget* parent)
    : QDialog{parent}
    , m_categoryList{new SimpleListView(this)}
    , m_mainGridLayout{new QGridLayout(this)}
    , m_stackedLayout{new QStackedLayout()}
    , m_pages{std::move(pages)}
{
    setupUi();
    setWindowTitle(tr("Settings"));

    m_model.setPages(m_pages);

    // TODO: Create icon size setting
    m_categoryList->setIconSize({20, 20});
    m_categoryList->setModel(&m_model);

    connect(m_categoryList->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            &SettingsDialog::currentChanged);

    m_categoryList->setFocus();
}

SettingsDialog::~SettingsDialog()
{
    m_pages.clear();
}

void SettingsDialog::setupUi()
{
    m_stackedLayout->setContentsMargins(0, 0, 0, 0);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel);

    connect(buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this, &SettingsDialog::apply);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);

    m_mainGridLayout->addWidget(m_categoryList, 0, 0);
    m_mainGridLayout->addLayout(m_stackedLayout, 0, 1);
    m_mainGridLayout->addWidget(buttonBox, 1, 0, 1, 2);
    m_mainGridLayout->setColumnStretch(1, 4);

    buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);
    m_mainGridLayout->setSizeConstraint(QLayout::SetMinimumSize);
}

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

    int categoryIndex = -1;
    int pageIndex     = -1;

    const CategoryList& categories = m_model.categories();
    const int categoriesCount      = static_cast<int>(categories.size());

    for(int i = 0; i < categoriesCount; ++i) {
        SettingsCategory* category = categories.at(i);
        if(category->findPageById(id, &pageIndex)) {
            categoryIndex = i;
            break;
        }
    }

    if(pageIndex < 0) {
        qWarning() << "Page not found: " << id.name();
        return;
    }

    if(categoryIndex >= 0) {
        const QModelIndex modelIndex = m_model.index(categoryIndex);
        m_categoryList->setCurrentIndex(modelIndex);

        if(auto* widget = categories.at(categoryIndex)->tabWidget) {
            widget->setCurrentIndex(pageIndex);
        }
    }
}

void SettingsDialog::showCategory(int index)
{
    const CategoryList& categories = m_model.categories();
    const int count                = static_cast<int>(categories.size());

    if(count == 0 || index < 0 || index > count) {
        return;
    }

    auto* category = categories.at(index);
    checkCategoryWidget(category);
    m_currentCategory         = category->id;
    const int currentTabIndex = category->tabWidget->currentIndex();
    if(currentTabIndex != -1) {
        auto* page    = category->pages.at(currentTabIndex);
        m_currentPage = page->id();
        m_visitedPages.emplace(page);
    }
    m_stackedLayout->setCurrentIndex(category->index);
}

void SettingsDialog::checkCategoryWidget(SettingsCategory* category)
{
    if(category->tabWidget) {
        return;
    }

    auto* tabWidget = new QTabWidget();
    tabWidget->setTabBarAutoHide(true);
    tabWidget->setDocumentMode(true);
    for(const auto& page : category->pages) {
        QWidget* widget = page->widget();
        if(widget) {
            auto* scrollArea = new ScrollArea(this);
            scrollArea->setWidget(widget);
            tabWidget->addTab(scrollArea, page->name());
        }
    }

    connect(tabWidget, &QTabWidget::currentChanged, this, &SettingsDialog::currentTabChanged);

    category->tabWidget = tabWidget;
    category->index     = m_stackedLayout->addWidget(tabWidget);
}

void SettingsDialog::done(int value)
{
    QDialog::done(value);
}

void SettingsDialog::accept()
{
    apply();
    for(const auto& page : m_pages) {
        page->finish();
    }
    done(Accepted);
}

void SettingsDialog::reject()
{
    for(const auto& page : m_pages) {
        page->finish();
    }
    done(Rejected);
}

void SettingsDialog::apply()
{
    for(const auto& page : m_visitedPages) {
        page->apply();
    }
}

void SettingsDialog::currentChanged(const QModelIndex& current)
{
    if(current.isValid()) {
        showCategory(current.row());
    }
    else {
        m_stackedLayout->setCurrentIndex(0);
    }
}

void SettingsDialog::currentTabChanged(int index)
{
    if(index < 0) {
        return;
    }

    const QModelIndex modelIndex = m_categoryList->currentIndex();
    if(!modelIndex.isValid()) {
        return;
    }

    SettingsCategory* category = m_model.categories().at(modelIndex.row());
    SettingsPage* page         = category->pages.at(index);
    m_currentPage              = page->id();
    m_visitedPages.emplace(page);
}
} // namespace Fy::Utils
