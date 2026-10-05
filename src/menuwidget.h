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
    void quit_clicked();
    void single_player_clicked();
    void two_players_clicked();
};
