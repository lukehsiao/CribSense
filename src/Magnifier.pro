#-------------------------------------------------
#
# Project created by QtCreator 2012-04-11T14:15:20
#
#-------------------------------------------------

QT       += core gui widgets
CONFIG += c++11
TARGET = Magnifier
TEMPLATE = app

HEADERS += \
    VideoSource.hpp \
    CommandLine.hpp \
    DisplayWidget.hpp \
    MainDialog.hpp \
    RieszTransform.hpp \
    ComplexMat.hpp \
    Butterworth.hpp \
    #

SOURCES += \
    CommandLine.cpp \
    MainDialog.cpp \
    Butterworth.cpp \
    main.cpp \
    #

CV2_INCLUDE = `pkg-config --cflags opencv`
CV2_LIB = `pkg-config --libs opencv`
CPPFLAGS += $$CV2_INCLUDE
LIBS += $$CV2_LIB

macx {
    # QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.6
    # ICON = applicaton-icon-file.icns
    plist.path = "$$DESTDIR/$$join(TARGET,,,.app)/Contents"
    plist.files = Info.plist
    INSTALLS += plist
}

win32 {
    CV22_INCLUDE =
    CV22_LIB =
}

clang {
    message(homebrew OpenCV 2.4.8.2)
    message(homebrew Qt 4.8.6 and following qmake.conf)
    message(/usr/local/Cellar/qt/4.8.5/mkspecs/unsupported/macx-clang-libc++/qmake.conf)
}
