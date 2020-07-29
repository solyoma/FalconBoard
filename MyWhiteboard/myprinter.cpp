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

        prdata.magn = (float)prdata.printArea.width() / (float)prdata.screenWidth;
           
        return printer;
    }

    return nullptr;
}
//----------------------------------------------
struct PageNum2
{
    int ny, nx;
    QVector<int> yindices = { -1 };
    void clear() {yindices.clear(); yindices.push_back(-1);}

    bool operator<(const PageNum2& o) { return ny < o.ny ? true : (ny == o.ny && nx < o.nx) ? true : false; }
    bool operator==(const PageNum2& o) { return (nx == o.nx && ny == o.ny); }
    bool operator!=(const PageNum2& o) { return (nx != o.nx || ny != o.ny); }
};

static struct SortedPageNumbers
{
    QVector<PageNum2> pgns;
    QRect screenPageRect;

    void Clear()
    {
        pgns.clear();
    }

    void Init(QRect screenRect)
    {
        Clear();
        screenPageRect = screenRect; 
    }
    void Insert(PageNum2 pgn)
    {
        int pos = pgpos(pgn, pgns);
        if (pos == pgns.size())
            pgns.push_back(pgn);
        else if (pgns[pos] != pgn)
            pgns.insert(pos, pgn);
        else // existinf
            pgns[pos].yindices.push_back(pgn.yindices[0]);
    }
    void PageForPoint(QPoint& p, int yindex, PageNum2 &pgn)
    {
        pgn.yindices[0] = yindex;
        pgn.ny = (p.y() + screenPageRect.width() - 1) / screenPageRect.height();
        pgn.nx = (p.x() + screenPageRect.width() - 1) / screenPageRect.width();
    };

    void AddPoints(HistoryItem* phi, int yindex, PageNum2 &pgn) // to 'pgns'
    {
        DrawnItem* pdrni;

        pdrni = phi->GetDrawable

    }
} sortedPageNumbers;

int pgpos(PageNum2& pgn, QVector<PageNum2> &pgns)
{
    auto it = std::lower_bound(pgns.begin(), pgns.end(), pgn);
    return it - pgns.begin();
}

int MyPrinter::_CalcPages()
{
    _pages.clear();

    int screenPageHeight = (int)(_data.printArea.height() / _data.magn);
    int w = _data.screenWidth;      
    sortedPageNumbers.Init( QRect(0, 0, w, screenPageHeight) );

    int nSize = _pHist->CountOfVisible();

    HistoryItem* phi;

    for (int yi = 0; yi < nSize; ++yi) // for each drawable
    {
        phi = _pHist->atYIndex(yi);
        PageNum2 pgn;
        if (phi->type == heScreenShot)   // then this item is in all page rectangles it intersects
        {
            PageNum2 lpgn;
            sortedPageNumbers.PageForPoint(phi->Area().topLeft(), yi, pgn);         
            sortedPageNumbers.PageForPoint(phi->Area().bottomRight(), yi, lpgn);
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
            pdrni = phi->GetDrawable(0);
            if (pdrni)                   // should be always!
            {
                switch (phi->type)
                {
                    case heScribble:
                    case heEraser:
                        AddPoints
                }
            }
        }
    }
    
    auto insert = [&](int yindex)
    { 
        int ny = (phi->Area().y() + screenPageHeight-1)/screenPageHeight, 
            nx = (phi->Area().x() + w - 1)/w; 
        pg.pageNumber = ny;
        int n = pages.indexOf(pg);
        if (n < 0)
        {
            pages.push_back(pg);
            n = pages.size() - 1;
        }
        pages[n].yindices.push_back(yindex);

        if (nx) // page at right of some other
        {
            pg.pageNumber = nx;
            int n1 = pages[n].pagesAtRight.indexOf(pg);
            if (n1 < 0)
            {
                pages[n].pagesAtRight.push_back(pg);
                n1 = pages[n].pagesAtRight.size() - 1;
            }
            pages[n].pagesAtRight[n].yindices.push_back(n1);
        }
    };

    for (int i = 0; i < nSize; ++i)
        insert(i);

    return 0;
}

bool MyPrinter::_CollectPage(int page)
{
    return false;
}

bool MyPrinter::_PrintPage(QRect& rect)
{
    return false;
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
    int pages = _CalcPages(); // although page data may change...

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
        int from = printer->fromPage();
        int to = printer->toPage();


        return pDlg;    // must not delete here so printer remains valid
        // print
    }
    delete pDlg;

    return nullptr;
}

bool MyPrinter::_Print(int from, int to)
{
    QVector<int> vec(to - from + 1);
    for (int i = 0; i <= to - from; ++i)
        vec[i] = from + i;
    return _Print(vec);
}

bool MyPrinter::_Print(QVector<int>& pages)
{
    return false;
}

bool MyPrinter::Print()
{
    if (_DoPrintDialog())
    {
        _CalcPages();

    }
}
