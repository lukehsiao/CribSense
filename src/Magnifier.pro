#-------------------------------------------------
#
# Project created by QtCreator 2012-04-11T14:15:20
#
#-------------------------------------------------

QT += core gui widgets
CONFIG += c++11 debug
QMAKE_CXXFLAGS += -O3 -pthread 
QMAKE_CXXFLAGS += -mfloat-abi=hard -mtune=cortex-a53 -mfpu=crypto-neon-fp-armv8 -funsafe-math-optimizations -fno-trapping-math -fno-signaling-nans
TARGET = Magnifier
TEMPLATE = app
LIBS += -pthread

HEADERS += \
    VideoSource.hpp \
    CommandLine.hpp \
    DisplayWidget.hpp \
    MainDialog.hpp \
    RieszTransform.hpp \
    ComplexMat.hpp \
    Butterworth.hpp \
    WorkerThread.hpp \
    INIReader.h \
    ini.h \
    MotionDetection.hpp \
    #

SOURCES += \
    CommandLine.cpp \
    MainDialog.cpp \
    Butterworth.cpp \
    INIReader.cpp \
    ini.c \
    MotionDetection.cpp \
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
