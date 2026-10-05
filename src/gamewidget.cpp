#include "gamewidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QDebug>
#include <cmath>
#include <QApplication>

GameWidget::GameWidget(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(1000, 600);

    QTimer::singleShot(100, this, [this]{ setFocus(); });

    game_ = std::make_unique<Game>();
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &GameWidget::on_frame);
    timer_->start(33);
    clock_.start();
}

GameWidget::~GameWidget() {
    if (game_) game_->stop();
}

void GameWidget::start_game(GameMode mode) {
    game_mode_ = mode;
    if (game_) game_->stop();
    game_ = std::make_unique<Game>();
    game_->setup(mode == GameMode::Multiplayer);
    game_->start();
    timer_->setInterval(16);

    // КРИТИЧЕСКИ ВАЖНО: возвращаем фокус клавиатуры после перезапуска игры
    setFocus(Qt::OtherFocusReason);
}

void GameWidget::close_app() {
    if (game_) game_->stop();
    QApplication::quit();
}

void GameWidget::update_input_flags() {
    // Игрок 1 (WASD + Space)
    p1_w_ = pressed_keys_.contains(Qt::Key_W);
    p1_s_ = pressed_keys_.contains(Qt::Key_S);
    p1_a_ = pressed_keys_.contains(Qt::Key_A);
    p1_d_ = pressed_keys_.contains(Qt::Key_D);
    p1_fire_ = pressed_keys_.contains(Qt::Key_Space);

    // Игрок 2 (Стрелки, PgUp, Home и т.д.)
    p2_up_ = pressed_keys_.contains(Qt::Key_Up) || pressed_keys_.contains(Qt::Key_PageUp) || pressed_keys_.contains(Qt::Key_Home);
    p2_down_ = pressed_keys_.contains(Qt::Key_Down) || pressed_keys_.contains(Qt::Key_PageDown) || pressed_keys_.contains(Qt::Key_End);
    p2_left_ = pressed_keys_.contains(Qt::Key_Left);
    p2_right_ = pressed_keys_.contains(Qt::Key_Right);
}

void GameWidget::update_camera() {
    auto snap = game_->snapshot();
    if (game_mode_ == GameMode::Multiplayer) {
        float sum_x = 0, sum_y = 0;
        int count = 0;
        for (auto& t : snap.tanks) {
            if (t.is_player && t.alive) {
                sum_x += t.x;
                sum_y += t.y;
                count++;
            }
        }
        if (count > 0) {
            cam_x_ = (sum_x / count) - view_w_cells_ / 2.0f;
            cam_y_ = (sum_y / count) - view_h_cells_ / 2.0f;
        }
    } else {
        for (auto& t : snap.tanks) {
            if (t.is_player && t.alive) {
                cam_x_ = t.x - view_w_cells_ / 2.0f;
                cam_y_ = t.y - view_h_cells_ / 2.0f;
                break;
            }
        }
    }
    cam_x_ = std::max(0.0f, std::min(cam_x_, float(cfg::MAP_W - view_w_cells_)));
    cam_y_ = std::max(0.0f, std::min(cam_y_, float(cfg::MAP_H - view_h_cells_)));
}

QPointF GameWidget::cell_to_screen(float x, float y) const {
    float sx = (x - cam_x_) * cfg::TILE_SIZE;
    float sy = (y - cam_y_) * cfg::TILE_SIZE;
    return QPointF(sx, sy);
}

void GameWidget::on_frame() {
    if (game_mode_ != GameMode::Menu) {
        update_camera();
        update_player_input();
    }
    update();
    explosion_pulse_ += 0.15f;
}

void GameWidget::update_player_input() {
    if (game_mode_ == GameMode::Menu || game_mode_ == GameMode::Paused) return;

    if (game_mode_ == GameMode::SinglePlayer) {
        float dx = 0, dy = 0;
        if (p1_w_) dy -= 1;
        if (p1_s_) dy += 1;
        if (p1_a_) dx -= 1;
        if (p1_d_) dx += 1;

        float wx = cam_x_ + mouse_world_x_ / cfg::TILE_SIZE;
        float wy = cam_y_ + mouse_world_y_ / cfg::TILE_SIZE;

        auto snap = game_->snapshot();
        float px = 0, py = 0;
        for (auto& t : snap.tanks) {
            if (t.is_player) { px = t.x; py = t.y; break; }
        }
        float aim = std::atan2(wy - py, wx - px);
        game_->set_player_input(0, dx, dy, aim, p1_fire_);
    } else if (game_mode_ == GameMode::Multiplayer) {
        // Игрок 1
        float dx1 = 0, dy1 = 0;
        if (p1_w_) dy1 -= 1;
        if (p1_s_) dy1 += 1;
        if (p1_a_) dx1 -= 1;
        if (p1_d_) dx1 += 1;
        float aim1 = (dx1 != 0 || dy1 != 0) ? std::atan2(dy1, dx1) : last_aim1_;
        last_aim1_ = aim1;
        game_->set_player_input(0, dx1, dy1, aim1, p1_fire_);

        // Игрок 2
        float dx2 = 0, dy2 = 0;
        if (p2_up_) dy2 -= 1;
        if (p2_down_) dy2 += 1;
        if (p2_left_) dx2 -= 1;
        if (p2_right_) dx2 += 1;
        float aim2 = (dx2 != 0 || dy2 != 0) ? std::atan2(dy2, dx2) : last_aim2_;
        last_aim2_ = aim2;
        game_->set_player_input(1, dx2, dy2, aim2, p2_fire_);
    }
}

void GameWidget::keyPressEvent(QKeyEvent* e) {
    if (game_mode_ == GameMode::Paused && e->key() == Qt::Key_Escape) {
        game_mode_ = saved_game_mode_;
        game_->set_paused(false);
        e->accept();
        return;
    }
    if (e->key() == Qt::Key_Escape && (game_mode_ == GameMode::SinglePlayer || game_mode_ == GameMode::Multiplayer)) {
        saved_game_mode_ = game_mode_;
        game_mode_ = GameMode::Paused;
        game_->set_paused(true);
        e->accept();
        return;
    }

    pressed_keys_.insert(e->key());
    update_input_flags();
    e->accept();
}

void GameWidget::keyReleaseEvent(QKeyEvent* e) {
    // Просто удаляем клавишу из набора. Никаких isAutoRepeat() проверок,
    // которые вызывают залипание при быстром нажатии.
    pressed_keys_.remove(e->key());
    update_input_flags();
    e->accept();
}

void GameWidget::mousePressEvent(QMouseEvent* e) {
    // КРИТИЧЕСКИ ВАЖНО: возвращаем фокус при любом клике, чтобы клавиши не "пропадали"
    setFocus(Qt::MouseFocusReason);

    if (game_mode_ == GameMode::Menu) {
        int W = width(), H = height();
        QRect btn1(W/2 - 150, H/2 - 20, 300, 50);
        QRect btn2(W/2 - 150, H/2 + 50, 300, 50);
        QRect btn3(W/2 - 150, H/2 + 120, 300, 50);
        if (btn1.contains(e->pos())) start_game(GameMode::SinglePlayer);
        else if (btn2.contains(e->pos())) start_game(GameMode::Multiplayer);
        else if (btn3.contains(e->pos())) close_app();
        return;
    }
    if (game_mode_ == GameMode::Paused) {
        int W = width(), H = height();
        QRect btn1(W/2 - 150, H/2 + 20, 300, 50);
        QRect btn2(W/2 - 150, H/2 + 90, 300, 50);
        QRect btn3(W/2 - 150, H/2 + 160, 300, 50);
        if (btn1.contains(e->pos())) {
            game_mode_ = saved_game_mode_;
            game_->set_paused(false);
        } else if (btn2.contains(e->pos())) {
            start_game(saved_game_mode_);
        } else if (btn3.contains(e->pos())) {
            game_mode_ = GameMode::Menu;
            game_->stop();
        }
        return;
    }

    // Обработка Game Over
    auto snap = game_->snapshot();
    if (snap.game_over) {
        int W = width(), H = height();
        QRect btnNew(W/2 - 150, H/2 + 60, 300, 50);
        QRect btnMenu(W/2 - 150, H/2 + 130, 300, 50);
        if (btnNew.contains(e->pos())) {
            start_game(game_mode_);
        } else if (btnMenu.contains(e->pos())) {
            game_mode_ = GameMode::Menu;
            game_->stop();
        }
        return;
    }

    if (e->button() == Qt::LeftButton) {
        if (game_mode_ == GameMode::Multiplayer) p2_fire_ = true;
        else p1_fire_ = true;
    }
    if (e->button() == Qt::RightButton) {
        p1_fire_ = true;
    }
}

void GameWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (game_mode_ == GameMode::Multiplayer && e->button() == Qt::LeftButton) {
        p2_fire_ = false;
    } else if (e->button() == Qt::LeftButton || e->button() == Qt::RightButton) {
        p1_fire_ = false;
    }
}

void GameWidget::mouseMoveEvent(QMouseEvent* e) {
    mouse_world_x_ = float(e->position().x());
    mouse_world_y_ = float(e->position().y());

    int W = width(), H = height();
    if (game_mode_ == GameMode::Menu) {
        QRect btn1(W/2 - 150, H/2 - 20, 300, 50);
        QRect btn2(W/2 - 150, H/2 + 50, 300, 50);
        QRect btn3(W/2 - 150, H/2 + 120, 300, 50);
        if (btn1.contains(e->pos())) menu_hover_ = 0;
        else if (btn2.contains(e->pos())) menu_hover_ = 1;
        else if (btn3.contains(e->pos())) menu_hover_ = 2;
        else menu_hover_ = -1;
        update();
    } else if (game_mode_ == GameMode::Paused) {
        QRect btn1(W/2 - 150, H/2 + 20, 300, 50);
        QRect btn2(W/2 - 150, H/2 + 90, 300, 50);
        QRect btn3(W/2 - 150, H/2 + 160, 300, 50);
        if (btn1.contains(e->pos())) pause_hover_ = 0;
        else if (btn2.contains(e->pos())) pause_hover_ = 1;
        else if (btn3.contains(e->pos())) pause_hover_ = 2;
        else pause_hover_ = -1;
        update();
    } else {
        auto snap = game_->snapshot();
        if (snap.game_over) {
            QRect btnNew(W/2 - 150, H/2 + 60, 300, 50);
            QRect btnMenu(W/2 - 150, H/2 + 130, 300, 50);
            if (btnNew.contains(e->pos())) gameover_hover_ = 0;
            else if (btnMenu.contains(e->pos())) gameover_hover_ = 1;
            else gameover_hover_ = -1;
            update();
        }
    }
}

void GameWidget::focusOutEvent(QFocusEvent* e) {
    // При потере фокуса очищаем ВСЕ нажатые клавиши. Это гарантирует отсутствие залипаний.
    pressed_keys_.clear();
    update_input_flags();
    p2_fire_ = false;
    QWidget::focusOutEvent(e);
}

// ============================================================ PAINT
void GameWidget::paintEvent(QPaintEvent*) {
    if (!hasFocus() && game_mode_ != GameMode::Menu) {
        setFocus(Qt::OtherFocusReason);
    }
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int W = width();
    const int H = height();

    if (game_mode_ == GameMode::Menu) {
        p.fillRect(rect(), QColor(20, 25, 20));
        p.setPen(QColor(255, 255, 255));
        QFont bigFont("Consolas", 42, QFont::Bold);
        p.setFont(bigFont);
        p.drawText(rect(), Qt::AlignCenter | Qt::AlignTop | Qt::TextWordWrap, "STEEL FRONT");

        QFont medFont("Consolas", 20);
        p.setFont(medFont);

        auto drawBtn = [&](const QRect& r, const QString& text, bool hover) {
            p.setBrush(hover ? QColor(80, 180, 80) : QColor(50, 50, 50));
            p.setPen(hover ? QColor(255, 255, 255) : QColor(150, 150, 150));
            p.drawRect(r);
            p.drawText(r, Qt::AlignCenter, text);
        };

        drawBtn(QRect(W/2 - 150, H/2 - 20, 300, 50), "Одиночная игра", menu_hover_ == 0);
        drawBtn(QRect(W/2 - 150, H/2 + 50, 300, 50), "Игра с другом", menu_hover_ == 1);
        drawBtn(QRect(W/2 - 150, H/2 + 120, 300, 50), "Выйти", menu_hover_ == 2);
        return;
    }

    // ----- ОТРИСОВКА ИГРЫ (фон для паузы и геймовера) -----
    p.fillRect(rect(), QColor(60, 90, 55));
    p.setPen(QPen(QColor(50, 75, 48), 1));
    for (int gx = 0; gx <= view_w_cells_ + 1; ++gx) {
        int sx = int(gx * cfg::TILE_SIZE - std::fmod(cam_x_ * cfg::TILE_SIZE, cfg::TILE_SIZE));
        p.drawLine(sx, 0, sx, H);
    }
    for (int gy = 0; gy <= view_h_cells_ + 1; ++gy) {
        int sy = int(gy * cfg::TILE_SIZE - std::fmod(cam_y_ * cfg::TILE_SIZE, cfg::TILE_SIZE));
        p.drawLine(0, sy, W, sy);
    }

    auto snap = game_->snapshot();

    for (auto& b : snap.buildings) {
        QPointF tl = cell_to_screen(float(b.x), float(b.y));
        QRectF r(tl.x(), tl.y(), b.w * cfg::TILE_SIZE, b.h * cfg::TILE_SIZE);
        if (b.destroyed) {
            p.fillRect(r, QColor(50, 40, 30));
            p.setPen(QPen(QColor(30, 25, 20), 2));
            p.drawLine(r.topLeft(), r.bottomRight());
            p.drawLine(r.topRight(), r.bottomLeft());
        } else {
            p.fillRect(r, QColor(120, 90, 60));
            p.setPen(QPen(QColor(80, 60, 40), 2));
            for (int gy = 0; gy < b.h * 2; ++gy) {
                float yy = tl.y() + gy * cfg::TILE_SIZE / 2.0f;
                p.drawLine(int(r.left()), int(yy), int(r.right()), int(yy));
            }
            p.setPen(QPen(QColor(60, 40, 25), 3));
            p.drawRect(r);
        }
    }

    for (auto& e : snap.explosions) {
        QPointF c = cell_to_screen(e.x, e.y);
        float r = e.radius * cfg::TILE_SIZE * (0.7f + 0.3f * std::sin(explosion_pulse_ * 5.0f));
        float alpha = std::max(0.0f, std::min(1.0f, e.ttl / 0.6f));
        p.setBrush(QColor(255, 140, 30, int(200 * alpha)));
        p.setPen(Qt::NoPen);
        p.drawEllipse(c, r, r);
        p.setBrush(QColor(255, 240, 120, int(220 * alpha)));
        p.drawEllipse(c, r * 0.55f, r * 0.55f);
        p.setBrush(QColor(255, 255, 255, int(180 * alpha)));
        p.drawEllipse(c, r * 0.25f, r * 0.25f);
    }

    for (auto& s : snap.shells) {
        QPointF c = cell_to_screen(s.x, s.y);
        QColor col = (s.type == ShellType::AP) ? QColor(255, 240, 150) :
                         (s.type == ShellType::HE) ? QColor(255, 130, 60) : QColor(255, 90, 200);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawEllipse(c, 4, 4);
        p.setBrush(QColor(255, 255, 255, 180));
        p.drawEllipse(c, 2, 2);
    }

    for (auto& t : snap.tanks) {
        if (!t.alive) continue;
        QPointF c = cell_to_screen(t.x, t.y);
        QColor body = (t.team == Team::Red) ? (t.is_player ? QColor(80, 200, 80) : QColor(60, 140, 60)) : QColor(180, 60, 60);
        QColor dark = body.darker(160);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 60));
        p.drawRect(QRectF(c.x() - 12, c.y() - 10, 24, 22));

        QRectF bodyRect(c.x() - 12, c.y() - 10, 24, 20);
        p.setBrush(body);
        p.setPen(QPen(dark, 2));
        p.drawRect(bodyRect);

        p.setBrush(dark);
        p.drawRect(QRectF(bodyRect.left(), bodyRect.top() - 3, 24, 3));
        p.drawRect(QRectF(bodyRect.left(), bodyRect.bottom(), 24, 3));

        float angle = t.angle;
        QPointF muzzle = c + QPointF(std::cos(angle) * 22.0f, std::sin(angle) * 22.0f);
        p.setPen(QPen(QColor(40, 40, 40), 4));
        p.drawLine(c, muzzle);
        p.setPen(QPen(dark, 2));
        p.setBrush(body.lighter(120));
        p.drawEllipse(c, 6, 6);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(30, 30, 30));
        p.drawEllipse(c, 2, 2);

        float hp_frac = float(t.hp) / float(t.max_hp);
        QRectF hpBg(c.x() - 14, c.y() - 20, 28, 4);
        QRectF hpFg(c.x() - 14, c.y() - 20, 28 * hp_frac, 4);
        p.setBrush(QColor(30, 30, 30, 200));
        p.drawRect(hpBg);
        QColor hpCol = (t.is_player) ? QColor(80, 220, 80) : QColor(220, 80, 80);
        if (hp_frac < 0.3f) hpCol = QColor(230, 100, 30);
        p.setBrush(hpCol);
        p.drawRect(hpFg);

        if (t.is_player) {
            p.setPen(QPen(QColor(255, 255, 0), 2));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, 16, 16);
        }
    }

    p.setBrush(QColor(0, 0, 0, 180));
    p.setPen(Qt::NoPen);
    p.drawRect(QRect(0, 0, W, 30));
    QFont font("Consolas", 11, QFont::Bold);
    p.setFont(font);
    p.setPen(QColor(220, 220, 220));
    QString hud = QString("HP: %1/%2    RED: %3    BLUE: %4    WIND: %5")
                      .arg(snap.tanks.empty() ? 0 : snap.tanks[0].hp)
                      .arg(snap.tanks.empty() ? 0 : snap.tanks[0].max_hp)
                      .arg(snap.red_alive)
                      .arg(snap.blue_alive)
                      .arg(snap.wind_x, 0, 'f', 2);
    p.drawText(10, 20, hud);

    const int mm_w = 120, mm_h = 60;
    int mm_x = W - mm_w - 10, mm_y = 10;
    p.setBrush(QColor(0, 0, 0, 160));
    p.drawRect(mm_x, mm_y, mm_w, mm_h);
    p.setPen(QPen(QColor(100, 100, 100), 1));
    p.drawRect(mm_x, mm_y, mm_w, mm_h);
    for (auto& t : snap.tanks) {
        if (!t.alive) continue;
        int px = mm_x + int(t.x / cfg::MAP_W * mm_w);
        int py = mm_y + int(t.y / cfg::MAP_H * mm_h);
        p.setPen(Qt::NoPen);
        if (t.is_player) p.setBrush(QColor(80, 220, 80));
        else if (t.team == Team::Red) p.setBrush(QColor(200, 100, 100));
        else p.setBrush(QColor(100, 100, 220));
        p.drawRect(px, py, 2, 2);
    }

    p.setPen(QColor(180, 180, 180, 200));
    p.setFont(QFont("Consolas", 9));
    QString controls = (game_mode_ == GameMode::Multiplayer)
                           ? "Игрок 1: WASD+Space | Игрок 2: Стрелки+ЛКМ | Esc: Пауза"
                           : "WASD — движение, мышь — прицел, ЛКМ/Space — огонь, Esc — пауза";
    p.drawText(10, H - 10, controls);

    // ----- ЭКРАН ОКОНЧАНИЯ ИГРЫ -----
    if (snap.game_over) {
        p.setBrush(QColor(0, 0, 0, 200));
        p.drawRect(rect());

        QFont big("Consolas", 32, QFont::Bold);
        p.setFont(big);

        QString txt;
        QColor col;
        if (game_mode_ == GameMode::Multiplayer) {
            txt = snap.player_won ? "ИГРОК 1 (Красный) ПОБЕДИЛ!" : "ИГРОК 2 (Синий) ПОБЕДИЛ!";
            col = snap.player_won ? QColor(80, 220, 80) : QColor(100, 100, 220);
        } else {
            txt = snap.player_won ? "ПОБЕДА!" : "ПОРАЖЕНИЕ";
            col = snap.player_won ? QColor(80, 220, 80) : QColor(220, 80, 80);
        }

        p.setPen(col);
        QFontMetrics fm(big);
        int tw = fm.horizontalAdvance(txt);
        p.drawText((W - tw) / 2, H / 2 - 20, txt);

        // Кнопки
        QFont med("Consolas", 20);
        p.setFont(med);
        auto drawBtn = [&](const QRect& r, const QString& text, bool hover) {
            p.setBrush(hover ? QColor(80, 180, 80) : QColor(50, 50, 50));
            p.setPen(hover ? QColor(255, 255, 255) : QColor(150, 150, 150));
            p.drawRect(r);
            p.drawText(r, Qt::AlignCenter, text);
        };
        drawBtn(QRect(W/2 - 150, H/2 + 60, 300, 50), "Новая игра", gameover_hover_ == 0);
        drawBtn(QRect(W/2 - 150, H/2 + 130, 300, 50), "Выйти в меню", gameover_hover_ == 1);
    }

    // ----- МЕНЮ ПАУЗЫ -----
    if (game_mode_ == GameMode::Paused) {
        p.fillRect(rect(), QColor(0, 0, 0, 180));
        p.setPen(QColor(255, 255, 255));
        QFont bigFont("Consolas", 36, QFont::Bold);
        p.setFont(bigFont);
        p.drawText(rect(), Qt::AlignCenter | Qt::AlignTop | Qt::TextWordWrap, "ПАУЗА");

        QFont medFont("Consolas", 20);
        p.setFont(medFont);
        auto drawBtn = [&](const QRect& r, const QString& text, bool hover) {
            p.setBrush(hover ? QColor(80, 180, 80) : QColor(50, 50, 50));
            p.setPen(hover ? QColor(255, 255, 255) : QColor(150, 150, 150));
            p.drawRect(r);
            p.drawText(r, Qt::AlignCenter, text);
        };
        drawBtn(QRect(W/2 - 150, H/2 + 20, 300, 50), "Возобновить (Esc)", pause_hover_ == 0);
        drawBtn(QRect(W/2 - 150, H/2 + 90, 300, 50), "Новая игра", pause_hover_ == 1);
        drawBtn(QRect(W/2 - 150, H/2 + 160, 300, 50), "Выйти в меню", pause_hover_ == 2);
    }
}
