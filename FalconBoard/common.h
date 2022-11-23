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
#include <QPageSize>
// version number 0xMMIISS;     M - major, I-minor, s- sub
constexpr int DRAWABLE_ZORDER_BASE = 10000000;  // zOrder for all images is below this number

const qint32 MAGIC_ID = 0x53414d57; // "SAMW" - little endian !! MODIFY this and Save() for big endian processors!
const qint32 MAGIC_VERSION = 0x56020001; // V 02.00.01      Cf.w. common.h
const QString sVersion = "2.0.1";

const qreal eps = 1e-4;         // for 0 test of double
inline constexpr qreal EqZero(qreal a) { return qAbs(a) < eps; }
//const QChar CHANGE_MARKER_CHAR = QChar(0x2757); /*red exclamation point*/
//const QChar CHANGE_MARKER_CHAR = QChar(0x274c);/* red x */
const QChar CHANGE_MARKER_CHAR = QChar(0x26d4);/* no entry traffic sign */

extern QString FB_WARNING,      // ins FalconBoard.cpp
              FB_ERROR;


const QString sWindowTitle =
#ifdef _VIEWER
        "FalconBoard Viewer";
#else
        "FalconBoard";
#endif

struct MyPageSizes
{
    double w, h;
    int index;
    QPageSize::PageSizeId pid;
    QPageSize PageSize() { return QPageSize(pid); }
};

constexpr MyPageSizes myPageSizes[] =
{    // in inches as resolution is always in dots / inch
                //{ 1.041666667,  1.458333333,  0, QPageSize::}, //   A10
                //{ 1.458333333,  2.041666667,  1, QPageSize::}, //   A9	
                //{ 2.041666667,  2.916666667,  2, QPageSize::}, //   A8	
                { 2.916666667,  4.125000000,  3, QPageSize::A7}, //   A7	
                { 4.125000000,  5.833333333,  4, QPageSize::A6}, //   A6	
                { 5.833333333,  8.250000000,  5, QPageSize::A5}, //   A5	
                { 8.267716535, 11.692913386,  6, QPageSize::A4}, //   A4	
                {11.708333333, 16.541666667,  7, QPageSize::A3}, //   A3	
                {16.541666667, 23.375000000,  8, QPageSize::A2}, //   A2	
                {23.375000000, 33.125000000,  9, QPageSize::A1}, //   A1	
                {33.125000000, 46.791666667, 10, QPageSize::A0}, //   A0 

                //{ 1.208333333,  1.750000000, 11, QPageSize::}, //   B10
                //{ 1.750000000,  2.458333333, 12, QPageSize::}, //   B9 
                //{ 2.458333333,  3.458333333, 13, QPageSize::}, //   B8 
                { 3.458333333,  4.916666667, 14, QPageSize::B7}, //   B7 
                { 4.916666667,  6.916666667, 15, QPageSize::B6}, //   B6 
                { 6.916666667,  9.833333333, 16, QPageSize::B5}, //   B5 
                { 9.833333333, 13.916666667, 17, QPageSize::B4}, //   B4 
                {13.916666667, 19.666666667, 18, QPageSize::B3}, //   B3 
                {19.666666667, 27.833333333, 19, QPageSize::B2}, //   B2 
                {27.833333333, 39.375000000, 20, QPageSize::B1}, //   B1 
                {39.375000000, 55.666666667, 21, QPageSize::B0}, //   B0 

                {8.5, 11 , 22, QPageSize::Letter}, // US Letter, 
                {2.5, 14 , 23, QPageSize::Legal}, // US Legal,     
                {5.5, 8.5, 24, QPageSize::B6}  // US Stationary, // ?????????
};

struct MyScreenSizes
{
    int w, h;
};

constexpr const MyScreenSizes myScreenSizes[] =
{
    {3840,   2160},
    {3440,   1440},
    {2560,   1440},
    {2560,   1080},
    {2048,   1152},
    {1920,   1200},
    {1920,   1080},
    {1680,   1050},
    {1600,   900 },
    {1536,   864 },
    {1440,   900 },
    {1366,   768 },
    {1360,   768 },
    {1280,   1024},
    {1280,   800 },
    {1280,   720 },
    {1024,   768 },
    {800 ,   600 },
    {640 ,   360 }
};


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
    pfDontPrintImages       = 32,
    pfOpenPDFInViewer       = 64
};
enum SaveResult { srCancelled = -1, srFailed, srSaveSuccess, srNoSave};
enum MyFontStyle { mfsNormal, mfsBold, mfsItalic, mfsSubSupScript, msfAllCaps};

constexpr const int resos[] = { 300, 600, 1200 };;


using IntVector = QVector<int>;

extern QString UNTITLED;     // in falconBoard.cpp

inline int IsUntitled(QString name)
{
    return name.isEmpty() ? 1 : (name.left(UNTITLED.length()) == UNTITLED ? 2 : 0);
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