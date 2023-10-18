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

#include <gui/splitterwidget.h>

#include "dummy.h"

#include <gui/guisettings.h>
#include <gui/widgetfactory.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/enumhelper.h>
#include <utils/settings/settingsmanager.h>

#include <QHBoxLayout>
#include <QJsonObject>
#include <QMenu>
#include <QSplitter>

namespace {
Fy::Utils::ActionContainer* createNewMenu(Fy::Utils::ActionManager* actionManager, Fy::Gui::Widgets::FyWidget* parent,
                                          const QString& title)
{
    auto id       = parent->id().append(title);
    auto* newMenu = actionManager->createMenu(id);
    newMenu->menu()->setTitle(title);
    return newMenu;
}
} // namespace

namespace Fy::Gui::Widgets {
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
            return QSplitterHandle::paintEvent(event);
        }
    };

private:
    bool m_showHandle{true};
};

class Splitter : public QSplitter
{
    Q_OBJECT

public:
    explicit Splitter(Qt::Orientation type, Utils::SettingsManager* settings, QWidget* parent = nullptr)
        : QSplitter{type, parent}
        , m_settings{settings}
    {
        setObjectName("Splitter");
        setChildrenCollapsible(false);
    }

protected:
    QSplitterHandle* createHandle() override
    {
        auto* handle = new SplitterHandle(orientation(), this);
        handle->showHandle(m_settings->value<Settings::SplitterHandles>());
        m_settings->subscribe<Settings::SplitterHandles>(handle, &SplitterHandle::showHandle);
        return handle;
    };

private:
    Utils::SettingsManager* m_settings;
};

struct SplitterWidget::Private
{
    SplitterWidget* self;

    Utils::SettingsManager* settings;
    Utils::ActionManager* actionManager;
    Widgets::WidgetFactory* widgetFactory;

    Splitter* splitter;
    QList<FyWidget*> children;
    Dummy* dummy;

    int limit{0};
    bool showDummy{true};
    int widgetCount{0};

    Private(SplitterWidget* self, Utils::ActionManager* actionManager, Widgets::WidgetFactory* widgetFactory,
            Utils::SettingsManager* settings)
        : self{self}
        , settings{settings}
        , actionManager{actionManager}
        , widgetFactory{widgetFactory}
        , splitter{new Splitter(Qt::Vertical, settings, self)}
        , dummy{nullptr}
    { }

    void checkShowDummy()
    {
        if(!dummy && showDummy && widgetCount == 0) {
            dummy = new Dummy(self);
            splitter->addWidget(dummy);
        }
    }
};

SplitterWidget::SplitterWidget(Utils::ActionManager* actionManager, Widgets::WidgetFactory* widgetFactory,
                               Utils::SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, actionManager, widgetFactory, settings)}
{
    setObjectName(SplitterWidget::name());

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(p->splitter);

    p->checkShowDummy();
}

void SplitterWidget::setWidgetLimit(int count)
{
    p->limit = count;
}

void SplitterWidget::showPlaceholder(bool show)
{
    p->showDummy = show;

    if(p->dummy && !show) {
        p->dummy->deleteLater();
        p->dummy = nullptr;
    }
}

SplitterWidget::~SplitterWidget() = default;

Qt::Orientation SplitterWidget::orientation() const
{
    return p->splitter->orientation();
}

void SplitterWidget::setOrientation(Qt::Orientation orientation)
{
    p->splitter->setOrientation(orientation);
    setObjectName(SplitterWidget::name());
}

QByteArray SplitterWidget::saveState() const
{
    return p->splitter->saveState();
}

bool SplitterWidget::restoreState(const QByteArray& state)
{
    return p->splitter->restoreState(state);
}

QWidget* SplitterWidget::widgetAtIndex(int index) const
{
    return p->splitter->widget(index);
}

int SplitterWidget::childCount()
{
    return static_cast<int>(p->children.size());
}

void SplitterWidget::addWidget(QWidget* newWidget)
{
    if(p->limit > 0 && p->widgetCount >= p->limit) {
        return;
    }

    auto* widget = qobject_cast<FyWidget*>(newWidget);
    if(!widget) {
        return;
    }

    p->widgetCount += 1;

    if(p->dummy && p->widgetCount > 0) {
        p->dummy->deleteLater();
        p->dummy = nullptr;
    }

    const int index = static_cast<int>(p->children.size());
    p->children.push_back(widget);
    return p->splitter->insertWidget(index, widget);
}

void SplitterWidget::insertWidget(int index, FyWidget* widget)
{
    if(index > static_cast<int>(p->children.size())) {
        return;
    }

    if(p->limit > 0 && p->widgetCount >= p->limit) {
        return;
    }

    if(!widget) {
        return;
    }

    p->widgetCount += 1;

    p->children.insert(index, widget);
    p->splitter->insertWidget(index, widget);
}

void SplitterWidget::setupAddWidgetMenu(Utils::ActionContainer* menu)
{
    if(!menu->isEmpty()) {
        return;
    }

    const auto widgets = p->widgetFactory->registeredWidgets();
    for(const auto& widget : widgets) {
        auto* parentMenu = menu;

        for(const auto& subMenu : widget.second.subMenus) {
            const Utils::Id id = Utils::Id{menu->id()}.append(subMenu);
            auto* childMenu    = p->actionManager->actionContainer(id);

            if(!childMenu) {
                childMenu = p->actionManager->createMenu(id);
                childMenu->menu()->setTitle(subMenu);
                parentMenu->addMenu(childMenu);
            }
            parentMenu = childMenu;
        }
        auto* addWidgetAction = new QAction(widget.second.name, parentMenu);
        QObject::connect(addWidgetAction, &QAction::triggered, this, [this, widget] {
            FyWidget* newWidget = p->widgetFactory->make(widget.first);
            addWidget(newWidget);
        });
        parentMenu->addAction(addWidgetAction);
    }
}

void SplitterWidget::replaceWidget(int index, FyWidget* widget)
{
    if(!widget || p->children.empty()) {
        return;
    }

    if(index < 0 || index >= childCount()) {
        return;
    }

    FyWidget* oldWidget = p->children.takeAt(index);
    oldWidget->deleteLater();

    p->children.insert(index, widget);
    p->splitter->insertWidget(index, widget);
}

void SplitterWidget::replaceWidget(FyWidget* oldWidget, FyWidget* newWidget)
{
    if(!oldWidget || !newWidget || p->children.empty()) {
        return;
    }

    const int index = findIndex(oldWidget);
    replaceWidget(index, newWidget);
}

void SplitterWidget::removeWidget(FyWidget* widget)
{
    const int index = findIndex(widget);
    if(index < 0) {
        return;
    }

    p->widgetCount -= 1;

    widget->deleteLater();
    p->children.remove(index);

    p->checkShowDummy();
}

int SplitterWidget::findIndex(FyWidget* widgetToFind)
{
    for(int i = 0; i < p->children.size(); ++i) {
        FyWidget* widget = p->children.value(i);
        if(widget == widgetToFind) {
            return i;
        }
    }
    return -1;
}

QString SplitterWidget::name() const
{
    return QString("%1 Splitter").arg(orientation() == Qt::Horizontal ? "Horizontal" : "Vertical");
}

QString SplitterWidget::layoutName() const
{
    return QString("Splitter%1").arg(orientation() == Qt::Horizontal ? "Horizontal" : "Vertical");
}

void SplitterWidget::layoutEditingMenu(Utils::ActionContainer* menu)
{
    QAction* changeSplitter = new QAction("Change Splitter", this);
    QAction::connect(changeSplitter, &QAction::triggered, this, [this] {
        setOrientation(p->splitter->orientation() == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal);
        setObjectName(QString("%1 Splitter").arg(Utils::EnumHelper::toString(p->splitter->orientation())));
    });
    menu->addAction(changeSplitter);

    if(p->limit > 0 && p->widgetCount >= p->limit) {
        return;
    }

    auto* addMenu = createNewMenu(p->actionManager, this, tr("&Add"));
    setupAddWidgetMenu(addMenu);
    menu->addMenu(addMenu);
}

void SplitterWidget::saveLayout(QJsonArray& array)
{
    QJsonArray children;
    for(const auto& widget : p->children) {
        widget->saveLayout(children);
    }
    const QString state = QString::fromUtf8(saveState().toBase64());

    QJsonObject options;
    options["State"]    = state;
    options["Children"] = children;

    QJsonObject splitter;
    splitter[layoutName()] = options;
    array.append(splitter);
}

void SplitterWidget::loadLayout(const QJsonObject& object)
{
    const auto state    = QByteArray::fromBase64(object["State"].toString().toUtf8());
    const auto children = object["Children"].toArray();

    for(const auto& widget : children) {
        const QJsonObject jsonObject = widget.toObject();
        if(!jsonObject.isEmpty()) {
            const auto widgetName = jsonObject.constBegin().key();
            if(auto* childWidget = p->widgetFactory->make(widgetName)) {
                addWidget(childWidget);
                const QJsonObject widgetObject = jsonObject.value(widgetName).toObject();
                childWidget->loadLayout(widgetObject);
            }
        }
        else {
            auto* childWidget = p->widgetFactory->make(widget.toString());
            addWidget(childWidget);
        }
    }
    restoreState(state);

    p->checkShowDummy();
}
} // namespace Fy::Gui::Widgets

#include "splitterwidget.moc"
