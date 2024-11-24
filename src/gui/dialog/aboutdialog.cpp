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

#include "aboutdialog.h"

#include <core/constants.h>
#include <gui/guiconstants.h>
#include <utils/utils.h>

#include <QApplication>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSysInfo>

using namespace Qt::StringLiterals;

constexpr int IconSize = 256;

namespace {
QString compilerVersion()
{
#if defined(Q_CC_CLANG)
    return u"Clang "_s + QString::number(__clang_major__) + u"."_s + QString::number(__clang_minor__);
#elif defined(Q_CC_GNU)
    return u"GCC "_s + QLatin1String(__VERSION__);
#elif defined(Q_CC_MSVC)
    return u"MSVC "_s + QString::number(_MSC_VER);
#endif
    return u"<unknown compiler>"_s;
}

QString copyright()
{
    return u"Copyright © 2024, Luke Taylor. All rights reserved.<br/>"
           "<br/>"
           "%1 is free software released under GPL. The source code is available on %2<br/>"
           "<br/>"
           "You should have received a copy of the GNU General Public License along with this program. If "
           "not, see "
           "%3"_s.arg("fooyin"_L1, R"(<a href="https://github.com/fooyin/fooyin">GitHub</a>.)"_L1,
                      R"(<a href="http://www.gnu.org/licenses">http://www.gnu.org/licenses</a>.)"_L1);
}

QString qtVersion()
{
    return u"Based on Qt %1 (%2, %3)"_s.arg(QString::fromLatin1(QT_VERSION_STR), compilerVersion(),
                                            QSysInfo::buildCpuArchitecture());
}

QString description()
{
    return u"<h3>%1</h3>"
           "Version: %2<br/>"
           "%3<br/>"
           "<br/>"_s.arg("fooyin"_L1, QCoreApplication::applicationVersion(), qtVersion());
}
} // namespace

namespace Fooyin {
AboutDialog::AboutDialog(QWidget* parent)
    : QDialog{parent}
{
    setWindowTitle(tr("About %1").arg("fooyin"_L1));
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
    logo->setPixmap(Utils::iconFromTheme(Constants::Icons::Fooyin).pixmap(IconSize));

    layout->addWidget(logo, 0, 0);
    layout->addWidget(aboutLabel, 0, 1);
    layout->addWidget(buttonBox, 4, 1);
}
} // namespace Fooyin

#include "moc_aboutdialog.cpp"
