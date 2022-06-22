#pragma once

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

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
    QVBoxLayout m_layout;
    QListWidget m_layoutList;
    QPushButton m_accept;
};
