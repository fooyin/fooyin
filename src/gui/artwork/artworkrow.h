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

#pragma once

#include <core/track.h>

#include <QWidget>

class QLabel;
class QPushButton;

namespace Fooyin {
class ArtworkRow : public QWidget
{
    Q_OBJECT

public:
    enum class Status : uint8_t
    {
        None,
        Added,
        Changed,
        Removed
    };

    explicit ArtworkRow(const QString& name, Track::Cover cover, bool readOnly, QWidget* parent);

    void replaceImage();
    void removeImage();

    void loadImage(const QByteArray& imageData, bool replace = false);
    void finalise(int trackCount);
    void reset();

    [[nodiscard]] Track::Cover type() const;
    [[nodiscard]] Status status() const;
    [[nodiscard]] QString mimeType() const;
    [[nodiscard]] QByteArray image() const;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    Track::Cover m_type;
    QLabel* m_name;
    QLabel* m_image;
    QLabel* m_details;
    QPushButton* m_addButton;
    QPushButton* m_removeButton;

    bool m_readOnly;
    Status m_status;
    QByteArray m_imageData;
    QByteArray m_imageHash;
    QString m_mimeType;

    bool m_multipleImages;
    int m_addedCount;
    int m_imageCount;
};
} // namespace Fooyin
