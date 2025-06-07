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
#include <QSettings>

constexpr auto DialogGeometry = "SettingsDialog/Geometry";
constexpr auto DialogSize     = "SettingsDialog/Size";
constexpr auto LastOpenPage   = "SettingsDialog/LastPage";

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
        p->geometry     = settingsDialog->saveGeometry();
        p->size         = settingsDialog->size();
        p->lastOpenPage = settingsDialog->currentPage();
        p->isOpen       = false;
        emit closing();
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
    else if(p->geometry.isEmpty()) {
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

void SettingsDialogController::saveState(QSettings& settings) const
{
    settings.setValue(DialogGeometry, p->geometry);
    settings.setValue(DialogSize, p->size);
    settings.setValue(LastOpenPage, p->lastOpenPage.name());
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

    if(settings.contains(LastOpenPage)) {
        const Id lastPage{settings.value(LastOpenPage).toString()};
        if(lastPage.isValid()) {
            p->lastOpenPage = lastPage;
        }
    }
}
} // namespace Fooyin

#include "utils/settings/moc_settingsdialogcontroller.cpp"
