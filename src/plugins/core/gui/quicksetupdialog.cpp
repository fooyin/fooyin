#include "quicksetupdialog.h"

#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <utils/typedefs.h>

QuickSeupDialog::QuickSeupDialog(QWidget* parent)
    : QDialog{parent}
    , m_layout{new QVBoxLayout(this)}
    , m_layoutList{new QListWidget(this)}
    , m_accept{new QPushButton("OK", this)}
{
    setObjectName("Quick Setup");
    setWindowTitle("Quick Setup");

    setupUi();
    setupList();

    connect(m_layoutList, &QListWidget::itemSelectionChanged, this, &QuickSeupDialog::changeLayout);
    connect(m_accept, &QPushButton::pressed, this, &QuickSeupDialog::close);
}

void QuickSeupDialog::setupUi()
{
    m_layout->addWidget(m_layoutList);
    m_layout->addWidget(m_accept);
}

QuickSeupDialog::~QuickSeupDialog() = default;

// Not the best way to handle layouts. Maybe save to (readable) files?
void QuickSeupDialog::setupList()
{
    m_layoutList->setSelectionMode(QAbstractItemView::SingleSelection);

    auto* emptyLayout = new QListWidgetItem("Empty", m_layoutList);
    auto empty = QString("eyJMYXlvdXQiOnsiU3BsaXR0ZXIiOnsiQ2hpbGRyZW4iOlsiRHVtbXkiXSwiU3RhdGUiOiJBQUFBL3dBQUFBRUFBQUFCQ"
                         "UFBQUZBRC8vLy8vQVFBQUFBSUEiLCJUeXBlIjoiVmVydGljYWwifX19")
                     .toUtf8();
    emptyLayout->setData(LayoutRole::Type, empty);

    auto* simpleLayout = new QListWidgetItem("Simple", m_layoutList);
    auto simple
        = QString("eyJMYXlvdXQiOnsiU3BsaXR0ZXIiOnsiQ2hpbGRyZW4iOlsiU3RhdHVzIiwiU2VhcmNoIiwiUGxheWxpc3QiLCJDb250c"
                  "m9scyJdLCJTdGF0ZSI6IkFBQUEvd0FBQUFFQUFBQUVBQUFBR1FBQUFCNEFBQU9oQUFBQUVnRC8vLy8vQVFBQUFBSUEiLC"
                  "JUeXBlIjoiVmVydGljYWwifX19")
              .toUtf8();
    simpleLayout->setData(LayoutRole::Type, simple);

    auto* stoneLayout = new QListWidgetItem("Stone", m_layoutList);
    auto stone = QString("eyJMYXlvdXQiOnsiU3BsaXR0ZXIiOnsiQ2hpbGRyZW4iOlsiU3RhdHVzIiwiU2VhcmNoIix7IlNwbGl0dGVyIjp7IkN"
                         "oaWxkcmVuIjpbeyJGaWx0ZXIiOnsiVHlwZSI6IkFsYnVtQXJ0aXN0In19LCJQbGF5bGlzdCJdLCJTdGF0ZSI6IkFBQU"
                         "Evd0FBQUFFQUFBQUNBQUFCUXdBQUJoOEEvLy8vL3dFQUFBQUJBQT09IiwiVHlwZSI6Ikhvcml6b250YWwifX0sIkNvb"
                         "nRyb2xzIl0sIlN0YXRlIjoiQUFBQS93QUFBQUVBQUFBRUFBQUFHUUFBQUI0QUFBT05BQUFBRWdELy8vLy9BUUFBQUFJ"
                         "QSIsIlR5cGUiOiJWZXJ0aWNhbCJ9fX0=")
                     .toUtf8();
    stoneLayout->setData(LayoutRole::Type, stone);

    auto* visionLayout = new QListWidgetItem("Vision", m_layoutList);
    auto vision = QString("eyJMYXlvdXQiOnsiU3BsaXR0ZXIiOnsiQ2hpbGRyZW4iOlsiU3RhdHVzIiwiU2VhcmNoIix7IlNwbGl0dGVyIjp7IkNo"
                          "aWxkcmVuIjpbIkFydHdvcmsiLCJQbGF5bGlzdCJdLCJTdGF0ZSI6IkFBQUEvd0FBQUFFQUFBQUNBQUFDY1FBQUFvRUEv"
                          "Ly8vL3dFQUFBQUJBQT09IiwiVHlwZSI6Ikhvcml6b250YWwifX0sIkNvbnRyb2xzIl0sIlN0YXRlIjoiQUFBQS93QUFB"
                          "QUVBQUFBRUFBQUFHUUFBQUI0QUFBT2hBQUFBRWdELy8vLy9BUUFBQUFJQSIsIlR5cGUiOiJWZXJ0aWNhbCJ9fX0=")
                      .toUtf8();
    visionLayout->setData(LayoutRole::Type, vision);

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
    emberLayout->setData(LayoutRole::Type, ember);
}

void QuickSeupDialog::changeLayout()
{
    const auto selectedItem = m_layoutList->selectionModel()->selectedRows().constFirst();
    const auto layout = QByteArray::fromBase64(selectedItem.data(LayoutRole::Type).toByteArray());

    emit layoutChanged(layout);
}

void QuickSeupDialog::showEvent(QShowEvent* event)
{
    // Centre to parent widget
    QRect parentRect{parentWidget()->mapToGlobal(QPoint(0, 0)), parentWidget()->size()};
    move(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), parentRect).topLeft());

    QDialog::showEvent(event);
}
