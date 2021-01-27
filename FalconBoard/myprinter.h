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
    QString directory;               // empty or ends with '/'
    QString* pdir = nullptr;         // points to either 'directory' or for PDF to caller

    int screenPageWidth = 1920;         // used to calculate pixel limits for pages
    int screenPageHeight = 1920 * 3500 / 2480;
    int dpi = 300;                  // printer resolution: dots / inch approx 118 dots/cm
    float magn = 1.29166663;        // magnification factor for print: 1 print pixel = _magn screen pixel (for A4 and HD)
    QString printerName;            // last selected printer, usually the default one not saved between sessions
    QString docName;
    QRect printArea = QRect(0, 0, 2480, 3500);    // get from printer:printable area in device pixels (dots): top left is usually not 0,0
    int flags;
    bool bExportPdf = false;

    // colors and others: set these before print in drawarea
    QColor  backgroundColor = "#FFFFFF",
            gridColor       = "#FFFFFF";
    QImage* pBackgroundImage;
    int nGridSpacingX       = 64,
        nGridSpacingY       = 64;
    bool gridIsFixed        = true;

        // for PDF printing only
    QString fileName;
    int pdfMarginLR, pdfMarginTB; // left-right and top bottom in pixels
};


class PrintProgressDialog;

class MyPrinter
{
public:

    enum StatusCode{rsOk, rsNothingToPrint, rsAllocationError, rsCancelled, rsInvalidPrinter, rsPrintError };
    MyPrinter(QWidget *parent, History* pHist, MyPrinterData &prdata);  // prdata is modifyed by printer parameters

                                                                     // used in DrawArea.cpp
    bool Print();
    bool isValid() const;
    StatusCode Status() const { return _status; }
    QPrinter* Printer() const { return _printer; }
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
    QWidget* _parent;
    struct Page
    {
        int pageNumber;
        QRect screenArea;        // absolute screen area for page set from _data.topLeftActPage, screenWidth and _data.printArea
        YIndexVector yindices;   // of scribbles to print on this page (ordered by y, z-order [screenshots:0, other:1] then x)
        bool operator==(const struct Page& other) { return pageNumber == other.pageNumber; }
    };
    using PageVector = QVector<Page>;
    PageVector _pages;

    // temporary
    Page _actPage;
    QPainter *_painter,
             *_painterPage,
             *_printPainter;
    StatusCode _status = rsInvalidPrinter;
    QImage* _pPageImage = nullptr,
          * _pItemImage = nullptr;
    PrintProgressDialog* _pProgress = nullptr;

    History* _pHist;
    QPrinter* _printer = nullptr;
    MyPrinterData _data;

    QVector<int> _selectedPages;            // to print e.g. 3,4,5,6
    QPrintDialog* _pDlg = nullptr;


private:

    bool _AllocateResources();
    bool _FreeResources();
    int _CalcPages();        // using _data 
    bool _PrintItem(Yindex yi);
    void _PrintGrid();
    void _PreparePage(int which);
    bool _PrintPage(int page, bool last);  // using _history last: no new page after this
    bool _Print(int from=1, int to=0x70000000);   // pages
    bool _Print(QVector<int>& pages);             // list of pages
    QPrintDialog* _DoPrintDialog();   // if 'Print' pressed recalculates page data () else returns nullptr
    int _PageForPoint(const QPoint p);

    StatusCode _GetPdfPrinter();    // when pdf printing
    StatusCode _GetPrinterParameters();    // set screenwidth and printer name into prdata first from printer name, false: no such printer
};

#endif	// _PRINTING_H