QT += core gui testlib
CONFIG += testcase console c++20
CONFIG -= app_bundle
TARGET = engine_test
SOURCES += engine_test.cpp ../src/engine.cpp ../src/theme.cpp
HEADERS += ../src/engine.h ../src/theme.h

RESOURCES += ../fonts.qrc
