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

#include <utils/settings/settingsdialogcontroller.h>

#include "settingsdialog.h"

#include <utils/id.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QIODevice>
#include <QMainWindow>

constexpr auto DialogSize   = "Interface/SettingsDialogSize";
constexpr auto LastOpenPage = "Interface/SettingsDialogLastPage";

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

    QSize size;
    PageList pages;
    Id lastOpenPage;
    bool isOpen{false};
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

    auto* settingsDialog = new SettingsDialog{p->pages, p->mainWindow};

    QObject::connect(settingsDialog, &QDialog::finished, this, [this, settingsDialog]() {
        p->size         = settingsDialog->size();
        p->lastOpenPage = settingsDialog->currentPage();
        p->isOpen       = false;
        emit closing();
        settingsDialog->deleteLater();
    });
    QObject::connect(settingsDialog, &SettingsDialog::resettingAll, this,
                     [this]() { p->settings->resetAllSettings(); });

    if(p->size.isValid()) {
        settingsDialog->resize(p->size);
    }
    else {
        settingsDialog->resize({800, 500});
    }

    p->isOpen = true;
    emit opening();

    settingsDialog->openSettings();

    if(page.isValid()) {
        settingsDialog->openPage(page);
    }
}

void SettingsDialogController::addPage(SettingsPage* page)
{
    p->pages.push_back(page);
}

void SettingsDialogController::saveState()
{
    p->settings->fileSet(DialogSize, p->size);
    p->settings->fileSet(LastOpenPage, p->lastOpenPage.name());
}

void SettingsDialogController::restoreState()
{
    if(p->settings->fileContains(DialogSize)) {
        const QSize size = p->settings->fileValue(DialogSize).toSize();
        if(size.isValid()) {
            p->size = size;
        }
    }

    if(p->settings->fileContains(LastOpenPage)) {
        const Id lastPage{p->settings->fileValue(LastOpenPage).toString()};
        if(lastPage.isValid()) {
            p->lastOpenPage = lastPage;
        }
    }
}
} // namespace Fooyin

#include "utils/settings/moc_settingsdialogcontroller.cpp"
