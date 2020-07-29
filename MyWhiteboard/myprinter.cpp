#pragma once

#include <QPrinter>
#include <QPrinterInfo>
#include <QPainter>
#include <QPageSize>

#include <algorithm>

#include "history.h"
#include "myprinter.h"


/*========================================================
 * TASK:    get printer parameters from printer
 * PARAMS:  prdata: input for printer name and screen width
 *                  these will not bechanged
 * GLOBALS:
 * RETURNS:
 * REMARKS: - this is a static function
 *          - magnification is calculated from printable
 *              page rectangle and screen width
 *-------------------------------------------------------*/
QPrinter* MyPrinter::GetPrinterParameters(MyPrinterData& prdata) 
{
    QPrinter* printer;
    if (!prdata.printerName.isEmpty())
    {
		printer = new QPrinter(QPrinter::HighResolution);
        if (!printer->isValid())
        {
            delete printer;
            return nullptr;
        }
		printer->setPrinterName(prdata.printerName);
		printer->setOrientation(prdata.orientation ? QPrinter::Portrait : QPrinter::Landscape);

		prdata.orientation = printer->orientation() == QPrinter::Portrait ? 0 : 1;
		prdata.printerName = printer->printerName();
    // get actual printer data (may be different from the one set in page setup)
        prdata.dpi = printer->resolution();
        prdata.printArea = printer->pageRect(QPrinter::DevicePixel);

        prdata.magn = (float)prdata.printArea.width() / (float)prdata.screenPageWidth;
        prdata.screenPageHeight = (int)((float)prdata.printArea.height() / prdata.magn);
           
        return printer;
    }

    return nullptr;
}
//----------------------------------------------
//----------------------------------------------
struct PageNum2
{
    int ny, nx;     // orindal of page vertically and horizontally
    int zorder;     // z-order [screenshots:0, other:1]
    QVector<int> yindices = { -1 };
    void clear() {yindices.clear(); yindices.push_back(-1);}

    bool operator<(const PageNum2& o) { return ny < o.ny ? true : (zorder < o.zorder) ? true: (ny == o.ny && nx < o.nx) ? true : false; }
    bool operator==(const PageNum2& o) { return (nx == o.nx && ny == o.ny); }
    bool operator!=(const PageNum2& o) { return (nx != o.nx || ny != o.ny); }
};

static struct SortedPageNumbers
{
    QVector<PageNum2> pgns;
    QRect screenPageRect;

    const PageNum2& operator[](int ix) const { return pgns[ix]; }
    int Size() const { return pgns.size(); }
    void Clear()
    {
        pgns.clear();
    }
    int pgpos(PageNum2& pgn)
    {
        auto it = std::lower_bound(pgns.begin(), pgns.end(), pgn);
        return it - pgns.begin();
    }
    void Init(QRect screenRect)
    {
        Clear();
        screenPageRect = screenRect; 
    }
    void Insert(PageNum2 pgn)
    {
        int pos = pgpos(pgn);
        if (pos == pgns.size())
            pgns.push_back(pgn);
        else if (pgns[pos] != pgn)
            pgns.insert(pos, pgn);
        else            // existing point see if item already added
        {
            int ix = pgns[pos].yindices.indexOf(pgn.yindices[0]);
            if (ix < 0)
                pgns[pos].yindices.push_back(pgn.yindices[0]);
        }
    }
    PageNum2 &PageForPoint(QPoint& p, int yindex, PageNum2 &pgn)
    {
        pgn.yindices[0] = yindex;
        pgn.ny = p.y() / screenPageRect.height();
        pgn.nx = p.x() / screenPageRect.width();
        return pgn;
    };

    void AddPoints(HistoryItem* phi, int yindex, PageNum2 &pgn) // to 'pgns'
    {
        DrawnItem* pdrni;

        pdrni = phi->GetDrawable(0);    // only one element for drawable / printable
        Insert(PageForPoint(pdrni->points[0], yindex, pgn));
        Insert(pgn);
        for (int i = 1; i < pdrni->points.size(); ++i)
            Insert(PageForPoint(pdrni->points[i], yindex, pgn) );
    }

} sortedPageNumbers;

int MyPrinter::_CalcPages()
{
    _pages.clear();

    sortedPageNumbers.Init( QRect(0, 0, _data.screenPageWidth, _data.screenPageHeight) );

    int nSize = _pHist->CountOfVisible();

    // for each drawable determine pages it apperas on and prepare
    // an list of pages ordered first by y then by x page indices
    HistoryItem* phi;
    PageNum2 pgn;
    for (int yi = 0; yi < nSize; ++yi) 
    {
        phi = _pHist->atYIndex(yi);
        if (phi->type == heScreenShot)   // then this item is in all page rectangles it intersects
        {
            PageNum2 lpgn;
            sortedPageNumbers.PageForPoint(phi->Area().topLeft(), yi, pgn);         
            sortedPageNumbers.PageForPoint(phi->Area().bottomRight(), yi, lpgn);
            pgn.zorder = lpgn.zorder = yi;       // screen shots below others
            PageNum2 act = pgn;
            for (int y = pgn.ny; y <= lpgn.ny; ++y)
            {
                act.ny = y;
                for (int x = pgn.nx; x <= lpgn.nx; ++x)
                {
                    act.ny = y;
					sortedPageNumbers.Insert(act);
                }
            }
        }
        else                            // scribbles, ereases and other (future) drawables
        {                               // must be checked point by point
            switch (phi->type)
            {
                case heScribble:
                case heEraser:
                    pgn.zorder = 1000000;
                    sortedPageNumbers.AddPoints(phi, yi, pgn);
                    break;
                default: break;
            }
        }
    }
    // add pages to linear page list
    _pages.clear();
    Page pg;
    pg.area = QRect(0, 0, _data.screenPageWidth, _data.screenPageHeight);

    for (int i = 0; i < sortedPageNumbers.Size(); ++i)
    {
        pg.pageNumber = i;
        pgn = sortedPageNumbers[i];
        pg.yindices = pgn.yindices;
        pg.area.moveTo(pgn.nx * _data.screenPageWidth, pgn.ny* _data.screenPageHeight);
        _pages.push_back(pg);
    }
    return _pages.size();
}

MyPrinter::MyPrinter(History* pHist, MyPrinterData prdata) :
    _pHist(pHist), _data (prdata)
{
    
}
QPrintDialog* MyPrinter::_DoPrintDialog()
{
    QPrinter* printer = GetPrinterParameters(_data);
    if (!printer)
        return nullptr;
    int pages = _CalcPages(); // page data may change if new printer is selected

    printer->setFromTo(1, pages);   // max page range

    pDlg = new QPrintDialog(printer);

    pDlg->setPrintRange(QAbstractPrintDialog::AllPages);

    if (pDlg->exec())
    {
        QPrinter* pActPrinter = pDlg->printer();
        if (printer != pActPrinter)
        {
            delete printer;
            printer = pActPrinter;
        }

        return pDlg;    // must not delete here so printer remains valid
        // print
    }
    delete pDlg;

    return nullptr;
}

int MyPrinter::_PageForPoint(QPoint p)
{
    for (int i = 0; i < _pages.size(); ++i)
        if (_pages[i].area.contains(p))
            return i;
    return -1;
}

bool MyPrinter::_PrintPage(int page)
{
    return false;
}

bool MyPrinter::_Print(QVector<int>& pages)
{
    bool res = true;
    for(int i=0; i < pages.size() && res; ++i)
        res &= _PrintPage(i);
    return res;
}

bool MyPrinter::_Print(int from, int to)
{
    QVector<int> vec(to - from + 1);
    for (int i = 0; i <= to - from; ++i)
        vec[i] = from + i;
    return _Print(vec);
}

bool MyPrinter::Print()
{
    QPrintDialog* pdlg;
    if ((pdlg=_DoPrintDialog()))
    {
        int n = _CalcPages();       // agsin
        QPrinter *printer = pdlg->printer();
        QPrinter::PrintRange range = printer->printRange();
        if (range == QPrinter::CurrentPage)
        {
            PageNum2 pgn;
            int n = _PageForPoint(_data.topLeftActPage);
            return _Print(n);
        }
        else if (range == QPrinter::AllPages)
        {
            return _Print(0, _pages.size() - 1);
        }
        else if (range == QPrinter::PageRange)
        {
            int from = printer->fromPage()-1;   
            int to = printer->toPage()-1;

            return _Print(from, to);
        }
        else          // selection
        {
            // ???
            return true;
        }
        delete pdlg;
    }
    return false;
}

