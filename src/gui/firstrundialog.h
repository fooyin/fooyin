#pragma once

#include <QDialog>
#include <QHBoxLayout>
#include <QListWidget>

class FirstRunDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FirstRunDialog(QWidget* parent = nullptr);
    ~FirstRunDialog() override;

signals:
    void layoutChanged(const QByteArray& layout);

protected:
    void setupList();
    void changeLayout();

private:
    QHBoxLayout m_layout;
    QListWidget m_layoutList;
};
