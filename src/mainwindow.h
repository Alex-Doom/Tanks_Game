#pragma once
#include <QMainWindow>
#include <QStackedWidget>

class QShowEvent;
class QKeyEvent;
class MenuWidget;
class GameWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private slots:
    void show_menu();
    void show_game();
    void start_new_game();

private:
    QStackedWidget* stack_ = nullptr;
    MenuWidget* menu_ = nullptr;
    GameWidget* game_ = nullptr;
    void start_single_player();
    void start_two_players();
};
