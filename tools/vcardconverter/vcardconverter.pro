PROJECT_NAME = vcardconverter
TEMPLATE = app
CONFIG += ordered hide_symbols

include(../../config.pri) | error("Can't load config")

CONFIG += link_pkgconfig
PKGCONFIG += Qt5Contacts Qt5Versit contactcache-qt5

QT = core
TARGET = $$PROJECT_NAME
CONFIG -= app_bundle # OS X

SOURCES += main.cpp

target.path = $$INSTALL_ROOT/usr/bin/
INSTALLS += target
