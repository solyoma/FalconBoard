
# ----------------------------------------------------
# FalconBoardViewer GUI Application
# ----------------------------------------------------
TARGET = FalconBoardViewer
DESTDIR = ../Debug
CONFIG += debug
LIBS += -L"."
DEPENDPATH += .
MOC_DIR += ./Debug/moc
OBJECTS_DIR += Debug
UI_DIR += ./Debug/Ui
RCC_DIR += ./Debug/rcc
win32:RC_FILE = FalconBoardViewer.rc
HEADERS += ../FalconBoard/common.h \
    ../FalconBoard/DrawArea.h \
    ../FalconBoard/FalconBoard.h \
    ../FalconBoard/history.h \
    ../FalconBoard/myevent.h \
    ../FalconBoard/myprinter.h \
    ../FalconBoard/pagesetup.h \
    ../FalconBoard/pdfesetup.h \
    ../FalconBoard/printprogress.h \
    ../FalconBoard/snipper.h
SOURCES += ../FalconBoard/DrawArea.cpp \
    ../FalconBoard/history.cpp \
    ../FalconBoard/snipper.cpp \
    ../FalconBoard/main.cpp \
    ../FalconBoard/FalconBoard.cpp \
    ../FalconBoard/pagesetup.cpp \
    ../FalconBoard/myprinter.cpp
    ../FalconBoard/printprogress.cpp \
    ../FalconBoard/snipper.cpp
FORMS += ../FalconBoard/FalconBoard.ui \
    ../FalconBoard/pagesetup.ui \
    ../FalconBoard/pdfesetup.ui \
    ../FalconBoard/printprogress.ui
RESOURCES += ../FalconBoard/FalconBoard.qrc
INCLUDES += ../MyWhiteBoard
QMAKE_CXXFLAGS += -std=c++17 -Wno-unused-parameter
QT += gui widgets printsupport
requires(qtConfig(filedialog))
qtHaveModule(printsupport): QT += printsupport
