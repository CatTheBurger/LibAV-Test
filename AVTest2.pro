TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

LIBS += -lavcodec -lavformat -lavutil

SOURCES += \
        main.c
