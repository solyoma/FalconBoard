# ----------------------------------------------------
# MyWhiteboard GUI application
# ------------------------------------------------------

TARGET = MyWhiteboard
DESTDIR = ../Debug
CONFIG += debug
LIBS += -L"."
DEPENDPATH += .
MOC_DIR += ./Debug/moc
OBJECTS_DIR += ./Debug
UI_DIR += ./Debug/Ui
RCC_DIR += ./Debug/rcc
win32:RC_FILE = MyWhiteboard.rc
HEADERS += ./common.h \
    ./history.h \
    ./MyWhiteboard.h \
    ./DrawArea.h \
    ./pagesetup.h \
    ./myprinter.h \
    ./snipper.h \
    ./printprogress.h
SOURCES += ./DrawArea.cpp \
    ./history.cpp \
    ./snipper.cpp \
    ./main.cpp \
    ./MyWhiteboard.cpp \
    ./pagesetup.cpp \
    ./myprinter.cpp
FORMS += ./MyWhiteboard.ui \
    ./pagesetup.ui \
    ./printprogress.ui
RESOURCES += ./MyWhiteboard.qrc
INCLUDES += . .MyWhiteBoard
QMAKE_CXXFLAGS += -std=c++17 -Wno-unused-parameter
QT += gui widgets printsupport
requires(qtConfig(filedialog))
qtHaveModule(printsupport): QT += printsupport

