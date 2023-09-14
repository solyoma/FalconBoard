#pragma once
#ifndef _PRINTING_H
#define _PRINTING_H

#include <QString>
#include <QRectF>
#include <QSize>
#include <QVector>
#include <QRectF>
#include <QPrintDialog>
#include <QPainter>
#include <QColor>
#include <QImage>

#include "common.h"
#include "config.h"

/* How: Create a full sized QImage (1:1) using the screen size in pixels
 *      to print everything on it, then use QPainter's drawImage(target rect, image, source rect)
 *      to copy it to the actual device (printer or PDF)
 *  The target rectangle defines the margins and Qt shrinks / expands the image
 */
class History;      // in history.h

class MyPrinterData
{
public:
    enum Unit {mpdInch, mpdDots};

    QPointF topLeftOfCurrentPage;    // if only the current page is selected for printing this is 
                                     // the top left of that page. Set in DrawArea before printing

    QString directory;               // empty or ends with '/'
    QString* pdir = nullptr;         // points to either 'directory' or for PDF to caller

    QString docName;
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
    QString pdfFileName;

    QRectF PrintAreaInInches() const
    {
        return _printAreaInInches;
    }
    QRectF PrintAreaInDots() const
    {
        return _printAreaInDots;
    }
    QRectF PrintAreaInDotsWhenMargins(bool useMargins, bool useGutter)
    {
        QRectF r = PrintAreaInDots();
        if (useMargins)
        {
            QPointF pm = Margins(useGutter, mpdDots);
            r = QRectF(r.topLeft() + pm, QSize(r.width() - 2 * pm.x(),r.height()-2 * pm.y()));
        }
        return r;
    }

    int  ConvertToDots(qreal inches)   // using _dpi
    {
        return inches * _dpi;
    }

    QRectF TargetRectInInches(bool gutterAtLeft) const     // with margins
    {
        qreal mx = PageParams::hMargin, my = PageParams::vMargin;
        return QRectF(mx + (gutterAtLeft ? PageParams::gutterMargin : 0), my, _printAreaInInches.width()- 2*mx - PageParams::gutterMargin, _printAreaInInches.height() - 2*my);
    }

    QRectF TargetRectInDots(bool gutterAtLeft) const
    {
        qreal mx = PageParams::hMargin * _dpi, 
              my = PageParams::vMargin * _dpi,
              gx = gutterAtLeft ? PageParams::gutterMargin * _dpi : 0;
		return QRectF(mx + gx, my, _printAreaInDots.width() - 2*mx - gx, _printAreaInDots.height() - 2*my);
    }

    int Dpi() const
    {
        return _dpi;
    }

    void SetDpi(int odpi, bool calcScreenHeight);
    void SetPrintArea(const QRectF area, bool calcScreenHeight);    // area does not include the margins
    void SetPrintArea(int paperSizeIndex, bool calcScreenHeight);   // area does not include the margins

    int HMargin(Unit u=mpdInch) const { return u==mpdDots ? PageParams::hMargin * _dpi : PageParams::hMargin; }
    int VMargin(Unit u=mpdInch) const { return u==mpdDots ? PageParams::vMargin * _dpi : PageParams::vMargin;}
    QPointF Margins(bool gutterToo, Unit u = mpdInch) const 
    { 
        QPointF qpConv = u == mpdDots ? QPointF(PageParams::vMargin * _dpi, PageParams::hMargin * _dpi) : QPointF(PageParams::vMargin, PageParams::hMargin);
        if (gutterToo)
            qpConv.setX(GutterMargin(u) + qpConv.x());
        return qpConv; 
    };
    int GutterMargin(Unit u=mpdInch) const { return u == mpdDots ? PageParams::gutterMargin * _dpi : PageParams::gutterMargin; }

    void CalcScreenPageHeight(bool calcScreenHeight)   // w. margins !
    {
        if (!calcScreenHeight)
            return;

        QRectF tr = TargetRectInDots(false);        // inside margins
        if (tr.isValid())
        {
            if (PageParams::flags & landscapeModeFlag)
                PageParams::screenPageHeight = tr.height() / tr.width() * PageParams::screenPageWidth;
            else
                PageParams::screenPageHeight = tr.width() / tr.height() * PageParams::screenPageWidth;
        }
    }
private:
    QRectF _printAreaInInches;       // full print (not screen) page in inches
    QRectF  _printAreaInDots;        // full print (not screen) page in dots
    int _dpi = 300;


    void _ConvertAreaToDots()
    {
        _printAreaInDots = QRectF( _printAreaInInches.x() * _dpi, _printAreaInInches.y()* _dpi, _printAreaInInches.width()* _dpi, _printAreaInInches.height()* _dpi).toRect();
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
        QRectF screenArea;        // absolute screen area for page set from _data.topLeftActPage, screenWidth and _data.printArea
        YIndexVector yindices;   // of scribbles to print on this page (ordered by y, z-order [screenshots:0, other:1] then x)
        bool operator==(const struct Page& other) { return pageNumber == other.pageNumber; }
    };
    using PageVector = QVector<Page>;
    PageVector _pages;

    // temporary
    Page _actPage;
    QPainter *_pDrawablePainter,  // paints to _pItemImage
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
    bool _PrintItem(int yi, const QRectF& clipR);
    void _PrintGrid();
    void _PreparePage(int page);
    bool _PrintPage(int page, bool last);  // using _history last: no new page after this
    bool _Print(int from=1, int to=0x70000000);   // pages
    bool _Print(IntVector& pages);             // list of pages
    QPrintDialog* _DoPrintDialog();   // if 'Print' pressed recalculates page data () else returns nullptr
    int _PageForPoint(const QPointF p);

    StatusCode _GetPdfPrinter();    // when pdf printing
    StatusCode _GetPrinterParameters();    // set screenwidth and printer name into prdata first from printer name, false: no such printer
};

#endif	// _PRINTING_H