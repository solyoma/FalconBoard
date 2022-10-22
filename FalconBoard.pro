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
HEADERS += ./common.h \
	./drawables.h \
    ./DrawArea.h \
    ./FalconBoard.h \
    ./history.h \
    ./helpdialog.h \
    ./myevent.h \
    ./myprinter.h \
	./quadtree.h \
    ./pagesetup.h \
    ./printprogress.h \
    ./screenshotTransparency.h \
    ./snipper.h
SOURCES += ./drawables.cpp \
    ./DrawArea.cpp \
    ./FalconBoard.cpp \
    ./history.cpp \
    ./main.cpp \
    ./myprinter.cpp
    ./pagesetup.cpp \
    ./screenshotTransparency.cpp \
    ./snipper.cpp \
FORMS += ./FalconBoard.ui \
	./helpdialog.ui \
    ./pagesetup.ui \
    ./printprogress.ui \
    ./screenshotTransparency.ui
RESOURCES += FalconBoard.qrc
INCLUDEPATH += .
QMAKE_CXXFLAGS += -std=c++17 -Wno-unused-parameter
QT += gui widgets printsupport
requires(qtConfig(filedialog))
qtHaveModule(printsupport): QT += printsupport
TRANSLATIONS += ../translations/FalconBoard_en.ts \
               ../translations/FalconBoard_hu.ts
