#include "common.h"
#include "config.h"
#include "drawables.h"

    // global variables
quint32 file_version_loaded = 0;	// aasuume old file, no grid data in _history->gridOptions yet

MyPageSizes myPageSizes;
DrawColors globalDrawColors;
MyScreenSizes myScreenSizes;


MyPageSizes::MyPageSizes()
{
        //              w               h        index   pid
        // in inches as resolution is always in dots / inch
                   //{ 1.041666667,  1.458333333,  0, QPageSize::}, //   A10
                   //{ 1.458333333,  2.041666667,  1, QPageSize::}, //   A9	
                   //{ 2.041666667,  2.916666667,  2, QPageSize::}, //   A8	
    push_back( MyPageSize(2.916666667,  4.125000000,  3, QPageSize::A7) ); //   A7	
    push_back( MyPageSize( 4.125000000,  5.833333333,  4, QPageSize::A6) ); //   A6	
    push_back( MyPageSize( 5.833333333,  8.250000000,  5, QPageSize::A5) ); //   A5	
    push_back( MyPageSize( 8.267716535, 11.692913386,  6, QPageSize::A4) ); //   A4	
    push_back( MyPageSize(11.708333333, 16.541666667,  7, QPageSize::A3) ); //   A3	
    push_back( MyPageSize(16.541666667, 23.375000000,  8, QPageSize::A2) ); //   A2	
    push_back( MyPageSize(23.375000000, 33.125000000,  9, QPageSize::A1) ); //   A1	
    push_back( MyPageSize(33.125000000, 46.791666667, 10, QPageSize::A0) ); //   A0 
    //{ 1.208333333,  1.750000000, 11, QPageSize::}, //   B10          ) );
    //{ 1.750000000,  2.458333333, 12, QPageSize::}, //   B9           ) );
    //{ 2.458333333,  3.458333333, 13, QPageSize::}, //   B8           ) );
    push_back( MyPageSize( 3.458333333,  4.916666667, 14, QPageSize::B7) ); //   B7 
    push_back( MyPageSize( 4.916666667,  6.916666667, 15, QPageSize::B6) ); //   B6 
    push_back( MyPageSize( 6.916666667,  9.833333333, 16, QPageSize::B5) ); //   B5 
    push_back( MyPageSize( 9.833333333, 13.916666667, 17, QPageSize::B4) ); //   B4 
    push_back( MyPageSize(13.916666667, 19.666666667, 18, QPageSize::B3) ); //   B3 
    push_back( MyPageSize(19.666666667, 27.833333333, 19, QPageSize::B2) ); //   B2 
    push_back( MyPageSize(27.833333333, 39.375000000, 20, QPageSize::B1) ); //   B1 
    push_back( MyPageSize(39.375000000, 55.666666667, 21, QPageSize::B0) ); //   B0 

    push_back( MyPageSize(8.5,          11,           22, QPageSize::Letter));// US Letter, 
    push_back( MyPageSize(2.5,          14,           23, QPageSize::Legal)); // US Legal,     
    push_back( MyPageSize(5.5,           8.5,         24, QPageSize::B6));  // US Stationary, // ?????????
}

//MyPageSize MyPageSizes::operator[](QPageSize::PageSizeId size)
//{
//    for (auto& v : *this)
//        if (v.pid == size)
//            return v;
//    return MyPageSize();
//}

extern MyPageSizes myPageSizes; // in pagesetup.cpp

MyScreenSizes::MyScreenSizes()
{
    push_back(MyScreenSize( 3840,   2160 ));
    push_back(MyScreenSize( 3440,   1440 ));
    push_back(MyScreenSize( 2560,   1440 ));
    push_back(MyScreenSize( 2560,   1080 ));
    push_back(MyScreenSize( 2048,   1152 ));
    push_back(MyScreenSize( 1920,   1200 ));
    push_back(MyScreenSize( 1920,   1080 ));
    push_back(MyScreenSize( 1680,   1050 ));
    push_back(MyScreenSize( 1600,    900 ));
    push_back(MyScreenSize( 1536,    864 ));
    push_back(MyScreenSize( 1440,    900 ));
    push_back(MyScreenSize( 1366,    768 ));
    push_back(MyScreenSize( 1360,    768 ));
    push_back(MyScreenSize( 1280,   1024 ));
    push_back(MyScreenSize( 1280,    800 ));
    push_back(MyScreenSize( 1280,    720 ));
    push_back(MyScreenSize( 1024,    768 ));
    push_back(MyScreenSize( 800 ,    600 ));
    push_back(MyScreenSize( 640 ,    360 ));
}

// ******************* FalconPens ***********************************
const char       // DEFAULTS
*PEN_DEFAULT_LIGHT_RED        = "#FF0000",
*PEN_DEFAULT_LIGHT_GREEN      = "#007D1A",
*PEN_DEFAULT_LIGHT_BLUE       = "#0055FF",
*PEN_DEFAULT_LIGHT_YELLOW     = "#B704BE",

*PEN_DEFAULT_DARK_RED         = "#FF0000",
*PEN_DEFAULT_DARK_GREEN       = "#00FF00",
*PEN_DEFAULT_DARK_BLUE        = "#82DBFC",
*PEN_DEFAULT_DARK_YELLOW      = "#FFFF00";

void FalconPens::Initialize()    // Set defaults. Call AFTER the GUI applcation created!
{
    SetupPen(penEraser, Qt::white, Qt::white, QObject::tr("&Eraser"), QObject::tr("&Eraser"),true);            // eraser must come first

    SetupPen(penBlack,  Qt::black   , Qt::white     , QObject::tr("Blac&k" ), QObject::tr("&White" ),true);

    SetupPen(penRed,    PEN_DEFAULT_LIGHT_RED      , PEN_DEFAULT_DARK_RED     , QObject::tr("&Red"   ), QObject::tr("&Red"   ), true);
    SetupPen(penGreen,  PEN_DEFAULT_LIGHT_GREEN    , PEN_DEFAULT_DARK_GREEN   , QObject::tr("&Green" ), QObject::tr("&Green" ), true);
    SetupPen(penBlue,   PEN_DEFAULT_LIGHT_BLUE     , PEN_DEFAULT_DARK_BLUE    , QObject::tr("&Blue"  ), QObject::tr("&Blue"  ), true);
    SetupPen(penYellow, PEN_DEFAULT_LIGHT_YELLOW   , PEN_DEFAULT_DARK_YELLOW  , QObject::tr("&Purple"), QObject::tr("&Yellow"), true);
}

FalconPens& FalconPens::operator=(const FalconPens& o)
{
    if (empty())
        Initialize();

    for (int i = penRed; i < penLastNotUseThis; ++i)
    {
        (*this)[(FalconPenKind)i] = o[(FalconPenKind)i];
        _pointers[i] = o._pointers[i];
    }
    return *this;
}

bool FalconPens::SetDarkMode(bool dark)
{
    bool res = _darkMode;
    _darkMode = dark;
    return res;
}

bool FalconPens::SetupPen(FalconPenKind pk, QColor lc, QColor dc, QString sLName, QString sDName, bool setDefaults)
{
    FalconPen pen(pk, lc, dc, sLName, sDName);
    if (setDefaults)
    {
        pen.SetDefaultColors(pen.lightColor, pen.darkColor);
    }

    if (pk != penNone)
    {
        int n = size(); //  (int)pk;
        if((int)pk >= size())  // a hole before pk in array
        {
            while (n++ <= (int)pk);
                push_back(pen);
        }
        else
            (*this)[(size_t)pk] = pen;

        _PreparePointerFor(pk);
    }
    return true;
}

bool FalconPens::SetupPen(FalconPenKind pk, QString lc_dc, QString sl_sdName)
{
    QStringList sc, sn;       // light,dark color and name
    sc = lc_dc.split(',');    
    sn = sl_sdName.split(',');
    if (sc.size() != 2 || sn.size() != 2)   // wrong data: do not change
        return false;

    return SetupPen(pk, sc[0], sc[1], sn[0], sn[1], false);         // ???
}

QColor FalconPens::Color(FalconPenKind pk, int dark) const
{
    int ix = (int)pk;   // ix <= 0 => pk == penNone or pk == penEraser
    if (dark < 0)
        dark = _darkMode;
    return  (ix <= 0 ? QColor() : (dark ? (*this)[size_t(pk)].darkColor : (*this)[size_t(pk)].lightColor));
}

QCursor FalconPens::Pointer(FalconPenKind pk) const
{
    return _pointers[(int)pk];
}

const QString& FalconPens::ActionText(FalconPenKind pk, int dark) const
{
    static QString what = "???";
    if (dark < 0)
        dark = _darkMode;
    return pk == penNone ? what : (dark ? operator[](int(pk)).darkName : operator[](int(pk)).lightName);
}

void FalconPens::SetActionText(FalconPenKind pk, QString text, int dark)
{
    if (text.isEmpty())
        return;

    if (dark < 0)
        dark = _darkMode;
    if(dark)
        (*this)[pk].darkName = text;
    else
        (*this)[pk].lightName = text;
}

bool FalconPens::IsAnyPensChanged()
{
    for (int i = 0; i < size(); ++i)
        if ((*this)[i].IsChanged())
            return true;
    return false;
}

bool FalconPens::FromSettings(QSettings* s) // may be overwritten in ".mwb" files
{
    QStringList sc, sN; 

    auto setit = [&](FalconPenKind pk, QString key, QString sDLc, QString sDLd, QString sDLn, QString sDDn)
        {
            key = QStringLiteral("PEN_") + key;
            QString nameKey = key + QStringLiteral("_NAME");
            if (SetupPen(pk, s->value(key, sDLc).toString(), s->value(key, sDLd).toString(), s->value(nameKey, sDLn).toString(), s->value(key, sDDn).toString(), true))
            {
                FalconPen* pen = &(*this)[(int)pk];
                pen->SetDefaultColors(pen->lightColor, pen->darkColor);
                _PreparePointerFor(pk);
            }
        };
    // always B&W ! setit(penBlack, "BLACK", "black,white", "black,black");
    setit(penRed,   "RED",   PEN_DEFAULT_LIGHT_RED   , PEN_DEFAULT_DARK_RED   , QObject::tr("&Red"   ),QObject::tr("&Red"   ) );
    setit(penGreen, "GREEN", PEN_DEFAULT_LIGHT_GREEN , PEN_DEFAULT_DARK_GREEN , QObject::tr("&Green" ),QObject::tr("&Green" ) );
    setit(penBlue,  "BLUE",  PEN_DEFAULT_LIGHT_BLUE  , PEN_DEFAULT_DARK_BLUE  , QObject::tr("&Blue"  ),QObject::tr("&Blue"  ) );
    setit(penYellow,"YELLOW",PEN_DEFAULT_LIGHT_YELLOW, PEN_DEFAULT_DARK_YELLOW, QObject::tr("&Purple"),QObject::tr("&Yellow") );

    return true;
}

bool FalconPens::ToSettings(QSettings* s) const
{
#ifndef _VIEWER
    // never save black pen, it is the same always
    auto saveit = [&](FalconPenKind pk, QString key)
        {
            key = QStringLiteral("PEN_") + key;
            QString nameKey = key + QStringLiteral("_NAME");

            const FalconPen* pen = &(*this)[pk];
            if (pen->IsChanged())
            {
                s->setValue(key, pen->lightColor.name() + "," + pen->darkColor.name());
                s->setValue(nameKey, pen->lightName+ "," + pen->darkName);
            }
            else
            {
                s->remove(key);
                s->remove(nameKey);
            }
        };
    saveit(penRed,   "RED"   );
    saveit(penGreen, "GREEN" );
    saveit(penBlue,  "BLUE"  );
    saveit(penYellow,"YELLOW");
#endif

    return true;
}
//----------------------------- DrawColors -------------------

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
 // object created in DrawArea.cpp

DrawColors& DrawColors::operator= (const DrawColors& o)
{
    if (!SameColors(o))
    {
        _pens = o._pens;
    }
    _dark = o._dark;
    _pkActual = o._pkActual;

    return *this;
}

bool DrawColors::SameColors(const DrawColors& o)
{
    if (_pens.empty())
        return false;

    for (int i = penRed; i < PEN_COUNT; ++i) // black are always the same
        if (_pens[i].lightColor != o._pens[i].lightColor ||
            _pens[i].darkColor != o._pens[i].darkColor)
            return false;
    return true;
}

bool DrawColors::SetDarkMode(bool dark) // returns previous mode
{
    bool b = _dark;
    if (b != dark)
    {
        _dark = dark;
        _pens.SetDarkMode(dark);
    }
    return b;
}

void DrawColors::SetDrawingPen(FalconPenKind pk)
{
    if (pk == _pkActual)
        return;
    _pkActual = pk;
}

bool DrawColors::ReadPen(QDataStream& ifs)      // type already read in
{
    int cnt = 0;
    int n;
    ifs >> n; // 'pk'
    if (n > 1 && n < PEN_COUNT) // no pen data may be read for none or eraser or black
    {
        FalconPenKind pk = (FalconPenKind)n;
        QString sl,sd;
        ifs >> sl >> sd;    // name of light color, dark color (#AARRGGBB)
        QColor qcl = sl, qcd = sd;

        ifs >> sl >> sd;    // user names of light and dark
        SetupPenAndCursor(pk, qcl, qcd, sl, sd);
        ++cnt;
    }
    return cnt;
}

QDataStream& DrawColors::SavePen(QDataStream& ofs, FalconPenKind pk)
{
    const FalconPen &pen = _pens[pk];
    if (pen.IsChanged())
    {
        ofs << DrawableType::dtPen; // == dtPen, see drawables.h
        ofs << (int)pk << pen.lightColor.name() << pen.darkColor.name()
            << pen.lightName << pen.darkName;
    }
    return ofs;
}

void DrawColors::SetupPenAndCursor(FalconPenKind pk, QColor lightcolor, QColor darkcolor, QString sLightColorUserName, QString sDarkColorUserName)
{
    _pens.SetupPen(pk, lightcolor, darkcolor, sLightColorUserName, sDarkColorUserName, false);
}

void DrawColors::SetActionText(FalconPenKind pk, QString text, bool dark)
{
    _pens.SetActionText(pk, text, dark);
}

bool DrawColors::ToSettings(QSettings* s)
{
    return _pens.ToSettings(s);
}

bool DrawColors::FromSettings(QSettings* s)
{
    return _pens.FromSettings(s);
}

QColor DrawColors::Color(FalconPenKind pk, int dark) const
{
    if (pk == penNone)
        pk = _pkActual;
    if (dark < 0)
        dark = _dark;
//    qDebug("pk:%d, %s, color:%s - %s:line #%ld", pk, (dark? "dark " : "light"), _pens.Color(pk, dark).name().toStdString().c_str(), __FILE__, __LINE__);
    return _pens.Color(pk, dark);
}

QCursor DrawColors::PenPointer(FalconPenKind pk) const
{
    if (pk == penNone)
        pk = _pkActual;
    return _pens.Pointer(pk);
}

QString DrawColors::ActionText(FalconPenKind pk, int dark)
{
    if (pk == penNone)
        pk = _pkActual;
    return _pens.ActionText(pk, dark);
}

QIcon FalconPens::_RecolorIcon(QIcon sourceIcon, QColor colorW, QColor colorB)
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
void FalconPens::_SetupEraser()
{
    QIcon icon = _RecolorIcon(QIcon(":/FalconBoard/Resources/eraser.png"), _darkMode ? Qt::black : Qt::white, _darkMode ? Qt::white : Qt::black);
    QPixmap pm = icon.pixmap(64, 64);
    _pointers[(int)penEraser] = QCursor(pm);
}

void FalconPens::_PreparePointerFor(FalconPenKind pk)
{
    if (pk == penNone)
        return;
    if (pk == penEraser)
    {
        _SetupEraser();
        return;
    }
    QColor color = Color(pk);
    constexpr int SIZE = 32;    //px
    int n = (int)pk;
    QPixmap pm(SIZE, SIZE);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);

    painter.setPen(QPen(color, 2, Qt::SolidLine));
    painter.drawLine(QPoint(0, SIZE / 2), QPoint(SIZE - 1, SIZE / 2));
    painter.drawLine(QPoint(SIZE / 2, 0), QPoint(SIZE / 2, SIZE - 1));
    _pointers[n] = QCursor(pm);
}

void FalconPens::_PreparePointers()
{
    for(int i = penRed; i < penLastNotUseThis; ++i)
        _PreparePointerFor((FalconPenKind)i);
}

//----------------------------- FBSettings -------------------
/*=============================================================
 * TASK:    centralized settings handler
 *      static members are initialized in FalconBoard.cpp!
 *------------------------------------------------------------*/
void FBSettings::Init()             // set home path for falconBoard
{
    homePath = QDir::homePath() +
#if defined (Q_OS_Linux)   || defined (Q_OS_Darwin) || defined(__linux__)
        "/.falconBoard";
#elif defined(Q_OS_WIN)
        "/Appdata/Local/FalconBoard";
#endif
}

QString FBSettings::Name()
{
    return homePath +
#ifdef _VIEWER
        "/FalconBoardViewer.ini";
#else
        "/FalconBoard.ini";
#endif        
}

QSettings* FBSettings::Open()
{
    return _ps = new QSettings(Name(), QSettings::IniFormat);
}
