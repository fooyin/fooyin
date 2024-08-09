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
#include "utils/settings/settingsmanager.h"

#include <gui/propertiesdialog.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QPushButton>
#include <QTabWidget>

#include <ranges>

constexpr auto DialogSize = "Interface/PropertiesDialogSize";

namespace Fooyin {
PropertiesTab::PropertiesTab(QString title, WidgetBuilder widgetBuilder, int index)
    : m_index{index}
    , m_title{std::move(title)}
    , m_widgetBuilder{std::move(widgetBuilder)}
{ }

int PropertiesTab::index() const
{
    return m_index;
}

QString PropertiesTab::title() const
{
    return m_title;
}

QWidget* PropertiesTab::widget(const TrackList& tracks) const
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

    void saveState(SettingsManager* settings)
    {
        settings->fileSet(DialogSize, size());
    }

    void restoreState(SettingsManager* settings)
    {
        if(settings->fileContains(DialogSize)) {
            const QSize size = settings->fileValue(DialogSize).toSize();
            if(size.isValid()) {
                resize(size);
            }
        }
    }

private:
    void done(int value) override;
    void accept() override;
    void reject() override;

    void apply();

    void currentTabChanged(int index);

    PropertiesDialog::TabList m_tabs;
    TrackList m_tracks;
};

PropertiesDialogWidget::PropertiesDialogWidget(TrackList tracks, PropertiesDialog::TabList tabs)
    : m_tabs{std::move(tabs)}
    , m_tracks{std::move(tracks)}
{
    auto* layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 5);

    setWindowTitle(tr("Properties"));

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);
    buttonBox->setContentsMargins(0, 0, 5, 5);

    QObject::connect(buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this,
                     &PropertiesDialogWidget::apply);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* tabWidget = new QTabWidget(this);
    QObject::connect(tabWidget, &QTabWidget::currentChanged, this, &PropertiesDialogWidget::currentTabChanged);

    for(const auto& tab : m_tabs) {
        tabWidget->insertTab(tab.index(), tab.widget(m_tracks), tab.title());
    }

    layout->addWidget(tabWidget, 0, 0);
    layout->addWidget(buttonBox, 1, 0);

    buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);
    layout->setSizeConstraint(QLayout::SetMinimumSize);

    tabWidget->setCurrentIndex(0);
}

void PropertiesDialogWidget::done(int value)
{
    QDialog::done(value);
}

void PropertiesDialogWidget::accept()
{
    apply();
    for(PropertiesTab& tab : m_tabs) {
        tab.finish();
    }
    done(Accepted);
}

void PropertiesDialogWidget::reject()
{
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
    if(index < 0) {
        return;
    }

    auto tabIt = std::ranges::find_if(m_tabs, [index](const PropertiesTab& tab) { return tab.index() == index; });
    if(tabIt != m_tabs.cend()) {
        tabIt->setVisited(true);
        const Track firstTrack = m_tracks.front();
        const QString title    = !firstTrack.title().isEmpty() ? firstTrack.title() : firstTrack.filename();
        const QString subtitle = m_tracks.size() == 1
                                   ? QStringLiteral(" (%1): %2").arg(title, tabIt->title())
                                   : QStringLiteral(" (%1 tracks): %2").arg(m_tracks.size()).arg(tabIt->title());
        setWindowTitle(tr("Properties") + subtitle);
    }
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
    for(int i = 0; i < count; ++i) {
        m_tabs[i].updateIndex(i);
    }
}

void PropertiesDialog::show(const TrackList& tracks)
{
    auto* dialog = new PropertiesDialogWidget(tracks, m_tabs);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(dialog, &QDialog::finished, this, [this, dialog]() { dialog->saveState(m_settings); });

    dialog->resize(600, 700);
    dialog->show();

    dialog->restoreState(m_settings);
}
} // namespace Fooyin

#include "gui/moc_propertiesdialog.cpp"
#include "propertiesdialog.moc"
