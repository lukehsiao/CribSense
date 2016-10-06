#-------------------------------------------------
#
# Project created by QtCreator 2012-04-11T14:15:20
#
#-------------------------------------------------

QT       += core gui

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

CV2_INCLUDE = /usr/local/include
CV2_LIB = /usr/local/lib
INCLUDEPATH += $$CV2_INCLUDE
LIBS += -L$$CV2_LIB
LIBS += -lopencv_highgui -lopencv_core -lopencv_imgproc

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
