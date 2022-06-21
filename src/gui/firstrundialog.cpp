#include "firstrundialog.h"

#include "utils/typedefs.h"

FirstRunDialog::FirstRunDialog(QWidget* parent)
    : QDialog{parent}
    , m_layout{this}
{
    setObjectName("Setup");
    setWindowTitle("Setup");
    setupList();
    connect(&m_layoutList, &QListWidget::itemSelectionChanged, this, &FirstRunDialog::changeLayout);
    m_layout.addWidget(&m_layoutList);
}

FirstRunDialog::~FirstRunDialog() = default;

// Not the best way to handle layouts. Maybe save to (readable) files?
void FirstRunDialog::setupList()
{
    m_layoutList.setSelectionMode(QAbstractItemView::SingleSelection);

    auto* stoneLayout = new QListWidgetItem("Stone", &m_layoutList);
    auto stone = QString("eyJMYXlvdXQiOnsiU3BsaXR0ZXIiOnsiQ2hpbGRyZW4iOlsiU3RhdHVzIiwiU2VhcmNoIix7IlNwbGl0dGVyIjp7IkN"
                         "oaWxkcmVuIjpbeyJGaWx0ZXIiOnsiVHlwZSI6IkFsYnVtQXJ0aXN0In19LCJQbGF5bGlzdCJdLCJTdGF0ZSI6IkFBQU"
                         "Evd0FBQUFFQUFBQUNBQUFCUXdBQUJoOEEvLy8vL3dFQUFBQUJBQT09IiwiVHlwZSI6Ikhvcml6b250YWwifX0sIkNvb"
                         "nRyb2xzIl0sIlN0YXRlIjoiQUFBQS93QUFBQUVBQUFBRUFBQUFHUUFBQUI0QUFBT05BQUFBRWdELy8vLy9BUUFBQUFJ"
                         "QSIsIlR5cGUiOiJWZXJ0aWNhbCJ9fX0=")
                     .toUtf8();
    stoneLayout->setData(LayoutRole::Type, stone);

    auto* emberLayout = new QListWidgetItem("Ember", &m_layoutList);
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

    auto* customLayout = new QListWidgetItem("Custom", &m_layoutList);
    auto custom
        = QString("eyJMYXlvdXQiOnsiU3BsaXR0ZXIiOnsiQ2hpbGRyZW4iOlsiRHVtbXkiXSwiU3RhdGUiOiJBQUFBL3dBQUFBRUFBQUFCQ"
                  "UFBQUZBRC8vLy8vQVFBQUFBSUEiLCJUeXBlIjoiVmVydGljYWwifX19")
              .toUtf8();
    customLayout->setData(LayoutRole::Type, custom);

    m_layoutList.addItem(emberLayout);
    m_layoutList.addItem(customLayout);
}

void FirstRunDialog::changeLayout()
{
    const auto selectedItem = m_layoutList.selectionModel()->selectedRows().constFirst();
    const auto layout = QByteArray::fromBase64(selectedItem.data(LayoutRole::Type).toByteArray());

    emit layoutChanged(layout);
}
