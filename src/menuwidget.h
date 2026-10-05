#pragma once
#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>

class MenuWidget : public QWidget {
    Q_OBJECT
public:
    explicit MenuWidget(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent*) override;

signals:
    void play_clicked();
    void quit_clicked();
};
