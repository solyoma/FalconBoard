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

/* How: Create a full sized QImage (1:1) using the screen size in pixels
 *      to print everything on it, then use QPainter's drawImage(target rect, image, source rect)
 *      to copy it to the actual device (printer or PDF)
 *  The target rectangle defines the margins and Qt shrinks ^expands the image
 */
class History;      // in history.h

class MyPrinterData
{
    qreal _hMargin = 1,              // in inches because QPageLayout does not have dots
          _vMargin = 1,              // left-right and top-bottom margins
          _gutterMargin = 0;         // added to left margin on odd, right margin on even pages
        // the areas below are the ones we print to. During printing all margins are 0
        // and reflect page orientation
    QRectF _printAreaInInches;           // full print (not screen) page in inches
    QRect  _printAreaInDots;             // full print (not screen) page in dots
    int _dpi = 300;


    void _ConvertAreaToDots()
    {
        _printAreaInDots = QRectF( _printAreaInInches.x() * _dpi, _printAreaInInches.y()* _dpi, _printAreaInInches.width()* _dpi, _printAreaInInches.height()* _dpi).toRect();
    }

    void _CalcScreenPageHeight(bool calcScreenHeight)   // w. margins !
    {
        if (!calcScreenHeight)
            return;

        QRectF tr = TargetRectInDots(false);
        if (tr.isValid())
        {
            qreal magn = tr.width() / qreal(screenPageWidth);
            screenPageHeight = tr.height() / magn;
        }
    }
public:
    enum Unit {mpdInch, mpdDots};

    QPoint topLeftActPage;          // set in DrawAre before printing
    QString directory;               // empty or ends with '/'
    QString* pdir = nullptr;         // points to either 'directory' or for PDF to caller

    int paperId = 3;                 // default: A4
    int screenPageWidth = 1920;      // width of page on screen in pixels - used to calculate pixel limits for pages (HD: 1920 x 1080)
    int screenPageHeight = 1920 * 3508 / 2480;  // height of screen for selected paper size in pixels - for A4 (210mm x 297mm, 8.27 in x 11.7 in) with 300 dpi

    QString printerName;            // last selected printer, usually the default one not saved between sessions
    QString docName;
    int flags;                      // ORed from PrinterFlags (common.h)
    bool bExportPdf = false;

    // colors and others: set these before print in drawarea
    QColor  backgroundColor = "#FFFFFF",
            gridColor       = "#D0D0D0";
    QImage* pBackgroundImage;
    int nGridSpacingX       = 64,    // in dots/pixels
        nGridSpacingY       = 64;
    bool gridIsFixed        = true;

    bool openPDFInViewerAfterPrint = false;

        // for PDF printing only
    QString fileName;

    QRectF PrintAreaInInches() const
    {
        return _printAreaInInches;
    }
    QRect PrintAreaInDots() const
    {
        return _printAreaInDots;
    }
    QRectF TargetRectInInches(bool gutterAtLeft) const     // with margins
    {
        qreal mx = _hMargin, my= _vMargin;
        return QRectF(mx + (gutterAtLeft ? _gutterMargin : 0), my, _printAreaInInches.width()- 2*mx - _gutterMargin, _printAreaInInches.height() - 2*my);
    }

    QRectF TargetRectInDots(bool gutterAtLeft) const
    {
        qreal mx = _hMargin * _dpi, 
              my = _vMargin * _dpi;
		return QRectF(mx + (gutterAtLeft ? _gutterMargin * _dpi : 0), my, _printAreaInDots.width() - 2*mx - _gutterMargin * _dpi, _printAreaInDots.height() - 2*my);
    }

    int Dpi() const
    {
        return _dpi;
    }

    void SetDpi(int odpi, bool calcScreenHeight);
    void SetPrintArea(const QRectF area, bool calcScreenHeight);    // area does not include the margins
    void SetPrintArea(int paperSizeIndex, bool calcScreenHeight);   // area does not include the margins
    void SetMargins(qreal horiz, qreal vert, qreal gutter, bool calcScreenHeight);

    int HMargin(Unit u=mpdInch) const { return u==mpdDots ? _hMargin * _dpi : _hMargin; }
    int VMargin(Unit u=mpdInch) const { return u==mpdDots ? _vMargin * _dpi : _vMargin;}
    QPoint Margins(bool gutterToo, Unit u = mpdInch) const 
    { 
        QPoint qpConv = u == mpdDots ? QPoint(_vMargin * _dpi, _hMargin * _dpi) : QPoint(_vMargin, _hMargin);
        if (gutterToo)
            qpConv.setX(GutterM(u) + qpConv.x());
        return qpConv; 
    };
    int GutterM(Unit u=mpdInch) const { return u == mpdDots ? _gutterMargin * _dpi : _gutterMargin; }

    QRect PrintPrintAreaInDots(bool useMargins, bool useGutter)
    {
        QRect r = PrintAreaInDots();
        if (useMargins)
        {
            QPoint pm = Margins(useGutter, mpdDots);
            r = QRect(r.topLeft() + pm, QSize(r.width() - 2 * pm.x(),r.height()-2 * pm.y()));
        }
        return r;
    }
};


class PrintProgressDialog;

class MyPrinter
{
public:

    enum StatusCode{rsOk, rsNothingToPrint, rsAllocationError, rsCancelled, rsInvalidPrinter, rsPrintError };
    MyPrinter(QWidget *parent, History* pHist, MyPrinterData &prdata);  // prdata is modifyed by printer parameters

                                                                     // used in DrawArea.cpp
    bool Print();
    bool isValid();
    StatusCode Status() const { return _status; }
    QPrinter* Printer() const { return _printer; }
public:
    //----------------------------------------------
    using YIndexVector = QVector<int>;
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
    QPainter *_pScribblePainter,  // paints to _pItemImage
             *_pImagePainter,     // paints on _pPageImage
             *_pPrintPainter;
    StatusCode _status = rsInvalidPrinter;
    QImage* _pPageImage = nullptr,
          * _pItemImage = nullptr;
    PrintProgressDialog* _pProgress = nullptr;

    History* _pHist;
    QPrinter* _printer = nullptr;
    MyPrinterData _data;

    IntVector _selectedPages;            // to print e.g. 3,4,5,6
    QPrintDialog* _pDlg = nullptr;


private:

    bool _AllocateResources();
    bool _FreeResources();
    int  _CalcPages();              // using _data 
    bool _PrintItem(int yi);
    void _PrintGrid();
    void _PreparePage(int which);
    bool _PrintPage(int page, bool last);  // using _history last: no new page after this
    bool _Print(int from=1, int to=0x70000000);   // pages
    bool _Print(IntVector& pages);             // list of pages
    QPrintDialog* _DoPrintDialog();   // if 'Print' pressed recalculates page data () else returns nullptr
    int _PageForPoint(const QPoint p);

    StatusCode _GetPdfPrinter();    // when pdf printing
    StatusCode _GetPrinterParameters();    // set screenwidth and printer name into prdata first from printer name, false: no such printer
};

#endif	// _PRINTING_H