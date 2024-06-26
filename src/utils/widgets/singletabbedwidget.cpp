/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <utils/widgets/singletabbedwidget.h>

#include <utils/widgets/editabletabbar.h>

#include <QApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QPointer>
#include <QScreen>
#include <QStackedWidget>
#include <QStyleOption>
#include <QStylePainter>
#include <QVBoxLayout>

namespace {
QSize basicSize(const QSize& lc, const QSize& rc, const QSize& widgetSize, const QSize& tabSize)
{
    return {std::max(widgetSize.width(), tabSize.width() + rc.width() + lc.width()),
            widgetSize.height() + (std::max(rc.height(), std::max(lc.height(), tabSize.height())))};
}

void initStyleBaseOption(QStyleOptionTabBarBase* optTabBase, QTabBar* tabBar, QSize size)
{
    QStyleOptionTab tabOverlap;

    tabOverlap.shape  = tabBar->shape();
    const int overlap = tabBar->style()->pixelMetric(QStyle::PM_TabBarBaseOverlap, &tabOverlap, tabBar);
    QWidget* parent   = tabBar->parentWidget();

    optTabBase->initFrom(tabBar);
    optTabBase->shape        = tabBar->shape();
    optTabBase->documentMode = tabBar->documentMode();

    if(parent && overlap > 0) {
        QRect rect;
        switch(tabOverlap.shape) {
            case(QTabBar::RoundedNorth):
            case(QTabBar::TriangularNorth):
                rect.setRect(0, size.height() - overlap, size.width(), overlap);
                break;
            default:
                break;
        }
        optTabBase->rect = rect;
    }
}
} // namespace

namespace Fooyin {
struct SingleTabbedWidget::Private
{
    SingleTabbedWidget* m_self;

    EditableTabBar* m_tabBar;
    TabShape m_shape{TabShape::Rounded};
    QRect m_panelRect;
    QPointer<QWidget> m_widget;
    QWidget* m_leftCornerWidget{nullptr};
    QWidget* m_rightCornerWidget{nullptr};
    bool m_dirty{false};

    explicit Private(SingleTabbedWidget* self)
        : m_self{self}
        , m_tabBar{new EditableTabBar(self)}
    {
        m_tabBar->setDrawBase(false);
    }

    [[nodiscard]] bool isAutoHidden() const
    {
        return (m_tabBar->autoHide() && m_tabBar->count() <= 1);
    }

    void initBasicStyleOption(QStyleOptionTabWidgetFrame* option) const
    {
        option->initFrom(m_self);

        if(m_self->documentMode()) {
            option->lineWidth = 0;
        }
        else {
            option->lineWidth = m_self->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, m_self);
        }

        option->shape = m_shape == TabShape::Rounded ? QTabBar::RoundedNorth : QTabBar::TriangularNorth;

        option->tabBarRect = m_tabBar->geometry();
    }

    void initStyleOption(QStyleOptionTabWidgetFrame* option) const
    {
        if(!option) {
            return;
        }

        initBasicStyleOption(option);

        const int baseHeight = m_self->style()->pixelMetric(QStyle::PM_TabBarBaseHeight, nullptr, m_self);
        QSize tabsSize{0, m_widget ? m_widget->width() : 0};

        if(m_tabBar->isVisibleTo(const_cast<SingleTabbedWidget*>(m_self))) {
            tabsSize = m_tabBar->sizeHint();
            if(m_self->documentMode()) {
                tabsSize.setWidth(m_self->width());
            }
        }

        if(m_rightCornerWidget) {
            const QSize rightCornerSizeHint = m_rightCornerWidget->sizeHint();
            const QSize bounds(rightCornerSizeHint.width(), tabsSize.height() - baseHeight);
            option->rightCornerWidgetSize = rightCornerSizeHint.boundedTo(bounds);
        }
        else {
            option->rightCornerWidgetSize = {0, 0};
        }

        if(m_leftCornerWidget) {
            const QSize leftCornerSizeHint = m_leftCornerWidget->sizeHint();
            const QSize bounds(leftCornerSizeHint.width(), tabsSize.height() - baseHeight);
            option->leftCornerWidgetSize = leftCornerSizeHint.boundedTo(bounds);
        }
        else {
            option->leftCornerWidgetSize = {0, 0};
        }

        option->tabBarSize = tabsSize;

        QRect selectedTabRect = m_tabBar->tabRect(m_tabBar->currentIndex());
        selectedTabRect.moveTopLeft(selectedTabRect.topLeft() + option->tabBarRect.topLeft());
        option->selectedTabRect = selectedTabRect;
    }

    void setupLayout(bool onlyCheck = false)
    {
        if(onlyCheck && !m_dirty) {
            return;
        }

        if(!m_self->isVisible()) {
            QStyleOptionTabWidgetFrame basicOption;
            initBasicStyleOption(&basicOption);
            m_dirty = true;
            return;
        }

        QStyleOptionTabWidgetFrame option;
        initStyleOption(&option);

        auto* style                 = m_self->style();
        m_panelRect                 = style->subElementRect(QStyle::SE_TabWidgetTabPane, &option, m_self);
        const QRect tabRect         = style->subElementRect(QStyle::SE_TabWidgetTabBar, &option, m_self);
        const QRect contentsRect    = style->subElementRect(QStyle::SE_TabWidgetTabContents, &option, m_self);
        const QRect leftCornerRect  = style->subElementRect(QStyle::SE_TabWidgetLeftCorner, &option, m_self);
        const QRect rightCornerRect = style->subElementRect(QStyle::SE_TabWidgetRightCorner, &option, m_self);

        m_tabBar->setGeometry(tabRect);
        if(m_widget) {
            m_widget->setGeometry(contentsRect);
        }
        if(m_leftCornerWidget) {
            m_leftCornerWidget->setGeometry(leftCornerRect);
        }
        if(m_rightCornerWidget) {
            m_rightCornerWidget->setGeometry(rightCornerRect);
        }

        if(!onlyCheck) {
            m_self->update();
        }

        m_self->updateGeometry();
    }
};

SingleTabbedWidget::SingleTabbedWidget(QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<Private>(this)}
{
    setFocusProxy(p->m_tabBar);

    setSizePolicy({QSizePolicy::Expanding, QSizePolicy::Expanding, QSizePolicy::TabWidget});

    QObject::connect(p->m_tabBar, &QTabBar::currentChanged, this, &SingleTabbedWidget::currentChanged);
    QObject::connect(p->m_tabBar, &QTabBar::tabMoved, this, &SingleTabbedWidget::tabMoved);
    QObject::connect(p->m_tabBar, &QTabBar::tabBarClicked, this, &SingleTabbedWidget::tabBarClicked);
    QObject::connect(p->m_tabBar, &QTabBar::tabBarDoubleClicked, this, &SingleTabbedWidget::tabBarDoubleClicked);

    if(p->m_tabBar->tabsClosable()) {
        QObject::connect(p->m_tabBar, &QTabBar::tabCloseRequested, this, &SingleTabbedWidget::tabCloseRequested);
    }

    p->setupLayout();
}

void SingleTabbedWidget::setWidget(QWidget* widget)
{
    if(p->m_widget) {
        p->m_widget->deleteLater();
    }

    if(widget) {
        p->m_widget = widget;
        p->m_widget->setParent(this);
        p->m_widget->setAttribute(Qt::WA_LaidOut);
        if(isVisible()) {
            QMetaObject::invokeMethod(p->m_widget, "_q_showIfNotHidden", Qt::QueuedConnection); // show later
        }
    }

    p->setupLayout();
}

int SingleTabbedWidget::addTab(const QString& label)
{
    return insertTab(-1, label);
}

int SingleTabbedWidget::addTab(const QIcon& icon, const QString& label)
{
    return insertTab(-1, icon, label);
}

int SingleTabbedWidget::insertTab(int index, const QString& label)
{
    return insertTab(index, {}, label);
}

int SingleTabbedWidget::insertTab(int index, const QIcon& icon, const QString& label)
{
    index = p->m_tabBar->insertTab(index, icon, label);
    p->setupLayout();
    return index;
}

void SingleTabbedWidget::removeTab(int index)
{
    p->m_tabBar->removeTab(index);
    p->setupLayout();
}

bool SingleTabbedWidget::isTabEnabled(int index) const
{
    return p->m_tabBar->isTabEnabled(index);
}

void SingleTabbedWidget::setTabEnabled(int index, bool enabled)
{
    p->m_tabBar->setTabEnabled(index, enabled);
}

bool SingleTabbedWidget::isTabVisible(int index) const
{
    return p->m_tabBar->isTabVisible(index);
}

void SingleTabbedWidget::setTabVisible(int index, bool visible)
{
    const bool currentIsVisible = p->m_tabBar->isTabVisible(p->m_tabBar->currentIndex());
    p->m_tabBar->setTabVisible(index, visible);

    if(visible && !currentIsVisible) {
        setCurrentIndex(index);
    }

    p->setupLayout();
}

QString SingleTabbedWidget::tabText(int index) const
{
    return p->m_tabBar->tabText(index);
}

void SingleTabbedWidget::setTabText(int index, const QString& text)
{
    p->m_tabBar->setTabText(index, text);
    p->setupLayout();
}

QIcon SingleTabbedWidget::tabIcon(int index) const
{
    return p->m_tabBar->tabIcon(index);
}

void SingleTabbedWidget::setTabIcon(int index, const QIcon& icon)
{
    p->m_tabBar->setTabIcon(index, icon);
    p->setupLayout();
}

int SingleTabbedWidget::currentIndex() const
{
    return p->m_tabBar->currentIndex();
}

QWidget* SingleTabbedWidget::currentWidget() const
{
    return p->m_widget;
}

int SingleTabbedWidget::count() const
{
    return p->m_tabBar->count();
}

bool SingleTabbedWidget::tabsClosable() const
{
    return p->m_tabBar->tabsClosable();
}

void SingleTabbedWidget::setTabsClosable(bool closeable)
{
    if(tabsClosable() == closeable) {
        return;
    }

    p->m_tabBar->setTabsClosable(closeable);

    if(closeable) {
        QObject::connect(p->m_tabBar, &QTabBar::tabCloseRequested, this, &SingleTabbedWidget::tabCloseRequested);
    }
    else {
        QObject::disconnect(p->m_tabBar, &QTabBar::tabCloseRequested, this, &SingleTabbedWidget::tabCloseRequested);
    }

    p->setupLayout();
}

bool SingleTabbedWidget::isMovable() const
{
    return p->m_tabBar->isMovable();
}

void SingleTabbedWidget::setMovable(bool movable)
{
    p->m_tabBar->setMovable(movable);
}

SingleTabbedWidget::TabShape SingleTabbedWidget::tabShape() const
{
    return p->m_shape;
}

void SingleTabbedWidget::setTabShape(TabShape shape)
{
    if(std::exchange(p->m_shape, shape) != shape) {
        p->m_tabBar->setShape(shape == TabShape::Rounded ? QTabBar::RoundedNorth : QTabBar::TriangularNorth);
        p->setupLayout();
    }
}

QSize SingleTabbedWidget::sizeHint() const
{
    QSize lc{0, 0};
    QSize rc{0, 0};
    QStyleOptionTabWidgetFrame opt;
    p->initStyleOption(&opt);
    opt.state = QStyle::State_None;

    if(p->m_leftCornerWidget) {
        lc = p->m_leftCornerWidget->sizeHint();
    }
    if(p->m_rightCornerWidget) {
        rc = p->m_rightCornerWidget->sizeHint();
    }
    if(!p->m_dirty) {
        p->setupLayout(true);
    }

    QSize widgetSize;
    if(p->m_widget) {
        widgetSize = p->m_widget->sizeHint();
    }

    QSize tabSize;
    if(!p->isAutoHidden()) {
        tabSize = p->m_tabBar->sizeHint();
        if(usesScrollButtons()) {
            tabSize = tabSize.boundedTo({200, 200});
        }
        else {
            tabSize = tabSize.boundedTo(QApplication::primaryScreen()->virtualGeometry().size());
        }
    }

    const QSize size = basicSize(lc, rc, widgetSize, tabSize);

    return style()->sizeFromContents(QStyle::CT_TabWidget, &opt, size, this);
}

QSize SingleTabbedWidget::minimumSizeHint() const
{
    QSize lc{0, 0};
    QSize rc{0, 0};

    if(p->m_leftCornerWidget) {
        lc = p->m_leftCornerWidget->minimumSizeHint();
    }
    if(p->m_rightCornerWidget) {
        rc = p->m_rightCornerWidget->minimumSizeHint();
    }
    if(!p->m_dirty) {
        p->setupLayout(true);
    }

    QSize widgetSize;
    if(p->m_widget) {
        widgetSize = p->m_widget->minimumSizeHint();
    }

    QSize tabSize;
    if(!p->isAutoHidden()) {
        tabSize = p->m_tabBar->minimumSizeHint();
    }

    const QSize size = basicSize(lc, rc, widgetSize, tabSize);

    QStyleOptionTabWidgetFrame opt;
    p->initStyleOption(&opt);
    opt.palette = palette();
    opt.state   = QStyle::State_None;
    return style()->sizeFromContents(QStyle::CT_TabWidget, &opt, size, this);
}

int SingleTabbedWidget::heightForWidth(int width) const
{
    QStyleOptionTabWidgetFrame opt;
    p->initStyleOption(&opt);
    opt.state = QStyle::State_None;

    const QSize zero{0, 0};
    const QSize padding = style()->sizeFromContents(QStyle::CT_TabWidget, &opt, zero, this);

    QSize lc{0, 0};
    QSize rc{0, 0};
    if(p->m_leftCornerWidget) {
        lc = p->m_leftCornerWidget->sizeHint();
    }
    if(p->m_rightCornerWidget) {
        rc = p->m_rightCornerWidget->sizeHint();
    }
    if(!p->m_dirty) {
        p->setupLayout(true);
    }

    QSize tabSize;
    if(!p->isAutoHidden()) {
        tabSize = p->m_tabBar->sizeHint();
        if(usesScrollButtons()) {
            tabSize = tabSize.boundedTo({200, 200});
        }
        else {
            tabSize = tabSize.boundedTo(QGuiApplication::primaryScreen()->virtualSize());
        }
    }

    const int contentsWidth = width - padding.width();
    const int widgetWidth   = contentsWidth;
    const int widgetHeight  = p->m_widget ? p->m_widget->heightForWidth(widgetWidth) : 0;
    const QSize size{widgetWidth, widgetHeight};
    const QSize contentSize = basicSize(lc, rc, size, tabSize);

    return (contentSize + padding).height();
}

bool SingleTabbedWidget::hasHeightForWidth() const
{
    if(p->m_widget) {
        return p->m_widget->hasHeightForWidth();
    }
    return true;
}

QWidget* SingleTabbedWidget::cornerWidget(Qt::Corner corner) const
{
    if(corner & Qt::TopRightCorner) {
        return p->m_rightCornerWidget;
    }
    return p->m_leftCornerWidget;
}

void SingleTabbedWidget::setCornerWidget(QWidget* widget, Qt::Corner corner)
{
    if(widget && widget->parentWidget() != this) {
        widget->setParent(this);
        widget->show();
    }

    if(corner & Qt::TopRightCorner) {
        if(p->m_rightCornerWidget) {
            delete p->m_rightCornerWidget;
            p->m_rightCornerWidget = nullptr;
        }
        p->m_rightCornerWidget = widget;
    }
    else {
        if(p->m_leftCornerWidget) {
            delete p->m_leftCornerWidget;
            p->m_leftCornerWidget = nullptr;
        }
        p->m_leftCornerWidget = widget;
    }

    p->setupLayout();
}

Qt::TextElideMode SingleTabbedWidget::elideMode() const
{
    return p->m_tabBar->elideMode();
}

void SingleTabbedWidget::setElideMode(Qt::TextElideMode mode)
{
    p->m_tabBar->setElideMode(mode);
}

QSize SingleTabbedWidget::iconSize() const
{
    return p->m_tabBar->iconSize();
}

void SingleTabbedWidget::setIconSize(const QSize& size)
{
    p->m_tabBar->setIconSize(size);
}

bool SingleTabbedWidget::usesScrollButtons() const
{
    return p->m_tabBar->usesScrollButtons();
}

void SingleTabbedWidget::setUsesScrollButtons(bool useButtons)
{
    p->m_tabBar->setUsesScrollButtons(useButtons);
}

bool SingleTabbedWidget::documentMode() const
{
    return p->m_tabBar->documentMode();
}

void SingleTabbedWidget::setDocumentMode(bool enabled)
{
    p->m_tabBar->setDocumentMode(enabled);
    p->m_tabBar->setDrawBase(enabled);
    p->setupLayout();
}

bool SingleTabbedWidget::tabBarAutoHide() const
{
    return p->m_tabBar->autoHide();
}

void SingleTabbedWidget::setTabBarAutoHide(bool enabled)
{
    p->m_tabBar->setAutoHide(enabled);
}

void SingleTabbedWidget::clear()
{
    while(count()) {
        removeTab(0);
    }
}

EditableTabBar* SingleTabbedWidget::tabBar() const
{
    return p->m_tabBar;
}

void SingleTabbedWidget::setCurrentIndex(int index)
{
    p->m_tabBar->setCurrentIndex(index);
}

void SingleTabbedWidget::showEvent(QShowEvent* /*event*/)
{
    p->setupLayout();
}

void SingleTabbedWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    p->setupLayout();
}

void SingleTabbedWidget::keyPressEvent(QKeyEvent* event)
{
    if(((event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab) && count() > 1
        && event->modifiers() & Qt::ControlModifier)) {
        const int tabCount = count();
        const int dx       = (event->key() == Qt::Key_Backtab || event->modifiers() & Qt::ShiftModifier) ? -1 : 1;
        int tab            = currentIndex();
        for(int pass{0}; pass < tabCount; ++pass) {
            tab += dx;
            if(tab < 0) {
                tab = tabCount - 1;
            }
            else if(tab >= tabCount) {
                tab = 0;
            }
            if(p->m_tabBar->isTabEnabled(tab) && p->m_tabBar->isTabVisible(tab)) {
                setCurrentIndex(tab);
                break;
            }
        }
        if(!QApplication::focusWidget()) {
            p->m_tabBar->setFocus();
        }
    }
    else {
        event->ignore();
    }
}

void SingleTabbedWidget::paintEvent(QPaintEvent* /*event*/)
{
    if(documentMode()) {
        QStylePainter painter{this};
        if(auto* w = cornerWidget(Qt::TopLeftCorner)) {
            QStyleOptionTabBarBase opt;
            initStyleBaseOption(&opt, tabBar(), w->size());
            opt.rect.moveLeft(w->x() + opt.rect.x());
            opt.rect.moveTop(w->y() + opt.rect.y());
            painter.drawPrimitive(QStyle::PE_FrameTabBarBase, opt);
            // painter.setPen(QPen(opt.palette.light(), 0));
            // painter.drawLine(opt.rect.topLeft(), opt.rect.topRight());
        }
        if(auto* w = cornerWidget(Qt::TopRightCorner)) {
            QStyleOptionTabBarBase opt;
            initStyleBaseOption(&opt, tabBar(), w->size());
            opt.rect.moveLeft(w->x() + opt.rect.x());
            opt.rect.moveTop(w->y() + opt.rect.y());
            painter.drawPrimitive(QStyle::PE_FrameTabBarBase, opt);
        }
        return;
    }

    QStylePainter painter{this};
    QStyleOptionTabWidgetFrame opt;
    p->initStyleOption(&opt);
    opt.rect = p->m_panelRect;
    painter.drawPrimitive(QStyle::PE_FrameTabWidget, opt);
}

void SingleTabbedWidget::changeEvent(QEvent* event)
{
    if(event->type() == QEvent::StyleChange) {
        p->setupLayout();
    }

    QWidget::changeEvent(event);
}

bool SingleTabbedWidget::event(QEvent* event)
{
    if(event->type() == QEvent::LayoutRequest) {
        p->setupLayout();
    }

    return QWidget::event(event);
}
} // namespace Fooyin

#include "utils/widgets/moc_singletabbedwidget.cpp"
