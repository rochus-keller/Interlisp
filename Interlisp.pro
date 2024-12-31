
QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = InterlispNavigator
TEMPLATE = app

INCLUDEPATH += ..

SOURCES += \
    LispReader.cpp \
    ../GuiTools/CodeEditor.cpp \
    ../GuiTools/AutoMenu.cpp \
    ../GuiTools/UiFunction.cpp \
    ../GuiTools/NamedFunction.cpp \
    LispLexer.cpp \
    LispHighlighter.cpp \
    LispBuiltins.cpp \
    LispRowCol.cpp \
    LispNavigator.cpp \
    ../GuiTools/AutoShortcut.cpp

HEADERS  += \
    LispReader.h \
    ../GuiTools/CodeEditor.h \
    ../GuiTools/AutoMenu.h \
    ../GuiTools/UiFunction.h \
    ../GuiTools/NamedFunction.h \
    LispLexer.h \
    LispHighlighter.h \
    LispBuiltins.h \
    LispRowCol.h \
    LispNavigator.h \
    ../GuiTools/AutoShortcut.h

CONFIG(debug, debug|release) {
        DEFINES += _DEBUG
}

!win32 {
    QMAKE_CXXFLAGS += -Wno-reorder -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable
}

RESOURCES += \
    Navigator.qrc
