QT += core gui quick testlib
CONFIG += testcase console c++20
CONFIG -= app_bundle
TARGET = windowplacement_test
SOURCES += windowplacement_test.cpp ../src/windowplacement.cpp
HEADERS += ../src/windowplacement.h
