NULL =

AM_CXXFLAGS = $(ARCH_CXXFLAGS) $(WARN_CXXFLAGS) -O3 \
	-funsafe-math-optimizations \
	-fno-trapping-math \
	-fno-signaling-nans \
	-pthread

AM_LDFLAGS = -lrt

bin_PROGRAMS = cribsense

cribsense_SOURCES = \
    src/VideoSource.hpp \
    src/CommandLine.hpp \
    src/RieszTransform.hpp \
    src/ComplexMat.hpp \
    src/Butterworth.hpp \
    src/WorkerThread.hpp \
    src/INIReader.h \
    src/ini.h \
    src/MotionDetection.hpp \
    src/CommandLine.cpp \
    src/Butterworth.cpp \
    src/INIReader.cpp \
    src/ini.c \
    src/MotionDetection.cpp \
    src/RieszTransform.cpp \
    src/main.cpp \
    src/VideoSource.cpp \
    $(NULL)

cribsense_CXXFLAGS = $(AM_CXXFLAGS) $(DEPS_CFLAGS)
cribsense_LDFLAGS = $(AM_LDFLAGS) $(DEPS_LIBS)

dist_doc_DATA = \
    docs/index.md \
    docs/setup/setup.md

GITIGNOREFILES = aclocal.m4 build-aux config.h.in

-include $(top_srcdir)/git.mk
