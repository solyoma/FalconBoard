
# ----------------------------------------------------
# MyWhiteboardViewer GUI Application
# ----------------------------------------------------
TARGET = MyWhiteboardViewer
DESTDIR = ../Debug
CONFIG += debug
LIBS += -L"."
DEPENDPATH += .
MOC_DIR += ./Debug/moc
OBJECTS_DIR += ./Debug
UI_DIR += ./Debug/Ui
RCC_DIR += ./Debug/rcc
win32:RC_FILE = MyWhiteboardViewer.rc
HEADERS += ../MyWhiteboard/common.h \
    ../MyWhiteboard/history.h \
    ../MyWhiteboard/MyWhiteboard.h \
    ../MyWhiteboard/DrawArea.h \
    ../MyWhiteboard/pagesetup.h \
    ../MyWhiteboard/myprinter.h \
    ../MyWhiteboard/snipper.h
SOURCES += ../MyWhiteboard/DrawArea.cpp \
    ../MyWhiteboard/history.cpp \
    ../MyWhiteboard/snipper.cpp \
    ../MyWhiteboard/main.cpp \
    ../MyWhiteboard/MyWhiteboard.cpp \
    ../MyWhiteboard/pagesetup.cpp \
    ../MyWhiteboard/myprinter.cpp
FORMS += ../MyWhiteboard/MyWhiteboard.ui \
    ../MyWhiteboard/pagesetup.ui \
    ../MyWhiteboard/printprogress.ui
RESOURCES += ../MyWhiteboard/MyWhiteboard.qrc
INCLUDES += . ../MyWhiteBoard
QMAKE_CXXFLAGS += -std=c++17 -Wno-unused-parameter
QT += gui widgets printsupport
requires(qtConfig(filedialog))
qtHaveModule(printsupport): QT += printsupport
