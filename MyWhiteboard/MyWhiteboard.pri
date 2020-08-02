# ----------------------------------------------------
# MyWhiteboard GUI Application
# ----------------------------------------------------
HEADERS += ./common.h \
    ./history.h \
    ./MyWhiteboard.h \
    ./DrawArea.h \
    ./pagestup.h
    ./myprinter.h
    ./snipper.h
SOURCES += ./DrawArea.cpp \
    ./history.cpp \
    ./snipper.cpp \
    ./main.cpp \
    ./MyWhiteboard.cpp \
    ./pagestup.cpp \
    ./myprinter.cpp
FORMS += ./MyWhiteboard.ui \
    ./pagestup.ui \
    ./myprinter.ui
RESOURCES += ./MyWhiteboard.qrc
INCLUDES += . .MyWhiteBoard
QMAKE_CXXFLAGS += -std=c++17 -Wno-unused-parameter
QT += gui widgets
requires(qtConfig(filedialog))
qtHaveModule(printsupport): QT += printsupport
