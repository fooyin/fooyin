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

#include "quicksetupdialog.h"

#include <core/typedefs.h>

#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace Gui {
QuickSetupDialog::QuickSetupDialog(QWidget* parent)
    : QDialog{parent}
    , m_layout{new QVBoxLayout(this)}
    , m_layoutList{new QListWidget(this)}
    , m_accept{new QPushButton("OK", this)}
{
    setObjectName("Quick Setup");
    setWindowTitle("Quick Setup");

    setupUi();
    setupList();

    connect(m_layoutList, &QListWidget::itemSelectionChanged, this, &QuickSetupDialog::changeLayout);
    connect(m_accept, &QPushButton::pressed, this, &QuickSetupDialog::close);
}

void QuickSetupDialog::setupUi()
{
    m_layout->addWidget(m_layoutList);
    m_layout->addWidget(m_accept);
}

QuickSetupDialog::~QuickSetupDialog() = default;

// Not the best way to handle layouts. Maybe save to (readable) files?
void QuickSetupDialog::setupList()
{
    m_layoutList->setSelectionMode(QAbstractItemView::SingleSelection);

    auto* emptyLayout = new QListWidgetItem("Empty", m_layoutList);
    auto empty = QString("eyJMYXlvdXQiOnsiU3BsaXR0ZXIiOnsiQ2hpbGRyZW4iOlsiRHVtbXkiXSwiU3RhdGUiOiJBQUFBL3dBQUFBRUFBQUFCQ"
                         "UFBQUZBRC8vLy8vQVFBQUFBSUEiLCJUeXBlIjoiVmVydGljYWwifX19")
                     .toUtf8();
    emptyLayout->setData(Core::LayoutRole::Type, empty);

    auto* simpleLayout = new QListWidgetItem("Simple", m_layoutList);
    auto simple
        = QString("eyJMYXlvdXQiOnsiU3BsaXR0ZXIiOnsiQ2hpbGRyZW4iOlsiU3RhdHVzIiwiU2VhcmNoIiwiUGxheWxpc3QiLCJDb250c"
                  "m9scyJdLCJTdGF0ZSI6IkFBQUEvd0FBQUFFQUFBQUVBQUFBR1FBQUFCNEFBQU9oQUFBQUVnRC8vLy8vQVFBQUFBSUEiLC"
                  "JUeXBlIjoiVmVydGljYWwifX19")
              .toUtf8();
    simpleLayout->setData(Core::LayoutRole::Type, simple);

    auto* stoneLayout = new QListWidgetItem("Stone", m_layoutList);
    auto stone = QString("eyJMYXlvdXQiOnsiU3BsaXR0ZXIiOnsiQ2hpbGRyZW4iOlsiU3RhdHVzIiwiU2VhcmNoIix7IlNwbGl0dGVyIjp7IkN"
                         "oaWxkcmVuIjpbeyJGaWx0ZXIiOnsiVHlwZSI6IkFsYnVtQXJ0aXN0In19LCJQbGF5bGlzdCJdLCJTdGF0ZSI6IkFBQU"
                         "Evd0FBQUFFQUFBQUNBQUFCUXdBQUJoOEEvLy8vL3dFQUFBQUJBQT09IiwiVHlwZSI6Ikhvcml6b250YWwifX0sIkNvb"
                         "nRyb2xzIl0sIlN0YXRlIjoiQUFBQS93QUFBQUVBQUFBRUFBQUFHUUFBQUI0QUFBT05BQUFBRWdELy8vLy9BUUFBQUFJ"
                         "QSIsIlR5cGUiOiJWZXJ0aWNhbCJ9fX0=")
                     .toUtf8();
    stoneLayout->setData(Core::LayoutRole::Type, stone);

    auto* visionLayout = new QListWidgetItem("Vision", m_layoutList);
    auto vision = QString("eyJMYXlvdXQiOnsiU3BsaXR0ZXIiOnsiQ2hpbGRyZW4iOlsiU3RhdHVzIiwiU2VhcmNoIix7IlNwbGl0dGVyIjp7IkNo"
                          "aWxkcmVuIjpbIkFydHdvcmsiLCJQbGF5bGlzdCJdLCJTdGF0ZSI6IkFBQUEvd0FBQUFFQUFBQUNBQUFDY1FBQUFvRUEv"
                          "Ly8vL3dFQUFBQUJBQT09IiwiVHlwZSI6Ikhvcml6b250YWwifX0sIkNvbnRyb2xzIl0sIlN0YXRlIjoiQUFBQS93QUFB"
                          "QUVBQUFBRUFBQUFHUUFBQUI0QUFBT2hBQUFBRWdELy8vLy9BUUFBQUFJQSIsIlR5cGUiOiJWZXJ0aWNhbCJ9fX0=")
                      .toUtf8();
    visionLayout->setData(Core::LayoutRole::Type, vision);

    auto* emberLayout = new QListWidgetItem("Ember", m_layoutList);
    auto ember
        = QString(
              "eyJMYXlvdXQiOnsiU3BsaXR0ZXIiOnsiQ2hpbGRyZW4iOlsiU3RhdHVzIix7IlNwbGl0dGVyIjp7IkNoaWxkcmVuIjpbeyJGaWx0ZXIi"
              "OnsiVHlwZSI6IkdlbnJlIn19LHsiRmlsdGVyIjp7IlR5cGUiOiJBbGJ1bUFydGlzdCJ9fSx7IkZpbHRlciI6eyJUeXBlIjoiQXJ0aXN0"
              "In19LHsiRmlsdGVyIjp7IlR5cGUiOiJBbGJ1bSJ9fV0sIlN0YXRlIjoiQUFBQS93QUFBQUVBQUFBRUFBQUJMQUFBQVRRQUFBRTZBQUFC"
              "VUFELy8vLy9BUUFBQUFFQSIsIlR5cGUiOiJIb3Jpem9udGFsIn19LHsiU3BsaXR0ZXIiOnsiQ2hpbGRyZW4iOlt7IlNwbGl0dGVyIjp7"
              "IkNoaWxkcmVuIjpbIkFydHdvcmsiLCJJbmZvIl0sIlN0YXRlIjoiQUFBQS93QUFBQUVBQUFBQ0FBQUJMQUFBQUw4QS8vLy8vd0VBQUFB"
              "Q0FBPT0iLCJUeXBlIjoiVmVydGljYWwifX0sIlBsYXlsaXN0Il0sIlN0YXRlIjoiQUFBQS93QUFBQUVBQUFBQ0FBQUJMQUFBQThZQS8v"
              "Ly8vd0VBQUFBQkFBPT0iLCJUeXBlIjoiSG9yaXpvbnRhbCJ9fSwiQ29udHJvbHMiXSwiU3RhdGUiOiJBQUFBL3dBQUFBRUFBQUFFQUFB"
              "QUdRQUFBS0FBQUFIdkFBQUFFZ0QvLy8vL0FRQUFBQUlBIiwiVHlwZSI6IlZlcnRpY2FsIn19fQ==")
              .toUtf8();
    emberLayout->setData(Core::LayoutRole::Type, ember);
}

void QuickSetupDialog::changeLayout()
{
    const auto selectedItem = m_layoutList->selectionModel()->selectedRows().constFirst();
    const auto layout       = QByteArray::fromBase64(selectedItem.data(Core::LayoutRole::Type).toByteArray());

    emit layoutChanged(layout);
}

void QuickSetupDialog::showEvent(QShowEvent* event)
{
    // Centre to parent widget
    const QRect parentRect{parentWidget()->mapToGlobal(QPoint(0, 0)), parentWidget()->size()};
    move(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), parentRect).topLeft());

    QDialog::showEvent(event);
}
} // namespace Gui
