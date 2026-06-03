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

#include <utils/settings/settingsdialogcontroller.h>

#include "settingsdialog.h"

#include <utils/id.h>
#include <utils/settings/settingsmanager.h>
#include <utils/settings/settingspage.h>

#include <QApplication>
#include <QDataStream>
#include <QIODevice>
#include <QMainWindow>
#include <QSettings>

#include <unordered_map>

constexpr auto DialogGeometry     = "SettingsDialog/Geometry";
constexpr auto DialogSize         = "SettingsDialog/Size";
constexpr auto LastOpenPage       = "SettingsDialog/LastPage";
constexpr auto ExpandedCategories = "SettingsDialog/ExpandedCategories";
constexpr auto PageStates         = "SettingsDialog/PageStates";

namespace {
QByteArray savePageStates(const std::unordered_map<QString, QByteArray>& pageStates)
{
    QByteArray stateData;
    QDataStream stream{&stateData, QIODeviceBase::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    stream << static_cast<quint32>(pageStates.size());
    for(const auto& [key, state] : pageStates) {
        stream << key << state;
    }

    return qCompress(stateData, 9);
}

std::unordered_map<QString, QByteArray> restorePageStates(const QByteArray& state)
{
    if(state.isEmpty()) {
        return {};
    }

    QByteArray stateData = qUncompress(state);
    QDataStream stream{&stateData, QIODeviceBase::ReadOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    quint32 count{0};
    stream >> count;

    std::unordered_map<QString, QByteArray> pageStates;
    for(quint32 i{0}; i < count; ++i) {
        QString key;
        QByteArray pageState;
        stream >> key >> pageState;

        if(!key.isEmpty() && !pageState.isEmpty()) {
            pageStates.emplace(std::move(key), std::move(pageState));
        }
    }

    return pageStates;
}
} // namespace

namespace Fooyin {
class SettingsDialogControllerPrivate
{
public:
    explicit SettingsDialogControllerPrivate(SettingsManager* settings_, QMainWindow* mainWindow_)
        : settings{settings_}
        , mainWindow{mainWindow_}
    { }

    SettingsManager* settings;
    QMainWindow* mainWindow;

    QByteArray geometry;
    QSize size;
    QByteArray expandedCategories;
    std::unordered_map<QString, QByteArray> pageStates;
    PageList pages;
    Id lastOpenPage;
    bool isOpen{false};

    void applyPageStates() const
    {
        for(auto* page : pages) {
            if(const auto it = pageStates.find(page->id().name()); it != pageStates.cend()) {
                page->setState(it->second);
            }
        }
    }

    void updatePageStates()
    {
        for(const auto* page : pages) {
            if(const QByteArray state = page->state(); !state.isEmpty()) {
                pageStates.insert_or_assign(page->id().name(), state);
            }
        }
    }
};

SettingsDialogController::SettingsDialogController(SettingsManager* settings, QMainWindow* mainWindow)
    : QObject{settings}
    , p{std::make_unique<SettingsDialogControllerPrivate>(settings, mainWindow)}
{ }

SettingsDialogController::~SettingsDialogController() = default;

void SettingsDialogController::open()
{
    openAtPage(p->lastOpenPage);
}

void SettingsDialogController::openAtPage(const Id& page)
{
    if(p->isOpen) {
        return;
    }

    p->applyPageStates();

    auto* settingsDialog = new SettingsDialog{p->pages, p->mainWindow};

    QObject::connect(settingsDialog, &QDialog::finished, this, [this, settingsDialog]() {
        p->updatePageStates();
        p->geometry           = settingsDialog->saveGeometry();
        p->size               = settingsDialog->size();
        p->expandedCategories = settingsDialog->saveState();
        p->lastOpenPage       = settingsDialog->currentPage();
        p->isOpen             = false;
        Q_EMIT closing();
        settingsDialog->deleteLater();
    });
    QObject::connect(settingsDialog, &SettingsDialog::resettingAll, this,
                     [this]() { p->settings->resetAllSettings(); });

    if(!p->geometry.isEmpty()) {
        settingsDialog->restoreGeometry(p->geometry);
    }
    if(p->size.isValid()) {
        settingsDialog->resize(p->size);
    }
    settingsDialog->restoreState(p->expandedCategories);

    p->isOpen = true;
    Q_EMIT opening();

    settingsDialog->openSettings();

    if(page.isValid()) {
        settingsDialog->openPage(page);
    }
}

void SettingsDialogController::addPage(SettingsPage* page)
{
    p->pages.push_back(page);
}

void SettingsDialogController::saveState(QSettings& settings) const
{
    settings.setValue(DialogGeometry, p->geometry);
    settings.setValue(DialogSize, p->size);
    settings.setValue(ExpandedCategories, p->expandedCategories);
    settings.setValue(LastOpenPage, p->lastOpenPage.name());
    settings.setValue(PageStates, savePageStates(p->pageStates));
}

void SettingsDialogController::restoreState(const QSettings& settings)
{
    if(settings.contains(DialogGeometry)) {
        const auto geometry = settings.value(DialogGeometry).toByteArray();
        if(!geometry.isEmpty()) {
            p->geometry = geometry;
        }
    }
    if(settings.contains(DialogSize)) {
        const auto size = settings.value(DialogSize).toSize();
        if(size.isValid()) {
            p->size = size;
        }
    }
    if(settings.contains(ExpandedCategories)) {
        p->expandedCategories = settings.value(ExpandedCategories).toByteArray();
    }

    p->pageStates = restorePageStates(settings.value(PageStates).toByteArray());

    if(!p->pageStates.empty()) {
        p->applyPageStates();
    }

    if(settings.contains(LastOpenPage)) {
        const Id lastPage{settings.value(LastOpenPage).toString()};
        if(lastPage.isValid()) {
            p->lastOpenPage = lastPage;
        }
    }
}
} // namespace Fooyin

#include "utils/settings/moc_settingsdialogcontroller.cpp"
