#include "menuwidget.h"
#include <QLabel>
#include <QFont>
#include <QPainter>

MenuWidget::MenuWidget(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);

    setMinimumSize(1000, 600);
    setAutoFillBackground(true);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);

    QLabel* title = new QLabel("STEEL FRONT");
    QFont tf("Consolas", 48, QFont::Bold);
    title->setFont(tf);
    title->setStyleSheet("color: #f0d060; background: transparent;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    QLabel* sub = new QLabel("Многопоточные танковые сражения");
    QFont sf("Consolas", 14);
    sub->setFont(sf);
    sub->setStyleSheet("color: #c0c0c0; background: transparent;");
    sub->setAlignment(Qt::AlignCenter);
    layout->addWidget(sub);

    layout->addSpacing(40);

    auto makeBtn = [&](const QString& text) {
        QPushButton* b = new QPushButton(text);
        b->setFixedSize(240, 50);
        QFont bf("Consolas", 16, QFont::Bold);
        b->setFont(bf);
        b->setStyleSheet(
            "QPushButton {"
            "  background-color: #2a5a2a;"
            "  color: #e0e0e0;"
            "  border: 3px solid #80c080;"
            "  border-radius: 6px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #3a7a3a;"
            "}"
            "QPushButton:pressed {"
            "  background-color: #1a4a1a;"
            "}");
        return b;
    };

    QPushButton* singleBtn = makeBtn("ОДИНОЧНАЯ ИГРА");
    QPushButton* duoBtn    = makeBtn("ИГРА С ДРУГОМ");
    QPushButton* quitBtn = makeBtn("ВЫХОД");
    layout->addWidget(singleBtn, 0, Qt::AlignCenter);
    layout->addSpacing(15);
    layout->addWidget(duoBtn, 0, Qt::AlignCenter);
    layout->addSpacing(15);
    layout->addWidget(quitBtn, 0, Qt::AlignCenter);

    connect(singleBtn, &QPushButton::clicked, this, &MenuWidget::single_player_clicked);
    connect(duoBtn,    &QPushButton::clicked, this, &MenuWidget::two_players_clicked);
    connect(quitBtn,   &QPushButton::clicked, this, &MenuWidget::quit_clicked);

    layout->addSpacing(40);

    QLabel* help = new QLabel(
        "WASD — движение   •   мышь — прицел\n"
        "ЛКМ / Space — огонь   •   1 / 2 / 3 — тип снаряда\n"
        "Esc — пауза   •   R — рестарт");
    QFont hf("Consolas", 11);
    help->setFont(hf);
    help->setStyleSheet("color: #909090; background: transparent;");
    help->setAlignment(Qt::AlignCenter);
    layout->addWidget(help);
}

// красивый фон
void MenuWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(20, 30, 25));
    // Пиксельная сетка
    p.setPen(QPen(QColor(30, 45, 35), 1));
    for (int x = 0; x < width(); x += 32)
        p.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += 32)
        p.drawLine(0, y, width(), y);
}
