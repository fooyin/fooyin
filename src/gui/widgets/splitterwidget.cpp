/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "splitterwidget.h"

#include "internalguisettings.h"

#include <gui/widgetprovider.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/enum.h>
#include <utils/settings/settingsmanager.h>

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>
#include <QSplitter>

namespace {
Fooyin::ActionContainer* createNewMenu(Fooyin::ActionManager* actionManager, Fooyin::FyWidget* parent,
                                       const QString& title)
{
    auto id       = parent->id().append(title);
    auto* newMenu = actionManager->createMenu(id);
    newMenu->menu()->setTitle(title);
    return newMenu;
}
} // namespace

namespace Fooyin {
class SplitterHandle : public QSplitterHandle
{
    Q_OBJECT

public:
    explicit SplitterHandle(Qt::Orientation type, QSplitter* parent = nullptr)
        : QSplitterHandle{type, parent}
    { }

    void showHandle(bool show)
    {
        m_showHandle = show;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        if(m_showHandle) {
            QSplitterHandle::paintEvent(event);
        }
    };

private:
    bool m_showHandle{true};
};

class Splitter : public QSplitter
{
    Q_OBJECT

public:
    explicit Splitter(Qt::Orientation type, SettingsManager* settings, QWidget* parent = nullptr)
        : QSplitter{type, parent}
        , m_settings{settings}
    {
        setObjectName(QStringLiteral("Splitter"));
        setChildrenCollapsible(false);
    }

protected:
    QSplitterHandle* createHandle() override
    {
        auto* handle = new SplitterHandle(orientation(), this);
        handle->showHandle(m_settings->value<Settings::Gui::Internal::SplitterHandles>());
        m_settings->subscribe<Settings::Gui::Internal::SplitterHandles>(handle, &SplitterHandle::showHandle);
        return handle;
    };

private:
    SettingsManager* m_settings;
};

SplitterWidget::SplitterWidget(ActionManager* actionManager, WidgetProvider* widgetProvider, SettingsManager* settings,
                               QWidget* parent)
    : WidgetContainer{widgetProvider, parent}
    , m_actionManager{actionManager}
    , m_widgetProvider{widgetProvider}
    , m_splitter{new Splitter(Qt::Vertical, settings, this)}
    , m_limit{0}
    , m_showDummy{false}
    , m_widgetCount{0}
    , m_baseWidgetCount{0}
{
    setObjectName(SplitterWidget::name());

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_splitter);

    checkShowDummy();
}

void SplitterWidget::setWidgetLimit(int count)
{
    m_limit = count;
}

void SplitterWidget::showPlaceholder(bool show)
{
    m_showDummy = show;

    checkShowDummy();
}

SplitterWidget::~SplitterWidget() = default;

Qt::Orientation SplitterWidget::orientation() const
{
    return m_splitter->orientation();
}

void SplitterWidget::setOrientation(Qt::Orientation orientation)
{
    m_splitter->setOrientation(orientation);
    setObjectName(SplitterWidget::name());
}

QByteArray SplitterWidget::saveState() const
{
    return m_splitter->saveState();
}

bool SplitterWidget::restoreState(const QByteArray& state)
{
    return m_splitter->restoreState(state);
}

bool SplitterWidget::canAddWidget() const
{
    return true;
}

int SplitterWidget::widgetIndex(const Id& id) const
{
    if(!id.isValid()) {
        return -1;
    }

    const auto widgetIt = std::ranges::find_if(m_children, [id](FyWidget* widget) { return widget->id() == id; });
    if(widgetIt != m_children.cend()) {
        return static_cast<int>(std::distance(m_children.cbegin(), widgetIt));
    }

    return -1;
}

FyWidget* SplitterWidget::widgetAtId(const Id& id) const
{
    if(!id.isValid()) {
        return nullptr;
    }

    const auto widgetIt = std::ranges::find_if(m_children, [id](FyWidget* widget) { return widget->id() == id; });
    if(widgetIt != m_children.cend()) {
        return *widgetIt;
    }

    return nullptr;
}

FyWidget* SplitterWidget::widgetAtIndex(int index) const
{
    if(index < 0 || std::cmp_greater_equal(index, m_children.size())) {
        return nullptr;
    }

    return m_children.at(index);
}

WidgetList SplitterWidget::widgets() const
{
    return m_children;
}

int SplitterWidget::addWidget(FyWidget* widget)
{
    const int index = static_cast<int>(m_children.size()) + m_baseWidgetCount;
    insertWidget(index, widget);
    return index;
}

void SplitterWidget::insertWidget(int index, FyWidget* widget)
{
    if(m_limit > 0 && m_widgetCount >= m_limit) {
        return;
    }

    if(!widget) {
        return;
    }

    if(index < 0 || index > m_splitter->count()) {
        return;
    }

    m_widgetCount += 1;

    if(m_dummy && m_widgetCount > 0) {
        m_dummy->deleteLater();
    }

    m_children.insert(m_children.begin() + index, widget);
    m_splitter->insertWidget(index, widget);
}

void SplitterWidget::removeWidget(int index)
{
    if(index < 0 || std::cmp_greater_equal(index, m_children.size())) {
        return;
    }

    m_widgetCount -= 1;

    m_children.at(index)->deleteLater();
    m_children.erase(m_children.begin() + index);

    checkShowDummy();
}

void SplitterWidget::replaceWidget(int index, FyWidget* newWidget)
{
    if(index < 0 || std::cmp_greater_equal(index, m_children.size())) {
        return;
    }

    auto* replacedWidget = m_splitter->replaceWidget(index, newWidget);
    m_children.erase(m_children.begin() + index);
    replacedWidget->deleteLater();

    m_children.insert(m_children.begin() + index, newWidget);
}

QString SplitterWidget::name() const
{
    return Utils::Enum::toString(m_splitter->orientation()) + QStringLiteral(" Splitter");
}

QString SplitterWidget::layoutName() const
{
    return QStringLiteral("Splitter") + Utils::Enum::toString(m_splitter->orientation());
}

void SplitterWidget::layoutEditingMenu(ActionContainer* menu)
{
    auto* changeSplitter = new QAction(tr("Change Splitter"), this);
    QObject::connect(changeSplitter, &QAction::triggered, this, [this] {
        setOrientation(m_splitter->orientation() == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal);
        setObjectName(Utils::Enum::toString(m_splitter->orientation()) + QStringLiteral(" Splitter"));
    });
    menu->addAction(changeSplitter);

    if(m_limit > 0 && m_widgetCount >= m_limit) {
        return;
    }

    auto* addMenu = createNewMenu(m_actionManager, this, tr("&Add"));

    m_widgetProvider->setupAddWidgetMenu(addMenu, this);

    menu->addMenu(addMenu);
}

void SplitterWidget::saveLayoutData(QJsonObject& layout)
{
    layout[QStringLiteral("State")] = QString::fromUtf8(saveState().toBase64());

    if(!m_children.empty()) {
        QJsonArray children;
        for(const auto& widget : m_children) {
            widget->saveLayout(children);
        }
        layout[QStringLiteral("Widgets")] = children;
    }
}

void SplitterWidget::loadLayoutData(const QJsonObject& layout)
{
    const auto state    = QByteArray::fromBase64(layout[QStringLiteral("State")].toString().toUtf8());
    const auto children = layout[QStringLiteral("Widgets")].toArray();

    WidgetContainer::loadWidgets(children);

    restoreState(state);

    checkShowDummy();
}

void SplitterWidget::checkShowDummy()
{
    if(!m_dummy && m_showDummy && m_widgetCount == 0) {
        m_dummy = new Dummy(this);
        m_splitter->addWidget(m_dummy);
    }
    else if(m_dummy && !m_showDummy && m_widgetCount > 0) {
        m_dummy->deleteLater();
    }
}
} // namespace Fooyin

#include "moc_splitterwidget.cpp"
#include "splitterwidget.moc"
