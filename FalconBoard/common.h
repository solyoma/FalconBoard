#pragma once
#ifndef _COMMON_H
#define _COMMON_H

#include <QBitmap>
#include <QIcon>
#include <QColor>
#include <QPainter>
#include <QMainWindow>
#include <QDir>
#include <QSettings>
// version number 0xMMIISS;     M - major, I-minor, s- sub
const long nVersion = 0x00010109;       // program version
const QString sVersion = "1.1.9";
const QString sWindowTitle =
#ifdef _VIEWER
        "FalconBoard Viewer";
#else
        "FalconBoard";
#endif


constexpr int PEN_COUNT = 6;    // no 'penNone' pen modify if pen count changes
enum FalconPenKind { penNone=0, penBlack=1, penRed=2, penGreen=3, penBlue=4, penYellow=5, penEraser=PEN_COUNT};
// cursors for drawing: arrow, cross for draing, opena and closed hand for moving, 
enum DrawCursorShape { csArrow, csCross, csOHand, csCHand, csPen, csEraser };

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
    bool operator<(const ItemIndex& other) const noexcept { return zorder < other.zorder; }
    bool operator==(const ItemIndex& other) const noexcept { return index == other.index; }
};
using ItemIndexVector = QVector<ItemIndex>;  // ordered by 'zorder'

using IntVector = QVector<int>;

const QString UNTITLED = QString(QMainWindow::tr("Untitled"));

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

inline bool IsUntitled(QString name)
{
    return name.isEmpty() || name.left(UNTITLED.length()) == UNTITLED;
}

// ******************************************************
/*========================================================
 * TASK: from a single QIcon create icons for all used colors
 * PARAMS:  icon - base icon (png with white and black color)
 *          colorW - replacement color for white must be valid
 *          colorB - replacement colors for black,
                     may be invalid
 * GLOBALS:
 * RETURNS: new icon with new colors set
 * REMARKS: -
 *-------------------------------------------------------*/
// ******************************************************
//----------------------------- DrawColors -------------------
// object created in DrawArea.cpp
class DrawColors
{
    QColor _invalid;
    bool _dark = false;
    struct _clr
    {
        FalconPenKind kind = penBlack;
        QColor lightColor = Qt::black,    // _dark = false - for light mode
               darkColor = Qt::white;     // _dark = true  - for dark mode
        QString lightName, darkName; // Menu names for dark and light modes
        _clr() : kind(penNone), lightColor(QColor()), darkColor(QColor()) { }
    } _colors[PEN_COUNT];     // no color for 'penNone'
    QCursor _cursors[PEN_COUNT];    // no cursor for penNone

    int _penColorIndex(FalconPenKind pk)
    {
        for (int i = 0; i < PEN_COUNT; ++i)
            if (_colors[i].kind == pk)
                return i;
        return -1;
    }
    void _Setup(FalconPenKind pk, QColor lc, QColor dc, QString sLName, QString sDName)
    {
        int ix = (int)pk - 1;
        if (pk != penEraser)        // else special handling
		{
			_colors[ix].kind = pk;
			_colors[ix].lightColor = lc;
			_colors[ix].darkColor = dc;
			_colors[ix].lightName = sLName;
			_colors[ix].darkName = sDName;
		}
    }
public:
    void Setup()
    {
        _Setup(penBlack, Qt::black, Qt::white,  QObject::tr("Blac&k" ),  QObject::tr("Blac&k") );
        _Setup(penRed,   Qt::red,   Qt::red,    QObject::tr("&Red"   ),  QObject::tr("&Red") );
        _Setup(penGreen, "#007d1a", Qt::green,  QObject::tr("&Green" ),  QObject::tr("&Green") );
        _Setup(penBlue , Qt::blue, "#82dbfc",   QObject::tr("&Blue"  ),  QObject::tr("&Blue") );
        _Setup(penYellow,"#b704be", Qt::yellow, QObject::tr("&Purple"), QObject::tr("&Yellow") );

        _Setup(penEraser, Qt::white, Qt::white, QObject::tr("&Eraser"), QObject::tr("&Eraser") );
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

class PenCursors 
{
    QCursor _cursors[PEN_COUNT];
    bool    _wasSetup[PEN_COUNT] = { false };

	QIcon ColoredIcon(QIcon sourceIcon, QColor colorW, QColor colorB)
	{
		QPixmap pm, pmW, pmB;
		pmW = pmB = pm = sourceIcon.pixmap(64, 64);
		QBitmap mask = pm.createMaskFromColor(Qt::white, Qt::MaskOutColor);

		pmW.fill(colorW);
		pmW.setMask(mask);
		if (colorB.isValid())
		{
			mask = pm.createMaskFromColor(Qt::black, Qt::MaskOutColor);
			pmB.fill(colorB);
			pmB.setMask(mask);
			QPainter painter(&pmW);
			painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
			painter.drawPixmap(0, 0, pm.width(), pm.height(), pmB);
		}
		return QIcon(pmW);
	}
    void _SetupEraser()
    {
        QIcon icon = ColoredIcon(QIcon(":/FalconBoard/Resources/eraser.png"), drawColors.IsDarkMode() ? Qt::black : Qt::white, drawColors.IsDarkMode() ? Qt::white : Qt::black);
        QPixmap pm = icon.pixmap(64,64);
        _cursors[(int)penEraser-1] = QCursor(pm);
        _wasSetup[(int)penEraser-1] = true;
    }

    void _Setup(FalconPenKind pk)
    {
        constexpr int SIZE = 32;    //px
        int n = (int)pk - 1;
        QPixmap pm(SIZE, SIZE);
        pm.fill(Qt::transparent);
        QPainter painter(&pm);

        QColor color = drawColors[pk];
        painter.setPen(QPen(color, 2, Qt::SolidLine));
        painter.drawLine(QPoint(0, SIZE/2), QPoint(SIZE-1, SIZE/2));
        painter.drawLine(QPoint(SIZE/2, 0), QPoint(SIZE/2, SIZE-1));
        _cursors[n] = QCursor(pm);
        _wasSetup[n] = true;
    }
public:
    void Setup()
    {
        _Setup(penBlack);
        _Setup(penRed);
        _Setup(penGreen);
        _Setup(penBlue);
        _Setup(penYellow);
        _SetupEraser();
    }
    QCursor operator[](FalconPenKind pk)
    {
        if (pk == penNone)
            return QCursor();
        int ix = (int)pk - 1;
        if (!_wasSetup[ix])
            _Setup(pk);
        return _cursors[ix];
    }
};
extern PenCursors penCursors;       // in drawarea.cpp

/*=============================================================
 * TASK:    centralized settings handler
 *      static members are initialized in FalconBoard.cpp!
 *------------------------------------------------------------*/
struct FBSettings
{
    static QString homePath;       // path for user's home directory
    static void Init()             // set home path for falconBoard
    {
        homePath = QDir::homePath() +
#if defined (Q_OS_Linux)   || defined (Q_OS_Darwin) || defined(__linux__)
            "/.falconBoard";
#elif defined(Q_OS_WIN)
            "/Appdata/Local/FalconBoard";
#endif
    }
    static QString Name()
    {
        return homePath +
#ifdef _VIEWER
            "/FalconBoardViewer.ini";
#else
            "/FalconBoard.ini";
#endif        
    }

    static QSettings *Open()
    {
        return _ps = new QSettings(Name(), QSettings::IniFormat);
    }
    static void Close() { delete _ps; }

private:
    static QSettings* _ps;

};

#endif // _COMMON_H