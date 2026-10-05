#pragma once
#include <QWidget>
#include <QTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QImage>
#include <QElapsedTimer>
#include <QSet>
#include <memory>
#include "game.h"

enum class GameMode { Menu, SinglePlayer, Multiplayer, Paused };

class GameWidget : public QWidget {
    Q_OBJECT
public:
    explicit GameWidget(QWidget* parent = nullptr);
    ~GameWidget() override;

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

    GameMode game_mode_ = GameMode::Menu;
    GameMode saved_game_mode_ = GameMode::SinglePlayer;

    // Надежный наборcurrently нажатых клавиш (исключает залипание)
    QSet<int> pressed_keys_;

    // Состояние ввода (вычисляется из pressed_keys_)
    bool p1_w_ = false, p1_a_ = false, p1_s_ = false, p1_d_ = false;
    bool p1_fire_ = false;
    bool p2_up_ = false, p2_down_ = false, p2_left_ = false, p2_right_ = false;
    bool p2_fire_ = false; // Для ЛКМ

    float mouse_world_x_ = 0, mouse_world_y_ = 0;
    float last_aim1_ = 0.0f;
    float last_aim2_ = 3.14159f;

    float cam_x_ = 0, cam_y_ = 0;
    int   view_w_cells_ = 60;
    int   view_h_cells_ = 30;

    float explosion_pulse_ = 0;
    int menu_hover_ = -1;
    int pause_hover_ = -1;
    int gameover_hover_ = -1;

    void  update_input_flags();
    void  update_player_input();
    QPointF cell_to_screen(float x, float y) const;
    void  update_camera();
    void  start_game(GameMode mode);
    void  close_app();
};
