#pragma once
#include <QMainWindow>

class GameWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
protected:
    void showEvent(QShowEvent* e) override;
private:
    GameWidget* game_ = nullptr;
};
