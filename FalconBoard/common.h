#pragma once
#ifndef _COMMON_H
#define _COMMON_H

#include <vector>

#include <QVector>
#include <QString>
#include <QBitmap>
#include <QIcon>
#include <QColor>
#include <QPainter>
#include <QMainWindow>
#include <QDir>
#include <QSettings>
#include <QPageSize>

extern quint32 file_version_loaded;	// set in history::Load

const QString sWindowTitle =
#ifdef _VIEWER
        "FalconBoard Viewer";
#else
        "FalconBoard";
#endif

using IntVector = QVector<int>;
const qreal eps = 1e-4;         // for 0 test of double
inline constexpr qreal EqZero(qreal a) { return qAbs(a) < eps; }

const int MAX_DRAWABLE_ID  = 0x7F;

const QColor CHANGED_MARKER_FOREGROUND = "white",
             CHANGED_MARKER_BACKGROUND = "red";

extern QString FB_WARNING,      // in FalconBoard.cpp
               FB_ERROR;

extern QString UNTITLED;     // in falconBoard.cpp

constexpr int PEN_COUNT = 6;    // including eraser, no 'penNone' pen! Modify if pen count changes !
enum FalconPenKind { penNone=-1, penEraser=0, penBlack, penRed, penGreen, penBlue, penYellow, penLastNotUseThis};

// cursors for drawing: arrow, cross for draing, opena and closed hand for moving, 
enum DrawCursorShape { csArrow, csCross, csOHand, csCHand, csPen, csEraser };

enum SaveResult { srCancelled = -1, srFailed, srSaveSuccess, srNoSave};
enum MyFontStyle { mfsNormal, mfsBold, mfsItalic, mfsSubSupScript, msfAllCaps};

constexpr const int resos[] = { 300, 600, 1200 };;

inline int IsUntitled(QString name)
{
    return name.isEmpty() ? 1 : (name.left(UNTITLED.length()) == UNTITLED ? 2 : 0);
}

struct MyPageSize
{
    double w=-1, h=-1;
    int index=-1;
    QPageSize::PageSizeId pid = QPageSize::A4;
    MyPageSize() {}
    MyPageSize(double w, double h, int ix, QPageSize::PageSizeId pid) : w(w), h(h), index(ix), pid(pid) {}
    QPageSize PageSize() { return QPageSize(pid); }
};

class MyPageSizes : public std::vector<MyPageSize>
{
public:
    MyPageSizes();
    //MyPageSize operator[](QPageSize::PageSizeId size);
};
extern MyPageSizes myPageSizes; // in common.cpp

struct MyScreenSize
{
    int w, h;
    MyScreenSize(int w, int h) : w(w), h(h) {}
};
class MyScreenSizes : public std::vector<MyScreenSize>
{
public:
    MyScreenSizes();
};

extern MyScreenSizes myScreenSizes;
     
//******************************************************
struct FalconPen
{
    FalconPenKind kind = penBlack;
    QColor lightColor = Qt::black,    // _dark = false - for light mode
        darkColor = Qt::white;        // _dark = true  - for dark mode
    QString lightName, darkName;      // Menu names for dark and light modes
    QColor defaultLightColor = "black", defaultDarkColor = "white";

    FalconPen() {}

    FalconPen(FalconPenKind pk, QColor lc, QColor dc, QString sLName, QString sDName) :
        kind(pk), lightColor(lc), darkColor(dc)
    {
        if (!sLName.isEmpty())
            lightName = sLName;
        if(!sDName.isEmpty())
            darkName = sDName;
    }

    FalconPen(const FalconPen &o) : 
        kind(o.kind), lightColor(o.lightColor), darkColor(o.darkColor), 
        lightName(o.lightName), darkName(o.darkName),
        defaultLightColor(o.defaultLightColor), defaultDarkColor(o.defaultDarkColor) {}

    bool IsChanged() const
    {
        return (defaultLightColor != lightColor) || (defaultDarkColor != darkColor);
    }

    void SetDefaultColors(QColor dlc, QColor ddc)
    {
        defaultLightColor = dlc;
        defaultDarkColor = ddc;
    }
};

//----------------------------- FalconPens -------------------
class FalconPens : public std::vector<FalconPen>
{
    QCursor _pointers[PEN_COUNT];
    bool _darkMode = false;     // use SetDarkMode to set up

    QIcon _RecolorIcon(QIcon sourceIcon, QColor colorW, QColor colorB);
    void _SetupEraser();
    void _PreparePointerFor(FalconPenKind pk);
    void _PreparePointers();
public:
    FalconPens() { resize(PEN_COUNT); }
    void Initialize();

    FalconPens(const FalconPens& o) { *this = o; }
    FalconPens& operator=(const FalconPens& o);

    bool SetDarkMode(bool dark);
    bool SetupPen(FalconPenKind pk, QColor lc, QColor dc, QString sLName, QString sDName, bool setDefaults);  // and pointer
    bool SetupPen(FalconPenKind pk, QString lc_dc, QString sl_sdName);

    QColor Color(FalconPenKind pk, int dark = -1) const;  //-1: use _darkMode
    QCursor Pointer(FalconPenKind pk) const;
    const QString& ActionText(FalconPenKind pk, int dark = -1) const; //-1: use _darkMode
    void SetActionText(FalconPenKind pk, QString text, int dark = -1);

    bool IsAnyPensChanged();

    bool FromSettings(QSettings* s);
    bool ToSettings(QSettings* s) const;
};

//----------------------------- DrawColors -------------------
class DrawColors
{
    bool _dark = false;
    FalconPens _pens;     // no color for 'penNone'
    FalconPenKind _pkActual = penNone;
public:
    DrawColors() {}
    DrawColors(const DrawColors& o) { *this = o; }

    DrawColors& operator= (const DrawColors& o);

    void Initialize() { _pens.Initialize(); }

    bool SameColors(const DrawColors& o);
    QColor  Color(FalconPenKind pk = penNone, int dark=-1) const;       // penNone and dark < 0 for actual pen
    QCursor PenPointer(FalconPenKind pk=penNone) const;    // for actual pen
    QString ActionText(FalconPenKind pk = penNone, int dark =-1) const;
    bool IsDarkMode() const { return _dark; }
    bool IsChanged() { return _pens.IsAnyPensChanged(); }

    bool SetDarkMode(bool dark);
    void SetDrawingPen(FalconPenKind pk);
    void SetupPenAndCursor(FalconPenKind pk, QColor lightcolor, QColor darkcolor, QString sLightColorUserName=QString(), QString sDarkColorUserName=QString());
    void SetActionText(FalconPenKind pk, QString text, bool dark);  // used from 'pencolors'
        // global pen colors
    bool ToSettings(QSettings* s);
    bool FromSettings(QSettings* s);
        // pen colors for files read
    bool ReadPen(QDataStream& ifs);    // only call if drawable type is dtPen! first data to be read is pen kind
    QDataStream& SavePen(QDataStream& ifs, FalconPenKind pk);    // only saved if pen is changed saves dtPen as first type
    void SetPens(FalconPens &pens);             // used when history changed Don't forget to change the corresponding actions in 'FalconBoard.cpp'
};
extern DrawColors globalDrawColors;

/*=============================================================
 * TASK:    centralized settings handler
 *      static members are initialized in FalconBoard.cpp!
 *------------------------------------------------------------*/
struct FBSettings
{
    static QString homePath;       // path for user's home directory
    static void Init();            // set home path for falconBoard
    static QString Name();
    static QSettings* Open();
    static void Close() { delete _ps; }

private:
    static QSettings* _ps;

};

struct FLAG {
    int b = 0;
    int operator++() { ++b; return b; }
    int operator--() { if (b) --b; return b; }
    operator bool() { return b; }
};


#endif // _COMMON_H