#pragma once
#ifndef _COMMON_H
#define _COMMON_H

//#include <>
#include <QColor>

enum MyPenKind { penNone, penBlack, penRed, penGreen, penBlue, penEraser, penYellow };
enum PrinterFlags :char {
    pfPrintBackgroundImage  = 1,
    pfWhiteBackground       = 2,		// or use display mode which may be dark
    pfGrid                  = 4,
    pfGrayscale             = 8,
    pfLandscape             = 16,
    pfDontPrintImages       = 32
};


// ******************************************************
//----------------------------- DrawColors -------------------
// object created in DrawArea.cpp
class DrawColors
{
    QColor _invalid;
    bool _dark = false;
    struct _clr
    {
        MyPenKind kind;
        QColor lightColor,    // _dark = false - for light mode
               darkColor;     // _dark = true  - for dark mode
        _clr() : kind(penNone), lightColor(QColor()), darkColor(QColor()) { }
    } _colors[5];
    const int _COLOR_COUNT = 5;

    int _penColorIndex(MyPenKind pk)
    {
        for (int i = 0; i < _COLOR_COUNT; ++i)
            if (_colors[i].kind == pk)
                return i;
        return -1;
    }
public:
    DrawColors(bool darkMode = false)
    {
        _colors[0].kind = penBlack;
        _colors[0].lightColor = Qt::black;
        _colors[0].darkColor = Qt::white;
        _colors[1].kind = penRed;
        _colors[1].lightColor = Qt::red;
        _colors[1].darkColor = Qt::red;
        _colors[2].kind = penGreen;
        _colors[2].lightColor = "#007d1a";
        _colors[2].darkColor = Qt::green;
        _colors[3].kind = penBlue;
        _colors[3].lightColor = Qt::blue;
        _colors[3].darkColor = "#82dbfc";
        _colors[4].kind = penYellow;
        _colors[4].lightColor = "#b704be";
        _colors[4].darkColor = Qt::yellow;
    }
    bool SetDarkMode(bool dark) // returns previous mode
    {
        bool b = _dark;
        _dark = dark;
        return b;
    }
    bool IsDarkMode() const { return _dark; }
    QColor& operator[](MyPenKind pk)
    {
        int i = _penColorIndex(pk);
        return   (i < 0 ? _invalid : (_dark ? _colors[i].darkColor : _colors[i].lightColor));
    }
};

extern DrawColors drawColors;       // in drawarea.cpp

#endif // _COMMON_H