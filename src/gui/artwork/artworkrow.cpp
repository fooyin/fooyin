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

#include "artworkrow.h"

#include <gui/guiconstants.h>
#include <gui/widgets/overlaywidget.h>
#include <utils/utils.h>

#include <QBuffer>
#include <QContextMenuEvent>
#include <QCryptographicHash>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QMenu>
#include <QMimeDatabase>
#include <QPushButton>

using namespace Qt::StringLiterals;

constexpr auto ImageSize = 150;

namespace Fooyin {
ArtworkRow::ArtworkRow(const QString& name, Track::Cover cover, bool readOnly, QWidget* parent)
    : QWidget{parent}
    , m_type{cover}
    , m_name{new QLabel(name, parent)}
    , m_image{new QLabel(parent)}
    , m_details{new QLabel(parent)}
    , m_addButton{new QPushButton(Utils::iconFromTheme(Constants::Icons::Add), tr("Add"), parent)}
    , m_removeButton{new QPushButton(Utils::iconFromTheme(Constants::Icons::Remove), tr("Remove"), parent)}
    , m_readOnly{readOnly}
    , m_status{Status::None}
    , m_multipleImages{false}
    , m_addedCount{0}
    , m_imageCount{0}
{
    m_image->setFixedSize(ImageSize, ImageSize);
    m_addButton->setFixedSize(ImageSize, ImageSize);
    m_removeButton->setFixedHeight(ImageSize);
    m_removeButton->setFlat(true);
    m_addButton->setDisabled(m_readOnly);
    m_removeButton->setDisabled(m_readOnly);

    m_name->setMinimumWidth(fontMetrics().horizontalAdvance(u" Front Cover"_s));
    m_details->setMinimumWidth(fontMetrics().horizontalAdvance(u"No artwork present"_s));
    m_image->setAlignment(Qt::AlignCenter);

    m_image->hide();
    m_image->setPixmap({});
    m_addButton->show();
    m_details->setText(tr("No artwork present"));
    m_removeButton->hide();

    auto* layout = new QHBoxLayout(this);
    layout->addWidget(m_name);
    layout->addWidget(m_image);
    layout->addWidget(m_addButton);
    layout->addWidget(m_details);
    layout->addWidget(m_removeButton);
    layout->addStretch();

    QObject::connect(m_addButton, &QPushButton::clicked, this, &ArtworkRow::replaceImage);
    QObject::connect(m_removeButton, &QPushButton::clicked, this, &ArtworkRow::removeImage);
}

void ArtworkRow::replaceImage()
{
    const QString filepath
        = QFileDialog::getOpenFileName(this, tr("Open Image"), QDir::homePath(), tr("Images") + " (*.png *.jpg)"_L1,
                                       nullptr, QFileDialog::DontResolveSymlinks);
    if(filepath.isEmpty()) {
        return;
    }

    QFile file{filepath};
    if(!file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QByteArray imageData = file.readAll();

    m_multipleImages = false;
    m_status         = (m_imageData.isEmpty() ? Status::Added : Status::Changed);
    loadImage(imageData, true);
    finalise(0);
}

void ArtworkRow::removeImage()
{
    m_image->hide();
    m_image->setPixmap({});
    m_addButton->show();
    m_removeButton->hide();
    m_details->setText(tr("No artwork present"));
    m_status = (m_status == Status::Added || m_status == Status::Changed ? Status::None : Status::Removed);
    m_imageData.clear();
}

void ArtworkRow::loadImage(const QByteArray& imageData, bool replace)
{
    if(imageData.isEmpty()) {
        if(!replace && !m_imageHash.isEmpty()) {
            m_multipleImages = true;
        }
        ++m_addedCount;
        return;
    }

    if(m_addedCount > 0 && m_imageHash.isEmpty()) {
        m_multipleImages = true;
    }

    if(!replace) {
        ++m_imageCount;
    }

    ++m_addedCount;

    if(m_multipleImages) {
        return;
    }

    QCryptographicHash hash{QCryptographicHash::Sha256};
    hash.addData(imageData);
    const QByteArray imageHash = hash.result();

    if(!replace && !m_imageHash.isEmpty() && m_imageHash != imageHash) {
        m_multipleImages = true;
        return;
    }

    m_imageData = imageData;
    m_imageHash = imageHash;

    const QMimeDatabase mimeDb;
    m_mimeType = mimeDb.mimeTypeForData(m_imageData).name();
}

void ArtworkRow::finalise(int trackCount)
{
    m_addedCount = 0;

    if(m_multipleImages) {
        m_image->setText(tr("Multiple images"));
        m_details->setText(tr("%1 of %2 files have artwork").arg(m_imageCount).arg(trackCount));
        m_image->show();
        m_addButton->hide();
        m_removeButton->show();
    }
    else if(!m_imageData.isEmpty()) {
        QBuffer buffer{&m_imageData};
        const QMimeDatabase mimeDb;
        const auto mimeType   = mimeDb.mimeTypeForData(&buffer);
        const auto formatHint = mimeType.preferredSuffix().toLocal8Bit().toLower();

        QImageReader reader{&buffer, formatHint};

        if(!reader.canRead()) {
            reader.setFormat({});
            reader.setDevice(&buffer);
            if(!reader.canRead()) {
                return;
            }
        }

        if(reader.canRead()) {
            reader.setScaledSize(m_image->size());
            m_image->setPixmap(QPixmap::fromImageReader(&reader));
            m_image->show();
            m_addButton->hide();
            m_removeButton->show();

            const auto size          = reader.size();
            const QString format     = mimeType.comment();
            const qint64 sizeInKB    = m_imageData.size() / 1024;
            const QString resolution = u"%1x%2"_s.arg(size.width()).arg(size.height());
            m_details->setText(u"%1 KB\n%2\n%3"_s.arg(sizeInKB).arg(resolution, format));
        }
        else {
            m_image->hide();
            m_addButton->show();
            m_details->clear();
        }
    }
}

void ArtworkRow::reset()
{
    m_status = ArtworkRow::Status::None;
}

Track::Cover ArtworkRow::type() const
{
    return m_type;
}

ArtworkRow::Status ArtworkRow::status() const
{
    return m_status;
}

QString ArtworkRow::mimeType() const
{
    return m_mimeType;
}

QByteArray ArtworkRow::image() const
{
    return m_imageData;
}

void ArtworkRow::contextMenuEvent(QContextMenuEvent* event)
{
    if(m_readOnly) {
        return;
    }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* overlay = new OverlayWidget(parentWidget());
    QObject::connect(menu, &QMenu::aboutToHide, overlay, &QObject::deleteLater);
    overlay->setGeometry(geometry());
    overlay->raise();
    overlay->show();

    const bool hasExistingImage = m_image->isVisible();

    auto* add = new QAction(hasExistingImage ? tr("Replace image") : tr("Add image"), menu);
    QObject::connect(add, &QAction::triggered, this, &ArtworkRow::replaceImage);

    menu->addAction(add);

    if(hasExistingImage) {
        auto* remove = new QAction(tr("Remove"), menu);
        QObject::connect(remove, &QAction::triggered, this, &ArtworkRow::removeImage);
        menu->addAction(remove);
    }

    menu->popup(event->globalPos());
}
} // namespace Fooyin

#include "moc_artworkrow.cpp"
