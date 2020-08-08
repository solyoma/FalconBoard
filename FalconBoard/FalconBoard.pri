# ----------------------------------------------------
# FalconBoard GUI Application
# ----------------------------------------------------
HEADERS += ./common.h \
    ./history.h \
    ./FalconBoard.h \
    ./DrawArea.h \
    ./pagestup.h
    ./myprinter.h
    ./snipper.h
SOURCES += ./DrawArea.cpp \
    ./history.cpp \
    ./snipper.cpp \
    ./main.cpp \
    ./FalconBoard.cpp \
    ./pagestup.cpp \
    ./myprinter.cpp
FORMS += ./FalconBoard.ui \
    ./pagestup.ui \
    ./myprinter.ui
RESOURCES += ./FalconBoard.qrc
INCLUDES += . .MyWhiteBoard
QMAKE_CXXFLAGS += -std=c++17 -Wno-unused-parameter
QT += gui widgets
requires(qtConfig(filedialog))
qtHaveModule(printsupport): QT += printsupport
