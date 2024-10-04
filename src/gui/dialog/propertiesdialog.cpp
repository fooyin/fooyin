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

#include "qdialog.h"

#include <core/coresettings.h>
#include <gui/propertiesdialog.h>
#include <utils/settings/settingsmanager.h>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QMenu>
#include <QPushButton>
#include <QTabWidget>
#include <QToolButton>

#include <ranges>

constexpr auto DialogGeometry = "Interface/PropertiesDialogGeometry";

namespace Fooyin {
bool PropertiesTabWidget::canApply() const
{
    return true;
}

bool PropertiesTabWidget::hasTools() const
{
    return false;
}

void PropertiesTabWidget::apply() { }

void PropertiesTabWidget::addTools(QMenu* /*menu*/) { }

PropertiesTab::PropertiesTab(QString title, WidgetBuilder widgetBuilder, int index)
    : m_index{index}
    , m_title{std::move(title)}
    , m_widgetBuilder{std::move(widgetBuilder)}
    , m_widget{nullptr}
    , m_visited{false}
{ }

int PropertiesTab::index() const
{
    return m_index;
}

QString PropertiesTab::title() const
{
    return m_title;
}

PropertiesTabWidget* PropertiesTab::widget(const TrackList& tracks) const
{
    if(!m_widget) {
        if(m_widgetBuilder) {
            m_widget = m_widgetBuilder(tracks);
        }
    }
    return m_widget;
}

bool PropertiesTab::hasVisited() const
{
    return m_visited;
}

void PropertiesTab::updateIndex(int index)
{
    m_index = index;
}

void PropertiesTab::setVisited(bool visited)
{
    m_visited = visited;
}

void PropertiesTab::apply()
{
    if(m_widget) {
        m_widget->apply();
    }
}

void PropertiesTab::finish()
{
    setVisited(false);
    if(m_widget) {
        delete m_widget;
        m_widget = nullptr;
    }
}

class PropertiesDialogWidget : public QDialog
{
    Q_OBJECT

public:
    explicit PropertiesDialogWidget(TrackList tracks, PropertiesDialog::TabList tabs);

    void saveState()
    {
        FyStateSettings stateSettings;
        stateSettings.setValue(QLatin1String{DialogGeometry}, saveGeometry());
    }

    void restoreState()
    {
        const FyStateSettings stateSettings;
        if(stateSettings.contains(QLatin1String{DialogGeometry})) {
            const auto geometry = stateSettings.value(QLatin1String{DialogGeometry}).toByteArray();
            if(!geometry.isEmpty()) {
                restoreGeometry(geometry);
            }
        }
    }

private:
    void done(int value) override;
    void accept() override;
    void reject() override;

    void apply();

    void currentTabChanged(int index);

    bool m_closing{false};
    QPushButton* m_applyButton{nullptr};
    QToolButton* m_toolsButton;
    QMenu* m_toolsMenu;
    PropertiesDialog::TabList m_tabs;
    TrackList m_tracks;
};

PropertiesDialogWidget::PropertiesDialogWidget(TrackList tracks, PropertiesDialog::TabList tabs)
    : m_toolsButton{new QToolButton(this)}
    , m_toolsMenu{new QMenu(tr("Tools"), this)}
    , m_tabs{std::move(tabs)}
    , m_tracks{std::move(tracks)}
{
    setWindowTitle(tr("Properties"));

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel);

    QObject::connect(buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this,
                     &PropertiesDialogWidget::apply);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* tabWidget = new QTabWidget(this);
    QObject::connect(tabWidget, &QTabWidget::currentChanged, this, &PropertiesDialogWidget::currentTabChanged);

    m_applyButton = buttonBox->button(QDialogButtonBox::Apply);
    buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);
    tabWidget->setCurrentIndex(0);

    m_toolsButton->setText(tr("Tools"));
    m_toolsButton->setMenu(m_toolsMenu);
    m_toolsButton->setPopupMode(QToolButton::InstantPopup);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->setSizeConstraint(QLayout::SetMinimumSize);

    auto* bottomLayout = new QGridLayout();
    bottomLayout->setContentsMargins(5, 0, 5, 5);

    bottomLayout->addWidget(m_toolsButton, 1, 0);
    bottomLayout->addWidget(buttonBox, 1, 1);

    layout->addWidget(tabWidget, 1);
    layout->addLayout(bottomLayout);

    for(const auto& tab : m_tabs) {
        tabWidget->insertTab(tab.index(), tab.widget(m_tracks), tab.title());
    }
}

void PropertiesDialogWidget::done(int value)
{
    QDialog::done(value);
}

void PropertiesDialogWidget::accept()
{
    apply();
    m_closing = true;
    for(PropertiesTab& tab : m_tabs) {
        tab.finish();
    }
    done(Accepted);
}

void PropertiesDialogWidget::reject()
{
    m_closing = true;
    for(PropertiesTab& tab : m_tabs) {
        tab.finish();
    }
    done(Rejected);
}

void PropertiesDialogWidget::apply()
{
    auto visitedTabs = std::views::filter(m_tabs, [&](const PropertiesTab& tab) { return tab.hasVisited(); });
    for(PropertiesTab& tab : visitedTabs) {
        tab.apply();
    }
}

void PropertiesDialogWidget::currentTabChanged(int index)
{
    if(index < 0 || m_closing) {
        return;
    }

    auto tabIt = std::ranges::find_if(m_tabs, [index](const PropertiesTab& tab) { return tab.index() == index; });
    if(tabIt == m_tabs.cend()) {
        return;
    }

    tabIt->setVisited(true);
    const QString subtitle = m_tracks.size() == 1
                               ? QStringLiteral(" (%1): %2").arg(m_tracks.front().effectiveTitle(), tabIt->title())
                               : QStringLiteral(" (%1 tracks): %2").arg(m_tracks.size()).arg(tabIt->title());

    setWindowTitle(tr("Properties") + subtitle);

    if(auto* widget = tabIt->widget(m_tracks)) {
        if(m_applyButton) {
            m_applyButton->setHidden(!widget->canApply());
        }
        if(widget->hasTools()) {
            m_toolsButton->show();
            m_toolsMenu->clear();
            widget->addTools(m_toolsMenu);
            return;
        }
    }

    m_toolsButton->hide();
}

PropertiesDialog::PropertiesDialog(SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_settings{settings}
{ }

PropertiesDialog::~PropertiesDialog()
{
    m_tabs.clear();
}

void PropertiesDialog::addTab(const QString& title, const WidgetBuilder& widgetBuilder)
{
    const int index = static_cast<int>(m_tabs.size());
    m_tabs.emplace_back(title, widgetBuilder, index);
}

void PropertiesDialog::addTab(const PropertiesTab& tab)
{
    const int index = static_cast<int>(m_tabs.size());
    PropertiesTab newTab{tab};
    newTab.updateIndex(index);
    m_tabs.emplace_back(tab);
}

void PropertiesDialog::insertTab(int index, const QString& title, const WidgetBuilder& widgetBuilder)
{
    m_tabs.insert(m_tabs.begin() + index, {title, widgetBuilder, index});

    const int count = static_cast<int>(m_tabs.size());
    for(int i{0}; i < count; ++i) {
        m_tabs[i].updateIndex(i);
    }
}

void PropertiesDialog::show(const TrackList& tracks)
{
    auto* dialog = new PropertiesDialogWidget(tracks, m_tabs);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(dialog, &QDialog::finished, dialog, &PropertiesDialogWidget::saveState);

    dialog->resize(600, 700);
    dialog->show();

    dialog->restoreState();
}
} // namespace Fooyin

#include "gui/moc_propertiesdialog.cpp"
#include "propertiesdialog.moc"
