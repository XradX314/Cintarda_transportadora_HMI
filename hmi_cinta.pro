QT       += core gui widgets serialport
CONFIG   += c++17
TARGET   = hmi_cinta
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    protocol.cpp \
    cintatracker.cpp

HEADERS += \
    mainwindow.h \
    protocol.h \
    cintatracker.h

win32: RC_ICONS =
