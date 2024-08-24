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

#include <gui/widgets/scriptlineedit.h>

#include "sandbox/sandboxdialog.h"

#include <gui/guiconstants.h>
#include <utils/utils.h>

#include <QAction>
#include <QIcon>

namespace Fooyin {
ScriptLineEdit::ScriptLineEdit(QWidget* parent)
    : ScriptLineEdit{{}, parent}
{ }

ScriptLineEdit::ScriptLineEdit(const QString& script, QWidget* parent)
    : QLineEdit{script, parent}
{
    auto* openEditor = new QAction(Utils::iconFromTheme(Constants::Icons::ScriptSandbox), tr("Open Sandbox"), this);
    QObject::connect(openEditor, &QAction::triggered, this, [this]() {
        SandboxDialog::openSandbox(text(), [this](const QString& editedScript) { setText(editedScript); });
    });
    addAction(openEditor, TrailingPosition);
}
} // namespace Fooyin
