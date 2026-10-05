#pragma once
#include <QWidget>
#include <QTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QImage>
#include <QElapsedTimer>
#include <memory>
#include "game.h"

class GameWidget : public QWidget {
    Q_OBJECT

public:
    explicit GameWidget(QWidget* parent = nullptr);
    ~GameWidget() override;
    void restart();

signals:
    void back_to_menu();

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void focusOutEvent(QFocusEvent*) override;

private slots:
    void on_frame();

private:
    std::unique_ptr<Game> game_;
    QTimer*    timer_ = nullptr;
    QElapsedTimer clock_;

    // input state (Qt thread)
    bool key_w_ = false, key_a_ = false, key_s_ = false, key_d_ = false;
    bool fire_ = false;
    float mouse_world_x_ = 0, mouse_world_y_ = 0;

    // virtual viewport
    float cam_x_ = 0, cam_y_ = 0;   // top-left of visible region (in cells)
    int   view_w_cells_ = 60;
    int   view_h_cells_ = 30;

    // animation
    float explosion_pulse_ = 0;

    void  update_player_input();
    QPointF cell_to_screen(float x, float y) const;
    void   update_camera();

    bool paused_ = false;
};
