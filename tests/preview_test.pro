QT += core gui testlib
CONFIG += console c++20 testcase
CONFIG -= app_bundle
TARGET = preview_test
INCLUDEPATH += ../src
SOURCES += preview_test.cpp ../src/engine.cpp ../src/theme.cpp ../src/drawing.cpp ../src/preview.cpp
HEADERS += ../src/engine.h ../src/theme.h ../src/drawing.h ../src/preview.h
