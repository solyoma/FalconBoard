#pragma once
#ifndef _PRINTING_H
#define _PRINTING_H

#include <QString>
#include <QRect>
#include <QSize>
#include <QVector>
#include <QRect>
#include <QPrintDialog>

class History;      // in history.h

struct MyPrinterData
{
    enum Flags :char {              // c.f. PageSetupDialog::Flags
        pfPrintBackgroundImage = 1,
        pfWhiteBackground = 2		// or use display mode which may be dark
    };

    int screenWidth = 1920;         // used to calculate pixel limits for pages
    int dpi = 300;                  // printer resolution: dots / inch approx 118 dots/cm
    int orientation = 0;            // print orientation
    QString printerName;            // last selected printer, usually the default one not saved between sessions
    int flags;
    float magn = 1.29166663;        // magnification factor for print: 1 print pixel = _magn screen pixel (for A4 and HD)
    QRectF printArea = QRectF(0, 0, 2480.0, 3500.0);               // printable area in device pixels (dots)
};

class MyPrinter
{
    struct Page
    {
        int pageNumber;
        QRect area;             // set from screenWidth and _data.printArea
        QVector<int> yindices;   // of drawables to print on this page (ordered by y
        bool operator==(const struct Page& other) { pageNumber == other.pageNumber; }
    };
    using PageVector = QVector<Page>;

    History* _pHist;

    MyPrinterData _data;

    QVector<int> _selectedPages;            // to print e.g. 3,4,5,6
    PageVector _pages;
    QPrintDialog* pDlg;

    int _CalcPages();        // using _data 
    bool _CollectPage(int page);   // determines 'imdices' from  *_pHist
    bool _PrintPage(QRect& rect);  // using _history
    bool _Print(int from=1, int to=0x7fffffff);   // pages
    bool _Print(QVector<int>& pages);             // list of pages
    QPrintDialog* _DoPrintDialog();   // if 'Print' pressed recalculates page data () else returns nullptr
public:

    MyPrinter(History* pHist, MyPrinterData prdata);

    static QPrinter *GetPrinterParameters(MyPrinterData &prdata);    // set screenwidth and printer name into prdata first from printer name, false: no such printer
    bool Print();
};

#endif	// _PRINTING_H