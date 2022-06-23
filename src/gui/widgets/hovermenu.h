#pragma once

#include <QDialog>
#include <QTimer>

class HoverMenu : public QDialog
{
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
