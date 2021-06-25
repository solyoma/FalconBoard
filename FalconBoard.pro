# ----------------------------------------------------
# FalconBoard GUI application
# ------------------------------------------------------

TARGET = FalconBoard
DESTDIR = ../Debug
CONFIG += debug
LIBS += -L"."
DEPENDPATH += .
MOC_DIR += ./Debug/moc
OBJECTS_DIR += ./Debug
UI_DIR += ./Debug/Ui
RCC_DIR += ./Debug/rcc
win32:RC_FILE = FalconBoard.rc
HEADERS += ./bands.h \
    FalconBoard/common.h \
    FalconBoard/history.h \
    FalconBoard/myevent.h \
    FalconBoard/myprinter.h \
    FalconBoard/FalconBoard.h \
    FalconBoard/DrawArea.h \
    FalconBoard/screenshotTransparency.h \
    FalconBoard/pdfsetup.h \
    FalconBoard/printprogress.h \
    FalconBoard/pagesetup.h \
    FalconBoard/snipper.h
SOURCES += ./bands.cpp \
    FalconBoard/DrawArea.cpp \
    FalconBoard/history.cpp \
    FalconBoard/pagesetup.cpp \
    FalconBoard/pdfsetup.cpp \
    FalconBoard/screenshotTransparency.cpp \
    FalconBoard/snipper.cpp \
    FalconBoard/main.cpp \
    FalconBoard/FalconBoard.cpp \
    FalconBoard/myprinter.cpp
FORMS += ./FalconBoard.ui \
    FalconBoard/pagesetup.ui \
    FalconBoard/pdfsetup.ui \
    FalconBoard/printprogress.ui \
    FalconBoard/screenshotTransparency.ui
RESOURCES += FalconBoard/FalconBoard.qrc
INCLUDES += FalconBoard
QMAKE_CXXFLAGS += -std=c++17 -Wno-unused-parameter
QT += gui widgets printsupport
requires(qtConfig(filedialog))
qtHaveModule(printsupport): QT += printsupport
TRANSLATIONS = FalconBoard_en.ts \
               FalconBoard_hu.ts
