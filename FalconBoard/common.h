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
#include <QApplication>
#include <QMainWindow>
#include <QDir>
#include <QSettings>
#include <QPageSize>
#include <QSplashScreen>

extern quint32 file_version_loaded;	// set in history::Load

#if !defined _VIEWER && defined _DEBUG
    extern bool    debugMode;     // can only be toggled when debug compile and Ctrl+Alt+D is pressed 
    extern bool    pencilMode;    // pen width is not used                      Ctrl+Shift+D
#endif

const QString sWindowTitle =
#ifdef _VIEWER
        "FalconBoard Viewer";
#else
        "FalconBoard";
#endif
enum class ScreenMode { smUseDefault=-1, smSystem, smLight, smWhite, smDark, smBlack, };

using IntVector = QVector<int>;
inline constexpr qreal EqZero(qreal a) { return qFuzzyIsNull(qAbs(a)); }

const int MAX_DRAWABLE_ID  = 0x7F;

const QColor CHANGED_MARKER_FOREGROUND = "white",
             CHANGED_MARKER_BACKGROUND = "red";

extern QString FB_WARNING,      // in FalconBoard.cpp
               FB_ERROR;

extern QString UNTITLED;     // in falconBoard.cpp

constexpr int penNone=-1,           // default color is none    // not a pen, not included in PEN_COUNT
              penEraser=0,          //  transparent
                                    // light  / dark
              penBlackOrWhite = 1,  // black  / white
              penT2 = 2,            // red    / red
              penT3 = 3,            // green  / green
              penT4 = 4,            // blue   / light blue
              penT5 = 5,            // purple / yellow
              penT6 = 6,            // 
              penT7 = 7,            // gray / gray
              PEN_COUNT = 8;

using FalconPenKind = int;

//enum FalconPenKind { penNone=-1, penEraser=0, penBlackOrWhite, penT2, penT3, penT4, penT5, PEN_COUNT};

// cursors for drawing: arrow, cross for draing, opena and closed hand for moving, 
enum DrawCursorShape { csArrow, csCross, csOHand, csCHand, csPen, csEraser };

enum SaveResult 
    { 
        srCancelled = -1, 
        srFailed,           // write error
        srSaveSuccess, 
        srNoNeedToSave,
        srNoSave            // user do not want it saved
    };
enum MyFontStyle { mfsNormal, mfsBold, mfsItalic, mfsSubSupScript, msfAllCaps};

void ShowSplashScreen(bool addText = false);
void CloseSplashScreen();

constexpr const int resos[] = { 300, 600, 1200 };;

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
    FalconPenKind kind = penBlackOrWhite;
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
        if (!sDName.isEmpty())
            darkName = sDName;
    }

    FalconPen(const FalconPen& o) :
        kind(o.kind), lightColor(o.lightColor), darkColor(o.darkColor),
        lightName(o.lightName), darkName(o.darkName),
        defaultLightColor(o.defaultLightColor), defaultDarkColor(o.defaultDarkColor) {}

    FalconPen& operator=(const FalconPen& o)
    {
        kind = o.kind; 
        lightColor = o.lightColor; 
        darkColor = o.darkColor;
        lightName = o.lightName; 
        darkName = o.darkName;
        defaultLightColor = o.defaultLightColor; 
        defaultDarkColor = o.defaultDarkColor;
        return *this;
    }

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
using FalconPenVector = std::vector<FalconPen>;
class FalconPens : public FalconPenVector
{
    bool _darkMode = false;     // use SetDarkMode to set up

    QIcon _RecolorIcon(QIcon sourceIcon, QColor colorW, QColor colorB) const;
    QCursor _SetupEraser() const;
public:
    FalconPens() { resize(PEN_COUNT); }
    FalconPens(const FalconPens& o) :FalconPenVector(*this) { *this = o; }
    FalconPens& operator=(const FalconPens& o);

    static const QString ItemName(int grp, int pen, const char* penName);
    static void Separate(const QString& from, QString& s1, QString& s2);
    static QString Merge(const  QString& s1, const QString& s2);

    bool SetDarkMode(bool dark);    // returns if previous mode was that
    bool SetupPen(FalconPenKind pk, QColor lc, QColor dc, QString sLName, QString sDName, bool setDefaultColors);  // and pointer
    bool SetupPen(FalconPenKind pk, QString lc_dc, QString sl_sdName, bool setDefaultColors);

    QColor Color(FalconPenKind pk, int dark = -1) const;  //-1: use _darkMode
    QCursor Pointer(FalconPenKind pk) const;
    const QString& ActionText(FalconPenKind pk, int dark = -1) const; //-1: use _darkMode
    void SetActionText(FalconPenKind pk, QString text, int dark = -1);

    bool IsAnyPensChanged();
};

//----------------------------- DrawColors -------------------
class DrawColors
{
    ScreenMode _mode = ScreenMode::smSystem;
    static FalconPens _defaultPens; // pens a new program instance starts with May be redefined
                                    // may differ from the base pen defaults used in the color
                                    // redefining dialog
    FalconPens _pens;     // no color for 'penNone'
    FalconPenKind _pkActual = penNone;

    inline bool _IsDarkMode(ScreenMode mode = ScreenMode::smUseDefault) const
    {
        //bool dark = (QApplication::palette().color(QPalette::WindowText).value() >
        //             QApplication::palette().color(QPalette::Window).value() ) || _mode > ScreenMode::smWhite;  // for system colors
        bool isSystemDark = QApplication::palette().window().color().value() < QApplication::palette().windowText().color().value();
        if (mode == ScreenMode::smUseDefault)
            mode = _mode;

        if (mode == ScreenMode::smSystem)
            return isSystemDark;
        else 
            return mode > ScreenMode::smWhite;
    }
public:
    DrawColors() {}
    DrawColors(const DrawColors& o) { *this = o; }

    DrawColors& operator= (const DrawColors& o);

    void Initialize() // call after the GUI is initialized! 
    { 
        SetBaseColorsInto(_pens); 
    }

    bool SameColors(const DrawColors& o);
    ScreenMode ActualMode() const { return _mode; }
    QColor  Color(FalconPenKind pk = penNone, ScreenMode mode=ScreenMode::smUseDefault) const;       // penNone and dark < 0 for actual pen
    QCursor PenPointer(FalconPenKind pk=penNone) const;    // for actual pen
    QString ActionText(FalconPenKind pk = penNone, ScreenMode mode = ScreenMode::smUseDefault) const;
    bool IsDarkMode() const 
    {
        return _IsDarkMode();
    }
    bool IsChanged() { return _pens.IsAnyPensChanged(); }

    void SetBaseColorsInto(FalconPens& pens);
    bool SetDarkMode(ScreenMode mode = ScreenMode::smUseDefault);
    void SetActualPen(FalconPenKind pk);
    void SetupPenAndCursor(FalconPenKind pk, QColor lightcolor, QColor darkcolor, QString sLightColorUserName=QString(), QString sDarkColorUserName=QString());
    void SetActionText(FalconPenKind pk, QString text, ScreenMode mode = ScreenMode::smUseDefault);  // used from 'pencolors'
        // global pen colors
    void SetDefaultPen(FalconPenKind pk, QColor lc, QColor dc, QString &ln, QString &dn);
    bool DefaultsToSettings() const;
    bool DefaultsFromSettings();
        // pen colors for files read
    bool ReadPen(QDataStream& ifs);    // only call if drawable type is dtPen! first data to be read is pen kind
    QDataStream& SavePen(QDataStream& ifs, FalconPenKind pk);    // only saved if pen is changed saves dtPen as first type
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