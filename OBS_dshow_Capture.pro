QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++14
CONFIG += console
TEMPLATE = app

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    cpswidget.cpp \
    obsqtdisplay.cpp \
    obssdk.cpp \
    window-projector.cpp

HEADERS += \
    cpswidget.h \
    display-helpers.hpp \
    obsqtdisplay.h \
    obssdk.h \
    screenshot-obj.hpp \
    window-projector.hpp

FORMS += \
    cpswidget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

LIBS += -L$$PWD/obsSDK/ -lobs

INCLUDEPATH += $$PWD/obsSDK/include
DEPENDPATH += $$PWD/obsSDK/include
LIBS += -L$$PWD/dependencies2019/win64/bin/ -lavcodec
#
INCLUDEPATH += $$PWD/dependencies2019/win64/include
DEPENDPATH += $$PWD/dependencies2019/win64/include
