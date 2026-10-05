QT += core gui widgets

CONFIG += c++20
CONFIG += console
CONFIG -= app_bundle

TARGET = SteelFrontQt
TEMPLATE = app

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/gamewidget.cpp \
    src/game.cpp \
    src/menuwidget.cpp

HEADERS += \
    src/mainwindow.h \
    src/gamewidget.h \
    src/game.h \
    src/entities.h \
    src/menuwidget.h \
    src/ring_queue.h

QMAKE_CXXFLAGS_RELEASE += -O2
QMAKE_CXXFLAGS += -pthread
LIBS += -pthread
