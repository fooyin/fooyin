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

void FirstRunDialog::setupList()
{
    m_layoutList.setSelectionMode(QAbstractItemView::SingleSelection);

    auto* emptyLayout = new QListWidgetItem("Empty", &m_layoutList);
    auto empty = QString("eyJMYXlvdXQiOnsiU3BsaXR0ZXIiOnsiQ2hpbGRyZW4iOlsiRHVtbXkiXSwiU3RhdGUiOiJBQUFBL3dBQUFBRUFBQUFCQ"
                         "UFBQUZBRC8vLy8vQVFBQUFBSUEiLCJUeXBlIjoiVmVydGljYWwifX19")
                     .toUtf8();
    emptyLayout->setData(LayoutRole::Type, empty);
    auto* fullLayout = new QListWidgetItem("Full", &m_layoutList);
    auto full = QString("eyJMYXlvdXQiOnsiU3BsaXR0ZXIiOnsiQ2hpbGRyZW4iOlsiU3RhdHVzIiwiU2VhcmNoIix7IlNwbGl0dGVyIjp7IkN"
                        "oaWxkcmVuIjpbeyJGaWx0ZXIiOnsiVHlwZSI6IkFsYnVtQXJ0aXN0In19LCJQbGF5bGlzdCJdLCJTdGF0ZSI6IkFBQU"
                        "Evd0FBQUFFQUFBQUNBQUFCUXdBQUJoOEEvLy8vL3dFQUFBQUJBQT09IiwiVHlwZSI6Ikhvcml6b250YWwifX0sIkNvb"
                        "nRyb2xzIl0sIlN0YXRlIjoiQUFBQS93QUFBQUVBQUFBRUFBQUFHUUFBQUI0QUFBT05BQUFBRWdELy8vLy9BUUFBQUFJ"
                        "QSIsIlR5cGUiOiJWZXJ0aWNhbCJ9fX0=")
                    .toUtf8();
    fullLayout->setData(LayoutRole::Type, full);

    m_layoutList.addItem(emptyLayout);
    m_layoutList.addItem(fullLayout);
}

void FirstRunDialog::changeLayout()
{
    const auto selectedItem = m_layoutList.selectionModel()->selectedRows().constFirst();
    const auto layout = QByteArray::fromBase64(selectedItem.data(LayoutRole::Type).toByteArray());

    emit layoutChanged(layout);
}
