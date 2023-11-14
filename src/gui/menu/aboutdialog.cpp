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

#include "aboutdialog.h"

#include "gui/guiconstants.h"

#include <core/constants.h>

#include <QApplication>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSysInfo>

using namespace Qt::Literals::StringLiterals;

constexpr int IconSize = 256;

namespace {
QString compilerVersion()
{
#if defined(Q_CC_CLANG)
    QString platformSpecific;
#if defined(Q_CC_MSVC)
    platformSpecific = QLatin1String(" (clang-cl)");
#endif
    return QStringLiteral("Clang ") + QString::number(__clang_major__) + QChar('.') + QString::number(__clang_minor__)
         + platformSpecific;
#elif defined(Q_CC_GNU)
    return QLatin1String("GCC ") + QLatin1String(__VERSION__);
#endif
    return QStringLiteral("<unknown compiler>");
}

QString copyright()
{
    return QString{"Copyright 2022-%1 %2. All rights reserved.<br/>"
                   "<br/>"
                   "%3 is free software released under GPL. The source code is available on %4<br/>"
                   "<br/>"
                   "You should have received a copy of the GNU General Public License along with this program.  If "
                   "not, see "
                   "%5"}
        .arg("2023", "Luke Taylor", Fy::Core::Constants::DisplayName,
             "<a href=\"https://github.com/ludouzi/fooyin\">GitHub</a>.",
             "<a href=\"http://www.gnu.org/licenses\">http://www.gnu.org/licenses</a>.");
}

QString qtVersion()
{
    return QString{u"Based on Qt %1 (%2, %3)"_s}.arg(qVersion(), compilerVersion(), QSysInfo::buildCpuArchitecture());
}

QString description()
{
    return QString{"<h3>%1</h3>"
                   "Version: %2<br/>"
                   "%3<br/>"
                   "<br/>"}
        .arg(Fy::Core::Constants::DisplayName, QCoreApplication::applicationVersion(), qtVersion());
}
} // namespace

namespace Fy::Gui {
AboutDialog::AboutDialog(QWidget* parent)
    : QDialog{parent}
{
    setWindowTitle(tr("About %1").arg(Core::Constants::DisplayName));
    auto* layout = new QGridLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    auto* aboutLabel = new QLabel(description() + copyright());
    aboutLabel->setWordWrap(true);
    aboutLabel->setOpenExternalLinks(true);
    aboutLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);

    auto* buttonBox          = new QDialogButtonBox(QDialogButtonBox::Close);
    QPushButton* closeButton = buttonBox->button(QDialogButtonBox::Close);
    buttonBox->addButton(closeButton, QDialogButtonBox::AcceptRole);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto* logo = new QLabel(this);
    logo->setPixmap(QIcon::fromTheme(Constants::Icons::Fooyin).pixmap(IconSize));

    layout->addWidget(logo, 0, 0);
    layout->addWidget(aboutLabel, 0, 1);
    layout->addWidget(buttonBox, 4, 1);
}
} // namespace Fy::Gui

#include "moc_aboutdialog.cpp"
