#include "gamewidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QDebug>
#include <cmath>
#include <QCursor>

GameWidget::GameWidget(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(1000, 600);

    QTimer::singleShot(100, this, [this]{ setFocus(); });

    game_ = std::make_unique<Game>();
    game_->setup();
    // game_->start();

    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &GameWidget::on_frame);
    timer_->start(16); // ~60 FPS
    clock_.start();
}

GameWidget::~GameWidget() {
    if (game_) game_->stop();
}

void GameWidget::update_camera() {
    // Центрируем на игроке
    auto snap = game_->snapshot();
    for (auto& t : snap.tanks) {
        if (t.is_player && t.alive) {
            cam_x_ = t.x - view_w_cells_ / 2.0f;
            cam_y_ = t.y - view_h_cells_ / 2.0f;
            break;
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
    update_camera();
    update_player_input();
    update();
    explosion_pulse_ += 0.15f;
}

void GameWidget::update_player_input() {
    float dx = 0, dy = 0;
    if (key_w_) dy -= 1;
    if (key_s_) dy += 1;
    if (key_a_) dx -= 1;
    if (key_d_) dx += 1;

    // Прицел — на курсор мыши
    float wx = cam_x_ + mouse_world_x_ / cfg::TILE_SIZE;
    float wy = cam_y_ + mouse_world_y_ / cfg::TILE_SIZE;

    auto snap = game_->snapshot();
    float px = 0, py = 0;
    for (auto& t : snap.tanks) {
        if (t.is_player) { px = t.x; py = t.y; break; }
    }
    float aim = std::atan2(wy - py, wx - px);
    game_->set_player_input(dx, dy, aim, fire_);
}

// ============================================================ input
void GameWidget::keyPressEvent(QKeyEvent* e) {
    qDebug() << "[key] pressed:" << e->key()
             << " autorep:" << e->isAutoRepeat()
             << " hasFocus:" << hasFocus();

    if (e->isAutoRepeat()) { e->accept(); return; }
    switch (e->key()) {
    case Qt::Key_W: case Qt::Key_Up:    key_w_ = true; break;
    case Qt::Key_S: case Qt::Key_Down:  key_s_ = true; break;
    case Qt::Key_A: case Qt::Key_Left:  key_a_ = true; break;
    case Qt::Key_D: case Qt::Key_Right: key_d_ = true; break;
    case Qt::Key_Space: fire_ = true; break;
    case Qt::Key_Escape: close(); break;
    case Qt::Key_Q:
        if (paused_) emit back_to_menu();
        break;

    default: QWidget::keyPressEvent(e); return;
    }
    e->accept();
}

void GameWidget::keyReleaseEvent(QKeyEvent* e) {
    // ВАЖНО: НЕ игнорируем auto-repeat для release.
    // Если пользователь отпустил клавишу — сбрасываем флаг в любом случае.
    switch (e->key()) {
    case Qt::Key_W: case Qt::Key_Up:    key_w_ = false; break;
    case Qt::Key_S: case Qt::Key_Down:  key_s_ = false; break;
    case Qt::Key_A: case Qt::Key_Left:  key_a_ = false; break;
    case Qt::Key_D: case Qt::Key_Right: key_d_ = false; break;
    case Qt::Key_Space: fire_ = false; break;
    default: QWidget::keyReleaseEvent(e); return;
    }
    e->accept();
}

void GameWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) fire_ = true;
    if (e->button() == Qt::RightButton) fire_ = true;
}

void GameWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) fire_ = false;
    if (e->button() == Qt::RightButton) fire_ = false;
}

// ============================================================ paint
void GameWidget::paintEvent(QPaintEvent*) {
    if (!hasFocus()) {
        setFocus(Qt::OtherFocusReason);
    }
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int W = width();
    const int H = height();

    // ----- Фон — земля -----
    // Шахматная пиксель-арт текстура
    p.fillRect(rect(), QColor(60, 90, 55));

    // Сетка (пиксель-арт линии)
    p.setPen(QPen(QColor(50, 75, 48), 1));
    for (int gx = 0; gx <= view_w_cells_ + 1; ++gx) {
        int sx = int(gx * cfg::TILE_SIZE - std::fmod(cam_x_ * cfg::TILE_SIZE,
                                                     cfg::TILE_SIZE));
        p.drawLine(sx, 0, sx, H);
    }
    for (int gy = 0; gy <= view_h_cells_ + 1; ++gy) {
        int sy = int(gy * cfg::TILE_SIZE - std::fmod(cam_y_ * cfg::TILE_SIZE,
                                                     cfg::TILE_SIZE));
        p.drawLine(0, sy, W, sy);
    }

    auto snap = game_->snapshot();

    // ----- Здания -----
    for (auto& b : snap.buildings) {
        if (b.destroyed) {
            // руины — тёмный прямоугольник с «трещинами»
            QPointF tl = cell_to_screen(float(b.x), float(b.y));
            QRectF r(tl.x(), tl.y(), b.w * cfg::TILE_SIZE, b.h * cfg::TILE_SIZE);
            p.fillRect(r, QColor(50, 40, 30));
            p.setPen(QPen(QColor(30, 25, 20), 2));
            p.drawLine(r.topLeft(), r.bottomRight());
            p.drawLine(r.topRight(), r.bottomLeft());
        } else {
            QPointF tl = cell_to_screen(float(b.x), float(b.y));
            QRectF r(tl.x(), tl.y(), b.w * cfg::TILE_SIZE, b.h * cfg::TILE_SIZE);
            // стена
            p.fillRect(r, QColor(120, 90, 60));
            // кирпичи — горизонтальные линии
            p.setPen(QPen(QColor(80, 60, 40), 2));
            for (int gy = 0; gy < b.h * 2; ++gy) {
                float yy = tl.y() + gy * cfg::TILE_SIZE / 2.0f;
                p.drawLine(int(r.left()), int(yy), int(r.right()), int(yy));
            }
            // окантовка
            p.setPen(QPen(QColor(60, 40, 25), 3));
            p.drawRect(r);
        }
    }

    // ----- Взрывы (под танками) -----
    for (auto& e : snap.explosions) {
        QPointF c = cell_to_screen(e.x, e.y);
        float r = e.radius * cfg::TILE_SIZE *
                  (0.7f + 0.3f * std::sin(explosion_pulse_ * 5.0f));
        float alpha = std::max(0.0f, std::min(1.0f, e.ttl / 0.6f));
        // внешний оранжевый круг
        p.setBrush(QColor(255, 140, 30, int(200 * alpha)));
        p.setPen(Qt::NoPen);
        p.drawEllipse(c, r, r);
        // внутренний жёлтый
        p.setBrush(QColor(255, 240, 120, int(220 * alpha)));
        p.drawEllipse(c, r * 0.55f, r * 0.55f);
        // белый центр
        p.setBrush(QColor(255, 255, 255, int(180 * alpha)));
        p.drawEllipse(c, r * 0.25f, r * 0.25f);
    }

    // ----- Снаряды -----
    for (auto& s : snap.shells) {
        QPointF c = cell_to_screen(s.x, s.y);
        QColor col;
        switch (s.type) {
            case ShellType::AP:   col = QColor(255, 240, 150); break;
            case ShellType::HE:   col = QColor(255, 130, 60);  break;
            case ShellType::HEAT: col = QColor(255, 90, 200);  break;
        }
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        // круг с ярким контуром
        p.drawEllipse(c, 4, 4);
        p.setBrush(QColor(255, 255, 255, 180));
        p.drawEllipse(c, 2, 2);
    }

    // ----- Танки -----
    for (auto& t : snap.tanks) {
        if (!t.alive) continue;
        QPointF c = cell_to_screen(t.x, t.y);

        // корпус
        QColor body = (t.team == Team::Red)
                    ? (t.is_player ? QColor(80, 200, 80) : QColor(60, 140, 60))
                    : QColor(180, 60, 60);
        QColor dark = body.darker(160);

        // тень
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 60));
        p.drawRect(QRectF(c.x() - 12, c.y() - 10, 24, 22));

        // корпус танка (квадрат)
        QRectF bodyRect(c.x() - 12, c.y() - 10, 24, 20);
        p.setBrush(body);
        p.setPen(QPen(dark, 2));
        p.drawRect(bodyRect);

        // пиксельные «гусеницы» — полоски сверху и снизу
        p.setPen(Qt::NoPen);
        p.setBrush(dark);
        p.drawRect(QRectF(bodyRect.left(),  bodyRect.top() - 3, 24, 3));
        p.drawRect(QRectF(bodyRect.left(),  bodyRect.bottom(), 24, 3));

        // башня + ствол
        float angle = t.angle;
        QPointF muzzle = c + QPointF(std::cos(angle) * 22.0f,
                                     std::sin(angle) * 22.0f);
        // ствол
        p.setPen(QPen(QColor(40, 40, 40), 4));
        p.drawLine(c, muzzle);
        // башня (круг)
        p.setPen(QPen(dark, 2));
        p.setBrush(body.lighter(120));
        p.drawEllipse(c, 6, 6);
        // окошко
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(30, 30, 30));
        p.drawEllipse(c, 2, 2);

        // HP-бар над танком
        float hp_frac = float(t.hp) / float(t.max_hp);
        QRectF hpBg(c.x() - 14, c.y() - 20, 28, 4);
        QRectF hpFg(c.x() - 14, c.y() - 20, 28 * hp_frac, 4);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(30, 30, 30, 200));
        p.drawRect(hpBg);
        QColor hpCol = (t.is_player) ? QColor(80, 220, 80)
                                     : QColor(220, 80, 80);
        if (hp_frac < 0.3f) hpCol = QColor(230, 100, 30);
        p.setBrush(hpCol);
        p.drawRect(hpFg);

        // метка игрока
        if (t.is_player) {
            p.setPen(QPen(QColor(255, 255, 0), 2));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, 16, 16);
        }
    }

    // ----- HUD -----
    p.setRenderHint(QPainter::Antialiasing, false);
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

    // Мини-карта
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

    // Управление
    p.setPen(QColor(180, 180, 180, 200));
    p.setFont(QFont("Consolas", 9));
    p.drawText(10, H - 10,
               "WASD — движение, мышь — прицел, ЛКМ/Space — огонь, Esc — выход");


    if (paused_) {
        p.setBrush(QColor(0, 0, 0, 220));
        p.drawRect(rect());
        p.setPen(QColor(255, 255, 255));
        QFont big("Consolas", 40, QFont::Bold);
        p.setFont(big);
        QString t = "PAUSE";
        QFontMetrics fm(big);
        int tw = fm.horizontalAdvance(t);
        p.drawText((W - tw) / 2, H / 2 - 40, t);

        p.setFont(QFont("Consolas", 16));
        p.setPen(QColor(220, 220, 220));
        QString s1 = "Esc — продолжить";
        QString s2 = "R — рестарт";
        QString s3 = "Q — в главное меню";
        tw = QFontMetrics(QFont("Consolas", 16)).horizontalAdvance(s1);
        p.drawText((W - tw) / 2, H / 2 + 30, s1);
        tw = QFontMetrics(QFont("Consolas", 16)).horizontalAdvance(s2);
        p.drawText((W - tw) / 2, H / 2 + 60, s2);
        tw = QFontMetrics(QFont("Consolas", 16)).horizontalAdvance(s3);
        p.drawText((W - tw) / 2, H / 2 + 90, s3);
    }

    // ----- Game Over -----
    if (snap.game_over) {
        p.setBrush(QColor(0, 0, 0, 200));
        p.drawRect(rect());
        p.setPen(QColor(255, 255, 255));
        QFont big("Consolas", 32, QFont::Bold);
        p.setFont(big);
        QString txt = snap.player_won ? "ПОБЕДА!" : "ПОРАЖЕНИЕ";
        QColor col = snap.player_won ? QColor(80, 220, 80) : QColor(220, 80, 80);
        p.setPen(col);
        QFontMetrics fm(big);
        int tw = fm.horizontalAdvance(txt);
        p.drawText((W - tw) / 2, H / 2, txt);
        p.setFont(QFont("Consolas", 14));
        p.setPen(QColor(220, 220, 220));
        p.drawText((W - 100) / 2, H / 2 + 40, "Esc — выйти");
    }
}

void GameWidget::mouseMoveEvent(QMouseEvent* e) {
    mouse_world_x_ = float(e->position().x());
    mouse_world_y_ = float(e->position().y());
}

void GameWidget::focusOutEvent(QFocusEvent* e) {
    // При потере фокуса сбрасываем ВСЁ — иначе клавиши залипают
    key_w_ = key_a_ = key_s_ = key_d_ = false;
    fire_ = false;
    QWidget::focusOutEvent(e);
}

void GameWidget::restart() {
    if (game_) game_->stop();
    game_ = std::make_unique<Game>();
    game_->setup();
    game_->start();
    paused_ = false;
    update();   // перерисовать сразу
}
