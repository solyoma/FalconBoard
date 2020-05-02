# ----------------------------------------------------
# MyWhiteboard GUI Application
# ----------------------------------------------------
HEADERS += ./MyWhiteboard/history.h \
    MyWhiteboard.h \
    DrawArea.h \
    snipper.h
SOURCES += DrawArea.cpp \
    history.cpp \
    snipper.cpp \
    main.cpp \
    MyWhiteboard.cpp
FORMS += MyWhiteboard.ui
RESOURCES += MyWhiteboard.qrc
INCLUDES += .
QMAKE_CXXFLAGS += -std=c++17 -Wno-unused-parameter
QT += gui widgets
requires(qtConfig(filedialog))
qtHaveModule(printsupport): QT += printsupport

