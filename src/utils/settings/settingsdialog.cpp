/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "settingsmodel.h"

#include <utils/modelutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/settings/settingspage.h>
#include <utils/utils.h>

#include <QDataStream>
#include <QDialogButtonBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QIODevice>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStackedLayout>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTreeView>
#include <QVBoxLayout>

constexpr auto PageListPadding = 50;

namespace Fooyin {
class SettingsFilterModel : public QSortFilterProxyModel
{
public:
    explicit SettingsFilterModel(QObject* parent)
        : QSortFilterProxyModel{parent}
    {
        setRecursiveFilteringEnabled(true);
    }

    void setMatchingPages(std::set<Id> pages)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
        m_matchingPages = std::move(pages);
        endFilterChange(Direction::Rows);
#else
        m_matchingPages = std::move(pages);
        invalidateRowsFilter();
#endif
    }

    [[nodiscard]] bool matches(const SettingsPage* page) const
    {
        return m_matchingPages.contains(page->id());
    }

protected:
    [[nodiscard]] bool filterAcceptsRow(int row, const QModelIndex& parent) const override
    {
        const auto index     = sourceModel()->index(row, 0, parent);
        const auto* category = index.data(SettingsItem::Data).value<SettingsCategory*>();
        return category && std::ranges::any_of(category->pages, [this](const auto* page) { return matches(page); });
    }

private:
    std::set<Id> m_matchingPages;
};

class ScrollArea : public QScrollArea
{
    Q_OBJECT

public:
    explicit ScrollArea(QWidget* parent);

    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool event(QEvent* event) override;

private:
    [[nodiscard]] int scrollBarWidth() const;
};

ScrollArea::ScrollArea(QWidget* parent)
    : QScrollArea{parent}
{
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);
    setWidgetResizable(true);
    setAutoFillBackground(true);
}

QSize ScrollArea::minimumSizeHint() const
{
    const QWidget* child = widget();
    if(!child) {
        return {0, 0};
    }

    const int width = frameWidth() * 2;
    QSize minSize   = child->minimumSizeHint();

    minSize += QSize{width, width};
    minSize += QSize{scrollBarWidth(), 0};

    minSize.setWidth(std::min(minSize.width(), 250));
    minSize.setHeight(std::min(minSize.height(), 250));

    return minSize;
}

void ScrollArea::resizeEvent(QResizeEvent* event)
{
    QWidget* child = widget();
    if(!child) {
        QScrollArea::resizeEvent(event);
        return;
    }

    const QSize chilSizeHint = child->minimumSizeHint();
    const int width          = frameWidth() * 2;
    QSize innerSize          = event->size() - QSize{width, width};

    if(chilSizeHint.height() > innerSize.height()) {
        // Child widget is bigger
        innerSize.setWidth(innerSize.width() - scrollBarWidth());
        innerSize.setHeight(chilSizeHint.height());
    }

    // Resize to fit scroll area
    child->resize(innerSize);
    QScrollArea::resizeEvent(event);
}

bool ScrollArea::event(QEvent* event)
{
    if(event->type() == QEvent::LayoutRequest) {
        updateGeometry();
    }
    return QScrollArea::event(event);
}

int ScrollArea::scrollBarWidth() const
{
    auto* scrollArea = const_cast<ScrollArea*>(this); // NOLINT
    QWidgetList list = scrollArea->scrollBarWidgets(Qt::AlignRight);
    if(list.isEmpty()) {
        return 0;
    }
    return list.first()->sizeHint().width();
}

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
    return {maxWidth + verticalScrollBar()->sizeHint().width() + (2 * frameWidth()), 100};
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

class SimpleListWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit SimpleListWidget(QWidget* parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;

private:
    [[nodiscard]] int calculateMaxItemWidth(const QModelIndex& index) const;
};

SimpleListWidget::SimpleListWidget(QWidget* parent)
    : QListWidget{parent}
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

QSize SimpleListWidget::sizeHint() const
{
    const int maxWidth = calculateMaxItemWidth({});
    return {maxWidth + verticalScrollBar()->sizeHint().width() + (2 * frameWidth()) + PageListPadding, 100};
}

int SimpleListWidget::calculateMaxItemWidth(const QModelIndex& index) const
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
    , m_filterModel{new SettingsFilterModel(this)}
    , m_search{new QLineEdit(this)}
    , m_navigationLayout{new QVBoxLayout()}
    , m_pages{std::move(pages)}
    , m_searchActive{false}
{
    setWindowTitle(tr("Settings"));

    m_stackedLayout->setContentsMargins({});

    m_model->setPages(m_pages);
    m_filterModel->setSourceModel(m_model);

    std::set<Id> pageIds;
    for(const auto* page : m_pages) {
        pageIds.emplace(page->id());
    }
    m_filterModel->setMatchingPages(std::move(pageIds));

    m_search->setPlaceholderText(tr("Filter…"));
    m_search->setClearButtonEnabled(true);

    auto* clearSearchShortcut = new QShortcut(QKeySequence{Qt::Key_Escape}, m_search);
    clearSearchShortcut->setContext(Qt::WidgetShortcut);
    clearSearchShortcut->setEnabled(false);
    QObject::connect(clearSearchShortcut, &QShortcut::activated, m_search, &QLineEdit::clear);
    QObject::connect(m_search, &QLineEdit::textChanged, clearSearchShortcut,
                     [clearSearchShortcut](const QString& text) { clearSearchShortcut->setEnabled(!text.isEmpty()); });

    auto* emptyPage = new QLabel(tr("No matching settings."), this);
    emptyPage->setAlignment(Qt::AlignCenter);
    m_stackedLayout->addWidget(emptyPage);

    m_categoryTree->setHeaderHidden(true);
    m_categoryTree->setModel(m_filterModel);
    m_categoryTree->setFocus();

    m_buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);
    m_buttonBox->button(QDialogButtonBox::RestoreDefaults)->setText(tr("Reset Page"));
    m_buttonBox->button(QDialogButtonBox::Reset)->setText(tr("Reset All"));

    m_navigationLayout->addWidget(m_search);
    m_navigationLayout->addWidget(m_categoryTree);

    auto* mainGridLayout = new QGridLayout(this);
    mainGridLayout->addLayout(m_navigationLayout, 0, 0);
    mainGridLayout->addLayout(m_stackedLayout, 0, 1);
    mainGridLayout->addWidget(m_buttonBox, 1, 0, 1, 2);
    mainGridLayout->setColumnStretch(1, 4);
    mainGridLayout->setSizeConstraint(QLayout::SetMinimumSize);

    QObject::connect(m_categoryTree->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
                     &SettingsDialog::currentChanged);
    QObject::connect(m_search, &QLineEdit::textChanged, this, &SettingsDialog::filterPages);
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
    setWindowModality(Qt::NonModal);
    show();
}

void SettingsDialog::openPage(const Id& id)
{
    if(!id.isValid()) {
        qCWarning(SETTINGS) << "Invalid page id:" << id.name();
        return;
    }

    const auto* category = m_model->categoryForPage(id);
    if(!category) {
        qCWarning(SETTINGS) << "Page not found:" << id.name();
        return;
    }

    if(!m_filterModel->matches(findPage(id))) {
        m_search->clear();
    }

    const QModelIndex categoryIndex = m_filterModel->mapFromSource(m_model->indexForCategory(category->id));
    {
        const QSignalBlocker blocker{m_categoryTree->selectionModel()};
        m_categoryTree->setCurrentIndex(categoryIndex);
    }
    showCategory(categoryIndex);
    m_categoryTree->scrollTo(categoryIndex);

    const int pageIndex = category->findPageById(id);
    if(pageIndex < 0) {
        return;
    }

    if(category->pageList) {
        category->pageList->setCurrentRow(pageIndex);
    }
    else if(category->tabWidget) {
        category->tabWidget->setCurrentIndex(pageIndex);
    }
}

void SettingsDialog::setSearchBarPosition(SettingsDialogController::SearchBarPosition position)
{
    using SearchBarPosition = SettingsDialogController::SearchBarPosition;
    if(position == SearchBarPosition::Hidden) {
        m_search->clear();
        m_search->hide();
        return;
    }

    m_navigationLayout->removeWidget(m_search);
    m_navigationLayout->insertWidget(position == SearchBarPosition::Bottom ? 1 : 0, m_search);
    m_search->show();
}

Id SettingsDialog::currentPage() const
{
    return m_currentPage;
}

QByteArray SettingsDialog::saveState() const
{
    if(m_searchActive) {
        return m_stateBeforeSearch;
    }

    QByteArray stateData;
    QDataStream stream{&stateData, QIODeviceBase::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    const QStringList expandedCategories = Utils::saveExpansionState(m_categoryTree, categoryKey);
    stream << expandedCategories;

    return qCompress(stateData, 9);
}

void SettingsDialog::restoreState(const QByteArray& state)
{
    if(state.isEmpty()) {
        return;
    }

    QByteArray stateData = qUncompress(state);
    QDataStream stream{&stateData, QIODeviceBase::ReadOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    QStringList expandedCategories;
    stream >> expandedCategories;

    Utils::restoreExpansionState(m_categoryTree, expandedCategories, categoryKey);
}

void SettingsDialog::done(int value)
{
    QDialog::done(value);
}

void SettingsDialog::accept()
{
    if(!apply()) {
        return;
    }
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

QSize SettingsDialog::sizeHint() const
{
    return Utils::proportionateSize(this, 0.4, 0.55);
}

bool SettingsDialog::apply()
{
    for(const auto& page : m_visitedPages) {
        const QString error = page->validationError();
        if(!error.isEmpty()) {
            openPage(page->id());
            QMessageBox::warning(this, tr("Settings"), error);
            return false;
        }
    }

    for(const auto& page : m_visitedPages) {
        page->apply();
    }

    return true;
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
    updateCategoryPages(category);

    m_currentCategory = category->id;
    m_currentPage     = {};

    const int currentPageIndex = category->pageList ? category->pageList->currentRow()
                                                    : (category->tabWidget ? category->tabWidget->currentIndex() : -1);
    if(currentPageIndex != -1 && std::cmp_less(currentPageIndex, category->pages.size())) {
        auto* page = category->pages.at(currentPageIndex);
        page->setSearchFilter(m_search->text().trimmed());
        m_currentPage = page->id();
        m_visitedPages.emplace(page);
    }

    m_buttonBox->button(QDialogButtonBox::RestoreDefaults)->setEnabled(m_currentPage.isValid());
    m_stackedLayout->setCurrentIndex(category->index);

    setWindowTitle(tr("Settings") + QLatin1String(": ") + category->name);
}

void SettingsDialog::checkCategoryWidget(SettingsCategory* category)
{
    if(category->index >= 0) {
        return;
    }

    if(category->pages.empty()) {
        auto* placeholder = new QWidget();
        auto* layout      = new QVBoxLayout(placeholder);
        auto* label       = new QLabel(tr("Select a page from this category."), placeholder);

        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);

        category->index = m_stackedLayout->addWidget(placeholder);
        return;
    }

    if(category->pages.size() == 1) {
        auto* page = category->pages.front();
        if(const QWidget* widget = page->widget()) {
            if(auto* layout = widget->layout()) {
                layout->setContentsMargins({});
            }
        }
    }

    if(category->selector == SettingsPageSelector::List && category->pages.size() > 1) {
        auto* container = new QWidget();
        auto* pageList  = new SimpleListWidget(container);
        auto* pageStack = new QStackedWidget(container);
        auto* layout    = new QHBoxLayout(container);

        layout->setContentsMargins({});
        layout->addWidget(pageList);
        layout->addWidget(pageStack, 1);

        for(const auto& page : category->pages) {
            if(QWidget* widget = page->widget()) {
                auto* scrollArea = new ScrollArea(pageStack);
                scrollArea->setWidget(widget);
                pageList->addItem(page->name());
                pageStack->addWidget(scrollArea);

                page->load();
            }
        }

        pageList->setCurrentRow(0);
        pageStack->setCurrentIndex(0);

        QObject::connect(pageList, &QListWidget::currentRowChanged, pageStack, &QStackedWidget::setCurrentIndex);
        QObject::connect(pageList, &QListWidget::currentRowChanged, this, &SettingsDialog::currentPageChanged);

        category->pageList = pageList;
        category->index    = m_stackedLayout->addWidget(container);
        return;
    }

    auto* tabWidget = new QTabWidget();
    tabWidget->setTabBarAutoHide(true);
    tabWidget->setDocumentMode(category->pages.size() == 1);

    for(const auto& page : category->pages) {
        if(QWidget* widget = page->widget()) {
            auto* scrollArea = new ScrollArea(tabWidget);
            scrollArea->setWidget(widget);
            tabWidget->addTab(scrollArea, page->name());

            page->load();
        }
    }

    QObject::connect(tabWidget, &QTabWidget::currentChanged, this, &SettingsDialog::currentPageChanged);

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
        m_currentCategory = {};
        m_currentPage     = {};
        m_buttonBox->button(QDialogButtonBox::RestoreDefaults)->setDisabled(true);
        setWindowTitle(tr("Settings"));
    }
}

void SettingsDialog::currentPageChanged(int index)
{
    if(index < 0) {
        m_currentPage = {};
        m_buttonBox->button(QDialogButtonBox::RestoreDefaults)->setDisabled(true);
        return;
    }

    const QModelIndex modelIndex = m_categoryTree->currentIndex();
    if(!modelIndex.isValid()) {
        return;
    }

    auto* category = modelIndex.data(SettingsItem::Data).value<SettingsCategory*>();
    if(!category || std::cmp_greater_equal(index, category->pages.size())) {
        m_currentPage = {};
        m_buttonBox->button(QDialogButtonBox::RestoreDefaults)->setDisabled(true);
        return;
    }

    SettingsPage* page = category->pages.at(index);
    page->setSearchFilter(m_search->text().trimmed());
    m_currentPage = page->id();
    m_visitedPages.emplace(page);
    m_buttonBox->button(QDialogButtonBox::RestoreDefaults)->setEnabled(true);
}

void SettingsDialog::filterPages(const QString& text)
{
    const QString query = text.trimmed();
    if(!m_searchActive && !query.isEmpty()) {
        m_pageBeforeSearch     = m_currentPage;
        m_categoryBeforeSearch = m_currentCategory;
        m_stateBeforeSearch    = saveState();
        m_searchActive         = true;
    }

    std::set<Id> matchingPages;
    for(auto* page : m_pages) {
        if(page->matches(query)) {
            matchingPages.emplace(page->id());
        }
        page->setSearchFilter(query);
    }

    {
        const QSignalBlocker blocker{m_categoryTree->selectionModel()};
        m_filterModel->setMatchingPages(std::move(matchingPages));
    }

    if(m_searchActive && query.isEmpty()) {
        m_searchActive = false;
        m_categoryTree->collapseAll();
        restoreState(m_stateBeforeSearch);

        if(m_pageBeforeSearch.isValid()) {
            openPage(m_pageBeforeSearch);
            return;
        }
        if(m_categoryBeforeSearch.isValid()) {
            const auto index = m_filterModel->mapFromSource(m_model->indexForCategory(m_categoryBeforeSearch));
            m_categoryTree->setCurrentIndex(index);
            currentChanged(index);
            return;
        }
    }
    else if(!query.isEmpty()) {
        m_categoryTree->expandAll();
    }

    QModelIndex current{m_categoryTree->currentIndex()};
    if(!current.isValid()) {
        current = m_filterModel->index(0, 0);
    }

    while(current.isValid()) {
        const auto* category = current.data(SettingsItem::Data).value<SettingsCategory*>();
        if(category
           && std::ranges::any_of(category->pages, [this](const auto* page) { return m_filterModel->matches(page); })) {
            break;
        }
        current = m_filterModel->index(0, 0, current);
    }

    {
        const QSignalBlocker blocker{m_categoryTree->selectionModel()};
        m_categoryTree->setCurrentIndex(current);
    }

    currentChanged(current);
}

void SettingsDialog::updateCategoryPages(SettingsCategory* category)
{
    auto* selector = category->pageList ? category->pageList : static_cast<QObject*>(category->tabWidget);
    if(!selector) {
        return;
    }

    const QSignalBlocker blocker{selector};

    int firstMatch{-1};
    for(int row{0}; std::cmp_less(row, category->pages.size()); ++row) {
        const bool visible = m_filterModel->matches(category->pages.at(row));
        if(category->pageList) {
            if(auto* item = category->pageList->item(row)) {
                item->setHidden(!visible);
            }
        }
        else {
            category->tabWidget->setTabVisible(row, visible);
        }

        if(visible && firstMatch < 0) {
            firstMatch = row;
        }
    }

    const int current = category->pageList ? category->pageList->currentRow() : category->tabWidget->currentIndex();
    if(current < 0 || std::cmp_greater_equal(current, category->pages.size())
       || !m_filterModel->matches(category->pages.at(current))) {
        if(category->pageList) {
            category->pageList->setCurrentRow(firstMatch);
        }
        else {
            category->tabWidget->setCurrentIndex(firstMatch);
        }
    }

    if(category->pageList) {
        if(const auto* container = m_stackedLayout->widget(category->index)) {
            if(auto* stack = container->findChild<QStackedWidget*>()) {
                stack->setCurrentIndex(category->pageList->currentRow());
            }
        }
    }
}

SettingsPage* SettingsDialog::findPage(const Id& id)
{
    const auto it
        = std::ranges::find_if(std::as_const(m_pages), [&id](SettingsPage* page) { return page->id() == id; });
    if(it != m_pages.cend()) {
        return *it;
    }
    return nullptr;
}

QString SettingsDialog::categoryKey(const QModelIndex& index)
{
    if(!index.isValid()) {
        return {};
    }

    const auto* category = index.data(SettingsItem::Data).value<SettingsCategory*>();
    return category ? category->id.name() : QString{};
}

} // namespace Fooyin

#include "moc_settingsdialog.cpp"
#include "settingsdialog.moc"
