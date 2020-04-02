# ----------------------------------------------------
# MyWhiteboard GUI Application
# ----------------------------------------------------
HEADERS += ./MyWhiteboard/history.h \
    ./MyWhiteboard/MyWhiteboard.h \
    ./MyWhiteboard/DrawArea.h \
    ./MyWhiteboard/snipper.h
SOURCES += ./MyWhiteboard/DrawArea.cpp \
    ./MyWhiteboard/snipper.cpp \
    ./MyWhiteboard/main.cpp \
    ./MyWhiteboard/MyWhiteboard.cpp
FORMS += ./MyWhiteboard/MyWhiteboard.ui
RESOURCES += MyWhiteboard.qrc
QMAKE_CXXFLAGS += -std=c++17 -Wno-unused-parameter
QT += gui widgets
requires(qtConfig(filedialog))
qtHaveModule(printsupport): QT += printsupport

