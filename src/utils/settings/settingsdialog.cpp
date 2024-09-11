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

#include "settingsdialog.h"

#include "scrollarea.h"
#include "settingsmodel.h"

#include <utils/settings/settingsmanager.h>
#include <utils/settings/settingspage.h>

#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedLayout>
#include <QTreeView>

namespace Fooyin {
class SimpleTreeView : public QTreeView
{
    Q_OBJECT

public:
    explicit SimpleTreeView(QWidget* parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;

private:
    [[nodiscard]] int calculateMaxItemWidth(const QModelIndex& index) const;
};

SimpleTreeView::SimpleTreeView(QWidget* parent)
    : QTreeView{parent}
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

QSize SimpleTreeView::sizeHint() const
{
    const int maxWidth = calculateMaxItemWidth({});
    return {maxWidth, 100};
}

int SimpleTreeView::calculateMaxItemWidth(const QModelIndex& index) const
{
    int maxItemWidth{0};

    const int itemWidth = sizeHintForIndex(index).width();
    maxItemWidth        = std::max(maxItemWidth, itemWidth);

    const int rowCount = model()->rowCount(index);
    for(int row{0}; row < rowCount; ++row) {
        const QModelIndex childIndex = model()->index(row, 0, index);
        const int childWidth         = calculateMaxItemWidth(childIndex);
        maxItemWidth                 = std::max(maxItemWidth, childWidth);
    }
    maxItemWidth += indentation();
    return maxItemWidth;
}

SettingsDialog::SettingsDialog(PageList pages, QWidget* parent)
    : QDialog{parent}
    , m_model{new SettingsModel(this)}
    , m_categoryTree{new SimpleTreeView(this)}
    , m_stackedLayout{new QStackedLayout()}
    , m_buttonBox{new QDialogButtonBox(QDialogButtonBox::RestoreDefaults | QDialogButtonBox::Reset
                                           | QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel,
                                       this)}
    , m_pages{std::move(pages)}
{
    setWindowTitle(tr("Settings"));
    setModal(true);

    m_stackedLayout->setContentsMargins(0, 0, 0, 0);

    m_model->setPages(m_pages);

    m_categoryTree->setHeaderHidden(true);
    m_categoryTree->setModel(m_model);
    m_categoryTree->setFocus();
    m_categoryTree->expandAll();

    m_buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);
    m_buttonBox->button(QDialogButtonBox::RestoreDefaults)->setText(tr("Reset Page"));
    m_buttonBox->button(QDialogButtonBox::Reset)->setText(tr("Reset All"));

    auto* mainGridLayout = new QGridLayout(this);
    mainGridLayout->addWidget(m_categoryTree, 0, 0);
    mainGridLayout->addLayout(m_stackedLayout, 0, 1);
    mainGridLayout->addWidget(m_buttonBox, 1, 0, 1, 2);
    mainGridLayout->setColumnStretch(1, 4);
    mainGridLayout->setSizeConstraint(QLayout::SetMinimumSize);

    QObject::connect(m_categoryTree->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
                     &SettingsDialog::currentChanged);
    QObject::connect(m_buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this,
                     &SettingsDialog::apply);
    QObject::connect(m_buttonBox->button(QDialogButtonBox::Reset), &QAbstractButton::clicked, this,
                     &SettingsDialog::resetAll);
    QObject::connect(m_buttonBox->button(QDialogButtonBox::RestoreDefaults), &QAbstractButton::clicked, this,
                     &SettingsDialog::reset);
    QObject::connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    QObject::connect(m_buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
}

void SettingsDialog::openSettings()
{
    open();
}

void SettingsDialog::openPage(const Id& id)
{
    if(!id.isValid()) {
        qCWarning(SETTINGS) << "Invalid page id:" << id.name();
        return;
    }

    auto* category = m_model->categoryForPage(id);
    if(!category) {
        qCWarning(SETTINGS) << "Page not found:" << id.name();
        return;
    }

    const QModelIndex categoryIndex = m_model->indexForCategory(category->id);
    m_categoryTree->setCurrentIndex(categoryIndex);

    if(auto* widget = category->tabWidget) {
        const int pageIndex = category->findPageById(id);
        if(pageIndex >= 0) {
            widget->setCurrentIndex(pageIndex);
        }
    }
}

Id SettingsDialog::currentPage() const
{
    return m_currentPage;
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

void SettingsDialog::reset()
{
    if(auto* page = findPage(m_currentPage)) {
        page->reset();
        page->load();
    }
}

void SettingsDialog::resetAll()
{
    QMessageBox message;
    message.setIcon(QMessageBox::Warning);
    message.setText(tr("Are you sure?"));
    message.setInformativeText(tr("This will reset all settings to default."));

    message.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    message.setDefaultButton(QMessageBox::No);

    const int buttonClicked = message.exec();

    if(buttonClicked == QMessageBox::Yes) {
        QMetaObject::invokeMethod(this, &SettingsDialog::resettingAll);
        for(auto* page : m_pages) {
            page->load();
        }
    }
}

void SettingsDialog::showCategory(const QModelIndex& index)
{
    auto* category = index.data(SettingsItem::Data).value<SettingsCategory*>();
    if(!category) {
        return;
    }

    checkCategoryWidget(category);

    m_currentCategory = category->id;

    const int currentTabIndex = category->tabWidget->currentIndex();
    if(currentTabIndex != -1) {
        auto* page    = category->pages.at(currentTabIndex);
        m_currentPage = page->id();
        m_visitedPages.emplace(page);
    }
    m_stackedLayout->setCurrentIndex(category->index);

    setWindowTitle(tr("Settings") + QLatin1String(": ") + category->name);
}

void SettingsDialog::checkCategoryWidget(SettingsCategory* category)
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

            page->load();
        }
    };

    std::ranges::for_each(category->pages, addPageToTabWidget);

    QObject::connect(tabWidget, &QTabWidget::currentChanged, this, &SettingsDialog::currentTabChanged);

    category->tabWidget = tabWidget;
    category->index     = m_stackedLayout->addWidget(tabWidget);
}

void SettingsDialog::currentChanged(const QModelIndex& current)
{
    if(current.isValid()) {
        showCategory(current);
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

    const QModelIndex modelIndex = m_categoryTree->currentIndex();
    if(!modelIndex.isValid()) {
        return;
    }

    auto* category     = modelIndex.data(SettingsItem::Data).value<SettingsCategory*>();
    SettingsPage* page = category->pages.at(index);
    m_currentPage      = page->id();
    m_visitedPages.emplace(page);
}

SettingsPage* SettingsDialog::findPage(const Id& id)
{
    auto it = std::ranges::find_if(std::as_const(m_pages), [&id](SettingsPage* page) { return page->id() == id; });
    if(it != m_pages.cend()) {
        return *it;
    }
    return nullptr;
}
} // namespace Fooyin

#include "moc_settingsdialog.cpp"
#include "settingsdialog.moc"
