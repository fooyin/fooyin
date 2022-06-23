#pragma once

#include <QDialog>
#include <QTimer>

class HoverMenu : public QDialog
{
    Q_OBJECT

public:
    explicit HoverMenu(QWidget* parent = nullptr);
    ~HoverMenu() override;

protected:
    void leaveEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

    void closeMenu();

private:
    QTimer m_timer;
};
