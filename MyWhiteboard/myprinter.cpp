#pragma once

#include <QPrinter>
#include <QPrinterInfo>
#include <QPainter>
#include <QPageSize>
#include <Qimage>

//#include <vector>
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
		printer->setOrientation((prdata.flags & pfLandscape) ? QPrinter::Landscape : QPrinter::Portrait);

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
struct PageNum2
{
    int ny, nx;     // ordinal of page vertically and horizontally
    MyPrinter::YIndexVector yindices; 

    PageNum2() { yindices.resize(1); }

    void clear() {yindices.clear(); yindices.resize(1);}

    bool operator<(const PageNum2& o) { return ny < o.ny ? true : (ny == o.ny && nx < o.nx) ? true :  false; }
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
        int pos = pgpos(pgn);       // index of first element that is equal or larger than pgn
        if (pos == pgns.size())     // all elements waeer smaller
            pgns.push_back(pgn);
        else if (pgns[pos] != pgn)  // pos-th element is larger
            pgns.insert(pos, pgn);  // before it
        else            // existing point see if item already added
        {
            MyPrinter::YIndexVector &yindices = pgns[pos].yindices;
            MyPrinter::Yindex index = pgn.yindices[0];
            int ix = std::lower_bound(yindices.begin(), yindices.end(), pgn.yindices[0]) - yindices.begin();
            if (ix == yindices.size())
                yindices.push_back(pgn.yindices[0]);
            else if(yindices[ix] != pgn.yindices[0])
                yindices.insert(ix, pgn.yindices[0]);
        }
    }
    PageNum2 &PageForPoint(QPoint& p, int Yindex, PageNum2 &pgn)
    {
        pgn.yindices[0].yix = Yindex;

        pgn.ny = p.y() / screenPageRect.height();
        pgn.nx = p.x() / screenPageRect.width();
        return pgn;
    };

    void AddPoints(HistoryItem* phi, int Yindex, PageNum2 &pgn) // to 'pgns'
    {
        DrawnItem* pdrni;

        pdrni = phi->GetDrawable(0);    // only one element for drawable / printable
        Insert(PageForPoint(pdrni->points[0], Yindex, pgn));
        // TODO when the line segment between 2 consecutive points goes through more than one page
        //          example: horizontal, vertical line, circle, etc *********

        for (int i = 1; i < pdrni->points.size(); ++i)
            Insert(PageForPoint(pdrni->points[i], Yindex, pgn) );
    }

} sortedPageNumbers;

bool MyPrinter::_AllocateResources()
{
    _pPageImage = new QImage((int)_data.printArea.width(), (int)_data.printArea.height(), QImage::Format_ARGB32);
    _pItemImage = new QImage((int)_data.printArea.width(), (int)_data.printArea.height(), QImage::Format_ARGB32);
    if (!_pPageImage || _pPageImage->isNull())
    {
        delete _pPageImage;
        _pPageImage = nullptr;
        return false;
    }

    if (!_pItemImage || _pItemImage->isNull())
    {
        delete _pItemImage;
        _pItemImage = nullptr;
        return false;
    }

    _painterPage = new QPainter(_pPageImage);      // always print on this then to the printer
    _painter = new QPainter(_pItemImage);      // always print on this then to the printer

    _printPainter = new QPainter;                   // using begin() instead of constructor makes possible
                                                    // to check status
    return _printPainter->begin(_printer);          // must close()
}

bool MyPrinter::_FreeResources()
{
#define DELETEPTR(a) delete a; a = nullptr

    DELETEPTR(_pDlg);
    DELETEPTR(_painterPage);
    DELETEPTR(_painter);
    DELETEPTR(_pItemImage);
    DELETEPTR(_pPageImage);
    DELETEPTR(_pPageImage);
#undef DELETEPTR
    
    return _printPainter->end();
}

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
            pgn.yindices[0].zorder = lpgn.yindices[0].zorder = phi->zorder;       // screen shots below others
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
                    pgn.yindices[0].zorder = phi->zorder;
                    sortedPageNumbers.AddPoints(phi, yi, pgn);
                    break;
                default: break;
            }
        }
    }
    // add pages to linear page list
    _pages.clear();
    Page pg;
    pg.screenArea = QRect(_data.topLeftActPage.x(), _data.topLeftActPage.y(), _data.screenPageWidth, _data.screenPageHeight);

    for (int i = 0; i < sortedPageNumbers.Size(); ++i)
    {
        pg.pageNumber = i;
        pgn = sortedPageNumbers[i];
        pg.yindices = pgn.yindices;
        pg.screenArea.moveTo(pgn.nx * _data.screenPageWidth, pgn.ny* _data.screenPageHeight);
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
    _printer = GetPrinterParameters(_data);
    if (!_printer)
        return nullptr;
    int pages = _CalcPages(); // page data may change if new printer is selected

    _printer->setFromTo(1, pages);   // max page range

    _pDlg = new QPrintDialog(_printer);

    _pDlg->setPrintRange(QAbstractPrintDialog::Selection);
    _pDlg->setPrintRange(QAbstractPrintDialog::CurrentPage);

    if (_pDlg->exec())
    {
        QPrinter* pActPrinter = _pDlg->printer();
        if (_printer != pActPrinter)
        {
            delete _printer;
            _printer = pActPrinter;
            _data.printerName = _printer->printerName();
        }

        return _pDlg;    // must not delete here so printer remains valid
        // print
    }
    delete _pDlg;
    _pDlg = nullptr;

    return nullptr;
}

int MyPrinter::_PageForPoint(QPoint p)
{
    for (int i = 0; i < _pages.size(); ++i)
        if (_pages[i].screenArea.contains(p))
            return i;
    return -1;
}


/*========================================================
 * TASK:
 * PARAMS:
 * GLOBALS:
 * RETURNS:
 * REMARKS: - item may start and/or end on an other page
 *          - there may be pages where no point 
 *              of this item is stored (long lines through
 *                  page boundaries)
 *          - screenshot images may extend to more than one pages
 *-------------------------------------------------------*/
bool MyPrinter::_PrintItem(Yindex yi)
{
    HistoryItem * phi = _pHist->atYIndex(yi.yix);
    if(phi->type == heScreenShot)
    {                               // paint over background layer
        ScreenShotImage* psi = phi->GetScreenShotImage();
        QRectF srcRect = phi->Area().intersected(_actPage.screenArea); // screen coordinates
        QPointF dp =  srcRect.topLeft() - _actPage.screenArea.topLeft(); // screen relative coord.
        srcRect.moveTopLeft(dp);  

        QRectF dstRect = QRect( QPoint(srcRect.x()*_data.magn, srcRect.y() * _data.magn), QSize(srcRect.width() * _data.magn, srcRect.height() * _data.magn)) ;
        Qt::ImageConversionFlag flag = _data.flags & pfGrayscale ? Qt::MonoOnly : Qt::AutoColor; // ?? destination may be monochrome already
		if (_data.flags & pfDontPrintImages)    //print placeholder
        {
            _painterPage->setPen(QPen(drawColors[penYellow], 2, Qt::SolidLine));
            _painterPage->drawLine(dstRect.topLeft(), dstRect.bottomRight());
            _painterPage->drawLine(dstRect.topRight(), dstRect.bottomLeft());
        }
        else
            _painterPage->drawImage(dstRect, psi->image, QRect(0, 0, psi->image.width(),psi->image.height()), flag);
    }
    else if (phi->type == heScribble || phi->type == heEraser)
    {             // paint over transparent layer
        DrawnItem* pdrni = phi->GetDrawable(0);
        MyPenKind pk = pdrni->penKind;
        int pw = pdrni->penWidth * _data.magn;
        bool erasemode = pdrni->type == heEraser ? true : false;

        QPointF actP = pdrni->points[0] - _actPage.screenArea.topLeft(),
                nextP = actP + QPointF(1.0, 1.0);

        _painter->setPen(QPen(drawColors[pk], pw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (erasemode)
            _painter->setCompositionMode(QPainter::CompositionMode_Clear);
        else
            _painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
        _painter->setRenderHint(QPainter::Antialiasing);

        if(pdrni->points.size() == 1)
            _painter->drawLine(actP * _data.magn, actP * _data.magn);
        else
            for (int i = 1; i < pdrni->points.size(); ++i)
            {
                nextP = pdrni->points[i] - _actPage.screenArea.topLeft();
                _painter->drawLine(actP * _data.magn, nextP * _data.magn);
                actP = nextP;
            }
    }
    return false;
}

void MyPrinter::_PrintGrid()
{
    int x, y,
        dx = _data.nGridSpacingX * _data.magn,
        dy = _data.nGridSpacingY * _data.magn;

    if (_data.gridIsFixed)
        x =  dx, y = dy;
    else
    {
        x = dx - (_actPage.screenArea.x() % _data.nGridSpacingX) * _data.magn;
        y = dy - (_actPage.screenArea.y() % _data.nGridSpacingY) * _data.magn;
    }

    _painterPage->setPen(QPen(_data.gridColor, 2, Qt::SolidLine));
    for (; y <= _data.printArea.height(); y += dy)
        _painterPage->drawLine(0, y, _data.printArea.width(), y);
    for (; x <= _data.printArea.width(); x += dx)
        _painterPage->drawLine(x, 0, x, _data.printArea.height());
}

void MyPrinter::_PreparePage(int which)
{
    _actPage = _pages[which];

    auto compareByZorder = [&](Yindex& left, Yindex& right)
    {
        return left.zorder < right.zorder;
    };
    std::sort(_actPage.yindices.begin(), _actPage.yindices.end(), compareByZorder);     // re- sort by z-order

    _pItemImage->fill(Qt::transparent);

    bool b = drawColors.IsDarkMode();
    if (!b || (_data.flags & pfWhiteBackground) != 0)
    {
        drawColors.SetDarkMode(false);
        _pPageImage->fill(Qt::white);
    }
    else
        _pPageImage->fill(_data.backgroundColor);
    if ((_data.flags & pfPrintBackgroundImage) != 0)  // background image image always start at top left of page
        _painter->drawImage(0, 0, *_data.pBackgroundImage);
    if ((_data.flags & pfGrid) != 0)  // gris is below scribbles
        _PrintGrid();
    for (auto ix : _actPage.yindices)
        _PrintItem(ix);

    _painterPage->drawImage(QPoint(0,0), *_pItemImage);

    drawColors.SetDarkMode(b);
}

// do not call this directly. call _Print(from = which, to=which) instead 
bool MyPrinter::_PrintPage(int which, bool last)
{
    _PreparePage(which);    // into _pPageImage
    _printPainter->drawImage(_data.printArea.left(), _data.printArea.top(), *_pPageImage); // printable area may be less than paper size
    if(!last)
        _printer->newPage();
    
    return true;
}
// this is the main print function. Frees resources afterwards
bool MyPrinter::_Print(QVector<int>& pages)
{
    bool res = true;
    for(int i=0; i < pages.size() && res; ++i)
        res &= _PrintPage(i, i == pages.size()-1);
    res &= _FreeResources();
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
    if ((_pDlg=_DoPrintDialog()))
    {
        int n = _CalcPages();       // agsin

    // now print
        if (!_AllocateResources())
            return false;

        QPrinter::PrintRange range = _printer->printRange();
        if (range == QPrinter::CurrentPage)
        {
            PageNum2 pgn;
            int n = _PageForPoint(_data.topLeftActPage);
            return _Print(n, n);
        }
        else if (range == QPrinter::AllPages)
        {
            return _Print(0, _pages.size() - 1);
        }
        else if (range == QPrinter::PageRange)
        {
            int from = _printer->fromPage()-1;   
            int to =   _printer->toPage()-1;

            return _Print(from, to);
        }
        else          // selection
        {
            // ???
            _FreeResources();
            return true;
        }
            // cleanup
    }
    return false;
}

