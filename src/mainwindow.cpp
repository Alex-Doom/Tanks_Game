#include "mainwindow.h"
#include "gamewidget.h"
#include <QMenuBar>
#include <QAction>
#include <QMessageBox>
#include <QApplication>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Steel Front — Multithreaded Tank Battle");
    resize(1100, 620);

    game_ = new GameWidget(this);
    setCentralWidget(game_);

    setFocusProxy(game_);

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
    if (game_) {
        game_->setFocus(Qt::OtherFocusReason);
        game_->activateWindow();
    }
}
