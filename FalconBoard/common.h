#pragma once
#ifndef _COMMON_H
#define _COMMON_H

//#include <>
#include <QColor>
// version number 0xMMIISS;     M - major, I-minor, s- sub
const long nVersion = 0x00010106;       // program version
const QString sVersion = "1.1.6";
const QString sWindowTitle =
#ifdef _VIEWER
        "FalconBoard Viewer";
#else
        "FalconBoard";
#endif



enum FalconPenKind { penNone=0, penBlack=1, penRed=2, penGreen=3, penBlue=4, penYellow=6, penEraser=5};
enum PrinterFlags :char {
    pfPrintBackgroundImage  = 1,
    pfWhiteBackground       = 2,		// or use display mode which may be dark
    pfGrid                  = 4,
    pfGrayscale             = 8,
    pfLandscape             = 16,
    pfDontPrintImages       = 32
};
enum SaveResult { srCancelled = -1, srFailed, srSaveSuccess};
enum MyFontStyle { mfsNormal, mfsBold, mfsItalic, mfsSubSupScript, msfAllCaps};

/*========================================================
 * Structure to hold item indices for one band or selection
 *  An ItemIndex may hold scribbles or screenshot images
 *  Screenshot items have zorder-s below DRAWABLE_ZORDER_BASE
 *-------------------------------------------------------*/
struct ItemIndex
{
    int zorder;                 // indices are ordered in ascending zorder
                                // if < DRAWABLE_ZORDER_BASE then image
    int index;                  // in pHist->_items
    bool operator<(const ItemIndex& other) { return zorder < other.zorder; }
};
using ItemIndexVector = QVector<ItemIndex>;  // ordered by 'zorder'

using IntVector = QVector<int>;


// ******************************************************
//  KELL EZ???
// 
//----------------------------- MyPainter -------------------
#if 0
#include <QPainter>
class FalconPainter
{
    QImage *_canvas = nullptr;
    QFont _font;
    QColor _penColor, _brushColor;
    int _penWidth;
    Qt::PenStyle _penStyle = Qt::SolidLine;
    QPainter::CompositionMode _compMode;
public:
    FalconPainter(QImage *canvas=nullptr):_canvas(canvas) {}

    inline void SetFont(QFont& font) { _font = font; }
    inline void FontFromString(QString sfont) { _font.fromString(sfont); }
    inline QFont Font() const {
        return  _font;
    }
    inline QString FontToString() const { return _font.toString();  }
    inline void SetCompositionMode(QPainter::CompositionMode cm) { _compMode = cm; }
    inline void SetPen(QColor& penColor, int width = 0, Qt::PenStyle style=Qt::SolidLine) 
    { 
        _penColor = penColor;
        if(width > 0) 
            _penWidth = width;
        _penStyle = style;
    }
    inline void SetPen(QString colorName, int width= -1, Qt::PenStyle style = Qt::SolidLine) { _penColor = colorName; }
    inline void SetPenStyle(Qt::PenStyle penStyle) { _penStyle = penStyle; }

    void DrawLine(); // uses _brushColor
};
#endif

// ******************************************************
//----------------------------- DrawColors -------------------
// object created in DrawArea.cpp
class DrawColors
{
    QColor _invalid;
    bool _dark = false;
    struct _clr
    {
        FalconPenKind kind;
        QColor lightColor,    // _dark = false - for light mode
               darkColor;     // _dark = true  - for dark mode
        QString lightName, darkName; // Menu names for dark and light modes
        _clr() : kind(penNone), lightColor(QColor()), darkColor(QColor()) { }
    } _colors[5];
    const int _COLOR_COUNT = 5;

    int _penColorIndex(FalconPenKind pk)
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
        _colors[0].lightName = QObject::tr("Blac&k");
        _colors[0].darkName  = QObject::tr("&White");

        _colors[1].kind = penRed;
        _colors[1].lightColor = Qt::red;
        _colors[1].darkColor = Qt::red;
        _colors[1].lightName = QObject::tr("&Red");
        _colors[1].darkName  = QObject::tr("&Red");

        _colors[2].kind = penGreen;
        _colors[2].lightColor = "#007d1a";
        _colors[2].darkColor = Qt::green;
        _colors[2].lightName = QObject::tr("&Green");
        _colors[2].darkName  = QObject::tr("&Green");

        _colors[3].kind = penBlue;
        _colors[3].lightColor = Qt::blue;
        _colors[3].darkColor = "#82dbfc";
        _colors[3].lightName = QObject::tr("&Blue");
        _colors[3].darkName  = QObject::tr("&Blue");

        _colors[4].kind = penYellow;
        _colors[4].lightColor = "#b704be";
        _colors[4].darkColor = Qt::yellow;
        _colors[4].lightName = QObject::tr("&Purple");
        _colors[4].darkName  = QObject::tr("&Yellow");
    }
    bool SetDarkMode(bool dark) // returns previous mode
    {
        bool b = _dark;
        _dark = dark;
        return b;
    }
    bool IsDarkMode() const { return _dark; }
    QColor& operator[](FalconPenKind pk)
    {
        int i = _penColorIndex(pk);
        return   (i < 0 ? _invalid : (_dark ? _colors[i].darkColor : _colors[i].lightColor));
    }
    QString &ActionName(FalconPenKind pk)
    { 
        static QString what = "???";
        int i = _penColorIndex(pk);
        return (i < 0 ? what : (_dark ? _colors[i].darkName : _colors[i].lightName));
    }
};

extern DrawColors drawColors;       // in drawarea.cpp

#endif // _COMMON_H