#pragma once

#include <QDialog>
#include <QHBoxLayout>
#include <QListWidget>

class QuickSeupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QuickSeupDialog(QWidget* parent = nullptr);
    ~QuickSeupDialog() override;

signals:
    void layoutChanged(const QByteArray& layout);

protected:
    void setupList();
    void changeLayout();

private:
    QHBoxLayout m_layout;
    QListWidget m_layoutList;
};
