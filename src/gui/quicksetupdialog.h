#pragma once

#include <QDialog>

class QVBoxLayout;
class QListWidget;
class QPushButton;

class QuickSeupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QuickSeupDialog(QWidget* parent = nullptr);
    ~QuickSeupDialog() override;

signals:
    void layoutChanged(const QByteArray& layout);

protected:
    void setupUi();
    void setupList();
    void changeLayout();

private:
    QVBoxLayout* m_layout;
    QListWidget* m_layoutList;
    QPushButton* m_accept;
};
