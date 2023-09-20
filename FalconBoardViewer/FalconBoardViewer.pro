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
HEADERS += \
	../FalconBoard/common.h \
	../FalconBoard/drawables.h \
	../FalconBoard/DrawArea.h \
	../FalconBoard/FalconBoard.h \
	../FalconBoard/helpdialog.h \
	../FalconBoard/history.h \
	../FalconBoard/myevent.h \
	../FalconBoard/myprinter.h \
	../FalconBoard/pagesetup.h \
	../FalconBoard/pencolors.h \
	../FalconBoard/printprogress.h \
	../FalconBoard/quadtree.h \
	../FalconBoard/screenshotTransparency.h \
	./FalconBoard/smoother.h \
	../FalconBoard/snipper.h
SOURCES += \
	../FalconBoard/common.cpp \
	../FalconBoard/drawables.cpp
	../FalconBoard/DrawArea.cpp \
	../FalconBoard/FalconBoard.cpp \
	../FalconBoard/history.cpp \
	../FalconBoard/main.cpp \
	../FalconBoard/myprinter.cpp \
	../FalconBoard/pagesetup.cpp \
	../FalconBoard/pencolors.cpp \
	../FalconBoard/screenshotTransparency.cpp \
	../FalconBoard/snipper.cpp
FORMS += \
	../FalconBoard/FalconBoard.ui \
	../FalconBoard/helpdialog.ui \
	../FalconBoard/pagesetup.ui \
	../FalconBoard/printprogress.ui \
	../FalconBoard/screenshotTransparency.ui
RESOURCES += ../FalconBoard/FalconBoard.qrc
INCLUDEPATH += . ../FalconBoard
QMAKE_CXXFLAGS += -std=c++17 -Wno-unused-parameter
QT += gui widgets printsupport
requires(qtConfig(filedialog))
qtHaveModule(printsupport): QT += printsupport
TRANSLATIONS += ../translations/FalconBoard_en.ts \
                ../translations/FalconBoard_hu.ts
