#pragma once
#ifndef _PRINTING_H
#define _PRINTING_H

#include <QString>
#include <QRect>
#include <QSize>
#include <QVector>
#include <QRect>
#include <QPrintDialog>
#include <QPainter>
#include <QColor>
#include <QImage>

#include "common.h"

class History;      // in history.h

struct MyPrinterData
{
    QPoint topLeftActPage;          // set in DrawAre before printing

    int screenPageWidth = 1920;         // used to calculate pixel limits for pages
    int screenPageHeight = 1920 * 3500 / 2480;
    int dpi = 300;                  // printer resolution: dots / inch approx 118 dots/cm
    QString printerName;            // last selected printer, usually the default one not saved between sessions
    float magn = 1.29166663;        // magnification factor for print: 1 print pixel = _magn screen pixel (for A4 and HD)
    QRectF printArea = QRectF(0, 0, 2480.0, 3500.0);    // get from printer:printable area in device pixels (dots): top left is usually not 0,0
    int flags;

    // colors and others: set these before print in drawarea
    QColor  backgroundColor = "#FFFFFF",
            gridColor       = "#FFFFFF";
    QImage* pBackgroundImage;
    int nGridSpacingX       = 64,
        nGridSpacingY       = 64;
    bool gridIsFixed        = true;
};


class PrintProgressDialog;

class MyPrinter
{
public:

    MyPrinter(History* pHist, MyPrinterData prdata);

    static QPrinter *GetPrinterParameters(MyPrinterData &prdata);    // set screenwidth and printer name into prdata first from printer name, false: no such printer
    bool Print();

public:
    //----------------------------------------------
    struct Yindex
    {
        int yix;
        int zorder;     // then by z-order [screenshots:0, other:1]
        bool operator==(const Yindex& other) { return yix == other.yix && zorder == other.zorder; }
        bool operator!=(const Yindex& other) { return !operator==(other); }
        bool operator<(const Yindex& other) { return yix < other.yix ? true: zorder < other.zorder ? true : false; }
    };

    using YIndexVector = QVector<Yindex>;
private:
    struct Page
    {
        int pageNumber;
        QRect screenArea;        // absolute screen area for page set from _data.topLeftActPage, screenWidth and _data.printArea
        YIndexVector yindices;   // of drawables to print on this page (ordered by y, z-order [screenshots:0, other:1] then x)
        bool operator==(const struct Page& other) { return pageNumber == other.pageNumber; }
    };
    using PageVector = QVector<Page>;
    PageVector _pages;

    // temporary
    Page _actPage;
    QPainter *_painter,
             *_painterPage,
             *_printPainter;
    QImage* _pPageImage = nullptr,
          * _pItemImage = nullptr;
    PrintProgressDialog* _pProgress = nullptr;

    History* _pHist;
    QPrinter* _printer = nullptr;
    MyPrinterData _data;

    QVector<int> _selectedPages;            // to print e.g. 3,4,5,6
    QPrintDialog* _pDlg;

private:

    bool _AllocateResources();
    bool _FreeResources();
    int _CalcPages();        // using _data 
    bool _PrintImage();
    bool _PrintItem(Yindex yi);
    void _PrintGrid();
    void _PreparePage(int which);
    bool _PrintPage(int page, bool last);  // using _history last: no new page after this
    bool _Print(int from=1, int to=0x7fffffff);   // pages
    bool _Print(QVector<int>& pages);             // list of pages
    QPrintDialog* _DoPrintDialog();   // if 'Print' pressed recalculates page data () else returns nullptr
    int _PageForPoint(const QPoint p);
};

#endif	// _PRINTING_H