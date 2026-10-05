#include "mainwindow.h"
#include "menuwidget.h"
#include "gamewidget.h"
#include <QMenuBar>
#include <QAction>
#include <QMessageBox>
#include <QApplication>
#include <QShowEvent>
#include <QKeyEvent>
#include <QStackedWidget>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Steel Front — Multithreaded Tank Battle");
    resize(1100, 620);

    stack_ = new QStackedWidget(this);
    setCentralWidget(stack_);

    menu_ = new MenuWidget(this);
    stack_->addWidget(menu_);

    // Создание пустой игры - ТОЛЬКО ПО КЛИКУ!!!
    // game_ = new GameWidget(this);
    // stack_->addWidget(game_);
    // game_->hide();

    connect(menu_, &MenuWidget::play_clicked, this, &MainWindow::start_new_game);
    connect(menu_, &MenuWidget::quit_clicked, this, &MainWindow::close);
    // connect(game_, &GameWidget::back_to_menu, this, &MainWindow::show_menu);


    stack_->setCurrentWidget(menu_);
    setFocusProxy(menu_);

    // game_->setFocus();
    // connect(game_, &GameWidget::destroyed, this, [] {});

    // auto* fileMenu = menuBar()->addMenu("&Игра");
    // auto* exitAct = fileMenu->addAction("Выход");
    // exitAct->setShortcut(QKeySequence::Quit);
    // connect(exitAct, &QAction::triggered, this, &QWidget::close);

    // auto* helpMenu = menuBar()->addMenu("&Справка");
    // auto* aboutAct = helpMenu->addAction("О программе");
    // connect(aboutAct, &QAction::triggered, [this] {
    //     QMessageBox::information(this, "О программе",
    //         "Steel Front — демонстрация многопоточности.\n\n"
    //         "Потоки:\n"
    //         "• AI по одному на каждый вражеский танк\n"
    //         "• Пул из 4 потоков для расчёта баллистики снарядов\n"
    //         "• Поток коллизий\n"
    //         "• Поток разрушений\n"
    //         "• Поток физики (sim)\n"
    //         "• Главный поток Qt — рендер\n\n"
    //         "Синхронизация: std::barrier, std::atomic, lock-free RingQueue.\n\n"
    //         "Управление: WASD — движение, мышь — прицел, ЛКМ/Space — огонь.");
    // });
}

void MainWindow::showEvent(QShowEvent* e) {
    QMainWindow::showEvent(e);
    // if (game_) {
    //     game_->setFocus(Qt::OtherFocusReason);
    //     game_->activateWindow();
    // }
    QWidget* cur = stack_->currentWidget();
    if (cur) cur->setFocus(Qt::OtherFocusReason);
}

void MainWindow::show_menu() {
    stack_->setCurrentWidget(menu_);
    menu_->setFocus(Qt::OtherFocusReason);
}

void MainWindow::show_game() {
    // stack_->setCurrentWidget(game_);
    // game_->show();
    if (!game_) return;
    stack_->setCurrentWidget(game_);
    game_->setFocus(Qt::OtherFocusReason);
}

void MainWindow::start_new_game() {
    qDebug() << "[MainWindow] start_new_game called";
    if (!game_) {
        qDebug() << "[MainWindow] creating GameWidget";
        game_ = new GameWidget(this);
        stack_->addWidget(game_);
        connect(game_, &GameWidget::back_to_menu, this, &MainWindow::show_menu);
        qDebug() << "[MainWindow] calling game_->restart()";
        game_->restart();
    } else {
        qDebug() << "[MainWindow] restarting existing GameWidget";
        game_->restart();
    }
    qDebug() << "[MainWindow] showing game";
    show_game();
    qDebug() << "[MainWindow] done";
}

void MainWindow::keyPressEvent(QKeyEvent* e) {
    // Q — выход в меню из игры
    if (e->key() == Qt::Key_Q && stack_->currentWidget() == game_) {
        show_menu();
        return;
    }
    QMainWindow::keyPressEvent(e);
}

