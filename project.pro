QT += gui widgets

CONFIG += c++17 console
CONFIG -= app_bundle

QMAKE_CXXFLAGS += -Werror=enum-compare -Werror=return-type

HEADERS += main.h
SOURCES += main.cpp
