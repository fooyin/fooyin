/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <gui/configdialog.h>

#include "utils/stringcollator.h"

#include <utils/fypaths.h>

#include <QDialogButtonBox>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>

using namespace Qt::StringLiterals;

// TODO: Add to ConfigDialog members for 1.0.
constexpr auto DefaultsButtonsName = "ConfigDefaultsButtons";

namespace Fooyin {
namespace {
std::optional<QJsonObject> readPresets()
{
    QFile file{Utils::configPath() + "/widgetpresets.json"_L1};
    if(!file.exists()) {
        return QJsonObject{};
    }
    if(!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const auto document = QJsonDocument::fromJson(file.readAll());
    if(!document.isObject()) {
        return {};
    }

    return document.object();
}

QJsonObject savedPresets(const QString& widgetType)
{
    const auto presets = readPresets();
    return presets ? presets->value(widgetType).toObject() : QJsonObject{};
}

bool storePresets(const QString& widgetType, const QJsonObject& presets)
{
    auto storedPresets = readPresets();
    if(!storedPresets) {
        return false;
    }

    storedPresets->insert(widgetType, presets);

    QSaveFile file{Utils::configPath() + "/widgetpresets.json"_L1};
    const QByteArray presetData = QJsonDocument{*storedPresets}.toJson(QJsonDocument::Indented);
    return file.open(QIODevice::WriteOnly) && file.write(presetData) == presetData.size() && file.commit();
}
} // namespace

ConfigDialog::ConfigDialog(const QString& title, QWidget* parent)
    : QDialog{parent}
    , m_contentLayout{new QGridLayout()}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(title);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(m_contentLayout);

    auto* defaultsButtons = new QDialogButtonBox(this);
    defaultsButtons->setObjectName(QLatin1StringView{DefaultsButtonsName});

    auto* defaultsButton = defaultsButtons->addButton(tr("Defaults"), QDialogButtonBox::ButtonRole::ActionRole);
    auto* defaultsMenu   = new QMenu(defaultsButton);

    auto* saveDefaultsAction           = new QAction(tr("Save as default"), defaultsMenu);
    auto* restoreSavedDefaultsAction   = new QAction(tr("Restore saved defaults"), defaultsMenu);
    auto* restoreFactoryDefaultsAction = new QAction(tr("Restore factory defaults"), defaultsMenu);
    auto* clearSavedDefaultsAction     = new QAction(tr("Reset saved defaults to factory"), defaultsMenu);

    defaultsMenu->addAction(saveDefaultsAction);
    defaultsMenu->addSeparator();
    defaultsMenu->addAction(restoreSavedDefaultsAction);
    defaultsMenu->addAction(restoreFactoryDefaultsAction);
    defaultsMenu->addSeparator();
    defaultsMenu->addAction(clearSavedDefaultsAction);

    defaultsButton->setMenu(defaultsMenu);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Apply
                                             | QDialogButtonBox::StandardButton::Cancel,
                                         this);

    if(auto* okButton = buttons->button(QDialogButtonBox::StandardButton::Ok)) {
        okButton->setDefault(true);
    }

    QObject::connect(restoreSavedDefaultsAction, &QAction::triggered, this, &ConfigDialog::restoreSavedDefaults);
    QObject::connect(restoreFactoryDefaultsAction, &QAction::triggered, this, &ConfigDialog::restoreFactoryDefaults);
    QObject::connect(clearSavedDefaultsAction, &QAction::triggered, this, &ConfigDialog::clearSavedDefaults);
    QObject::connect(saveDefaultsAction, &QAction::triggered, this, &ConfigDialog::saveDefaults);
    QObject::connect(buttons->button(QDialogButtonBox::StandardButton::Apply), &QPushButton::clicked, this,
                     &ConfigDialog::apply);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        apply();
        accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* footerLayout = new QHBoxLayout();
    footerLayout->addWidget(defaultsButtons, 0, Qt::AlignLeft);
    footerLayout->addStretch();
    footerLayout->addWidget(buttons);

    mainLayout->addLayout(footerLayout);
}

QGridLayout* ConfigDialog::contentLayout() const
{
    return m_contentLayout;
}

void ConfigDialog::addPresets(const QString& widgetType, std::function<QJsonObject()> save,
                              std::function<void(const QJsonObject&)> restore)
{
    auto* defaultsButtons = findChild<QDialogButtonBox*>(QLatin1StringView{DefaultsButtonsName});
    auto* presetButton    = defaultsButtons->addButton(tr("Presets"), QDialogButtonBox::ActionRole);

    auto* presetMenu = new QMenu(presetButton);
    presetButton->setMenu(presetMenu);

    auto* saveAction  = presetMenu->addAction(tr("Save preset…"));
    auto* restoreMenu = presetMenu->addMenu(tr("Restore preset"));
    auto* deleteMenu  = presetMenu->addMenu(tr("Delete preset"));

    QObject::connect(saveAction, &QAction::triggered, this, [this, widgetType, save]() {
        bool accepted{false};
        const QString name
            = QInputDialog::getText(this, tr("Save Preset"), tr("Preset name"), QLineEdit::Normal, {}, &accepted)
                  .trimmed();
        if(!accepted || name.isEmpty()) {
            return;
        }

        auto presets = savedPresets(widgetType);
        if(presets.contains(name)
           && QMessageBox::question(this, tr("Replace Preset"), tr("Replace the preset ‘%1’?").arg(name),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                  != QMessageBox::Yes) {
            return;
        }

        // Reload after the modal prompt so any edits made from another dialog are retained
        presets = savedPresets(widgetType);
        presets.insert(name, save());
        if(!storePresets(widgetType, presets)) {
            QMessageBox::warning(this, tr("Save Preset"), tr("The preset could not be saved."));
        }
    });

    QObject::connect(presetMenu, &QMenu::aboutToShow, this, [this, widgetType, restoreMenu, deleteMenu, restore]() {
        restoreMenu->clear();
        deleteMenu->clear();

        const auto presets = savedPresets(widgetType);

        auto names{presets.keys()};
        StringCollator collator;
        std::ranges::sort(names, [&collator](const QString& left, const QString& right) {
            return collator.compare(left, right) < 0;
        });

        restoreMenu->setEnabled(!names.isEmpty());
        deleteMenu->setEnabled(!names.isEmpty());

        for(const QString& name : names) {
            if(!presets.value(name).isObject()) {
                continue;
            }

            QString label{name};
            label.replace(u'&', "&&"_L1);
            auto* restoreAction = restoreMenu->addAction(label);
            auto* deleteAction  = deleteMenu->addAction(label);

            QObject::connect(restoreAction, &QAction::triggered, this, [widgetType, name, restore]() {
                const auto preset = savedPresets(widgetType).value(name);
                if(preset.isObject()) {
                    restore(preset.toObject());
                }
            });
            QObject::connect(deleteAction, &QAction::triggered, this, [this, widgetType, name]() {
                if(QMessageBox::question(this, tr("Delete Preset"), tr("Delete the preset ‘%1’?").arg(name),
                                         QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                   != QMessageBox::Yes) {
                    return;
                }

                auto currentPresets = savedPresets(widgetType);
                currentPresets.remove(name);
                if(!storePresets(widgetType, currentPresets)) {
                    QMessageBox::warning(this, tr("Delete Preset"), tr("The preset could not be deleted."));
                }
            });
        }
    });
}
} // namespace Fooyin
