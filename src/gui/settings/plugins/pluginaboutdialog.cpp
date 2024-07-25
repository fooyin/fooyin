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

#include "pluginaboutdialog.h"

#include <core/plugins/plugininfo.h>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>

namespace Fooyin {
PluginAboutDialog::PluginAboutDialog(PluginInfo* plugin, QWidget* parent)
    : QDialog{parent}
{
    auto* layout = new QGridLayout(this);
    layout->setHorizontalSpacing(20);

    auto* nameLabel      = new QLabel(tr("Name") + QStringLiteral(":"), this);
    auto* versionLabel   = new QLabel(tr("Version") + QStringLiteral(":"), this);
    auto* authorLabel    = new QLabel(tr("Author") + QStringLiteral(":"), this);
    auto* categoryLabel  = new QLabel(tr("Category") + QStringLiteral(":"), this);
    auto* descLabel      = new QLabel(tr("Description") + QStringLiteral(":"), this);
    auto* urlLabel       = new QLabel(tr("URL") + QStringLiteral(":"), this);
    auto* copyrightLabel = new QLabel(tr("Copyright") + QStringLiteral(":"), this);
    auto* licenseLabel   = new QLabel(tr("License") + QStringLiteral(":"), this);

    auto* name      = new QLabel(plugin->name(), this);
    auto* version   = new QLabel(plugin->version(), this);
    auto* author    = new QLabel(plugin->author(), this);
    auto* category  = new QLabel(plugin->category().join(u", "), this);
    auto* desc      = new QLabel(plugin->description(), this);
    auto* url       = new QLabel(QStringLiteral("<a href=\"%1/\">%1</a>").arg(plugin->url()), this);
    auto* copyright = new QLabel(plugin->copyright(), this);
    auto* license   = new QPlainTextEdit(plugin->license(), this);

    url->setTextInteractionFlags(Qt::TextBrowserInteraction);
    url->setOpenExternalLinks(true);
    license->setReadOnly(true);

    auto* buttonBox          = new QDialogButtonBox(QDialogButtonBox::Close);
    QPushButton* closeButton = buttonBox->button(QDialogButtonBox::Close);
    buttonBox->addButton(closeButton, QDialogButtonBox::AcceptRole);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

    int row{0};
    layout->addWidget(nameLabel, row, 0);
    layout->addWidget(name, row++, 1);
    layout->addWidget(versionLabel, row, 0);
    layout->addWidget(version, row++, 1);
    layout->addWidget(authorLabel, row, 0);
    layout->addWidget(author, row++, 1);
    layout->addWidget(categoryLabel, row, 0);
    layout->addWidget(category, row++, 1);
    layout->addWidget(descLabel, row, 0);
    layout->addWidget(desc, row++, 1);
    layout->addWidget(urlLabel, row, 0);
    layout->addWidget(url, row++, 1);
    layout->addWidget(copyrightLabel, row, 0);
    layout->addWidget(copyright, row++, 1);
    layout->addWidget(licenseLabel, row, 0);
    layout->addWidget(license, row++, 1);
    layout->addWidget(buttonBox, row++, 0, 1, 2);
}
} // namespace Fooyin

#include "moc_pluginaboutdialog.cpp"
