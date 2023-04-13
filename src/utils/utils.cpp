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

#include "utils.h"

#include <QDir>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QRandomGenerator>
#include <QVBoxLayout>
#include <QWidget>

namespace Fy::Utils {
namespace File {
QString cleanPath(const QString& path)
{
    return (path.trimmed().isEmpty()) ? QString("") : QDir::cleanPath(path);
}

bool isSamePath(const QString& filename1, const QString& filename2)
{
    const auto cleaned1 = cleanPath(filename1);
    const auto cleaned2 = cleanPath(filename2);

    return (cleaned1.compare(cleaned2) == 0);
}

bool isSubdir(const QString& dir, const QString& parentDir)
{
    if(isSamePath(dir, parentDir)) {
        return false;
    }

    const auto cleanedDir       = cleanPath(dir);
    const auto cleanedParentDir = cleanPath(parentDir);

    if(cleanedDir.isEmpty() || cleanedParentDir.isEmpty()) {
        return false;
    }

    const QFileInfo info(cleanedDir);

    QDir d1(cleanedDir);
    if(info.exists() && info.isFile()) {
        const auto d1String = getParentDirectory(cleanedDir);
        if(isSamePath(d1String, parentDir)) {
            return true;
        }

        d1 = QDir(d1String);
    }

    const QDir d2(cleanedParentDir);

    while(!d1.isRoot()) {
        d1 = QDir(getParentDirectory(d1.absolutePath()));
        if(isSamePath(d1.absolutePath(), d2.absolutePath())) {
            return true;
        }
    }

    return false;
}

bool exists(const QString& filename)
{
    return (!filename.isEmpty()) && QFile::exists(filename);
}

QString getParentDirectory(const QString& filename)
{
    const auto cleaned = cleanPath(filename);
    const auto index   = cleaned.lastIndexOf(QDir::separator());

    return (index > 0) ? cleanPath(cleaned.left(index)) : QDir::rootPath();
}

bool createDirectories(const QString& path)
{
    return QDir().mkpath(path);
}
} // namespace File

namespace Widgets {
QWidget* indentWidget(QWidget* widget, QWidget* parent)
{
    auto* indentWidget = new QWidget(parent);
    indentWidget->setLayout(new QVBoxLayout());
    indentWidget->layout()->addWidget(widget);
    indentWidget->layout()->setContentsMargins(25, 0, 0, 0);
    return indentWidget;
}
} // namespace Widgets

int randomNumber(int min, int max)
{
    if(min == max) {
        return max;
    }
    return QRandomGenerator::global()->bounded(min, max + 1);
}

QString msToString(uint64_t ms)
{
    const int milliseconds = static_cast<int>(ms);
    const QTime t(0, 0, 0);
    auto time = t.addMSecs(milliseconds);
    return time.toString(time.hour() == 0 ? "mm:ss" : "hh:mm:ss");
}

QString secsToString(uint64_t secs)
{
    const int seconds = static_cast<int>(secs);
    const QTime t(0, 0, 0);
    auto time = t.addSecs(seconds);
    return time.toString(time.hour() == 0 ? "mm:ss" : "hh:mm:ss");
}

uint64_t currentDateToInt()
{
    const auto str = QDateTime::currentDateTimeUtc().toString("yyyyMMddHHmmss");
    return static_cast<uint64_t>(str.toULongLong());
}

void setMinimumWidth(QLabel* label, const QString& text)
{
    const QString oldText = label->text();
    label->setText(text);
    label->setMinimumWidth(0);
    auto width = label->sizeHint().width();
    label->setText(oldText);

    label->setMinimumWidth(width);
}

QString capitalise(const QString& s)
{
    QStringList parts = s.split(' ', Qt::SkipEmptyParts);

    for(auto& part : parts) {
        part.replace(0, 1, part[0].toUpper());
    }

    return parts.join(" ");
}

QPixmap scaleImage(QPixmap& image, int size)
{
    static const int maximumSize = size;
    static const int scale       = 4 * maximumSize;
    const int width              = image.size().width();
    const int height             = image.size().height();
    if(width > maximumSize || height > maximumSize) {
        return image.scaled(scale, scale)
            .scaled(maximumSize, maximumSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

QPixmap changePixmapColour(const QPixmap& orig, const QColor& color)
{
    QPixmap pixmap{orig.size()};
    pixmap.fill(color);
    pixmap.setMask(orig.createMaskFromColor(Qt::transparent));
    return pixmap;
}

void showMessageBox(const QString& text, const QString& infoText)
{
    QMessageBox message;
    message.setText(text);
    message.setInformativeText(infoText);
    message.exec();
}

} // namespace Fy::Utils
