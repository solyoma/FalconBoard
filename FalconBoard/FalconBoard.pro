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
	./common.h \
	./drawables.h \
	./DrawArea.h \
	./FalconBoard.h \
	./helpdialog.h \
	./history.h \
	./myevent.h \
	./myprinter.h \
	./pagesetup.h \
	./pencolorsdialog.h \
	./pencolorset.h \
	./printprogress.h \
	./quadtree.h \
	./rotateinput.h \
	./screenshotTransparency.h \
	./smoother.h \
	./snipper.h
SOURCES += \
	./common.cpp \
	./drawables.cpp \
	./DrawArea.cpp \
	./FalconBoard.cpp \
	./history.cpp \
	./main.cpp \
	./myprinter.cpp \
	./pagesetup.cpp \
	./pencolorsdialog.cpp \
	./pencolorset.cpp \
	./rotateinput.cpp \
	./screenshotTransparency.cpp \
	./snipper.cpp
FORMS += ./FalconBoard.ui \
	./helpdialog.ui \
	./pagesetup.ui \
	./pencolorsdialog.ui \
	./pencolorset.ui \
	./printprogress.ui \
	./rotateinput.ui \
	./screenshotTransparency.ui
RESOURCES += FalconBoard.qrc
INCLUDEPATH += .
QMAKE_CXXFLAGS += -std=c++17 -Wno-unused-parameter -Wno-reorder
QT += gui widgets printsupport network
requires(qtConfig(filedialog))
qtHaveModule(printsupport): QT += printsupport
TRANSLATIONS += ../translations/FalconBoard_en.ts \
			   ../translations/FalconBoard_hu.ts
