#include <QApplication>
#include <QMainWindow>
#include <QFileDialog>
#include <QPrinter>
#include <QPrinterInfo>
#include <QPainter>
#include <QPageSize>
#include <QImage>
// DEBUG
#include <QMargins>
// /DEBUG

//#include <vector>
#include <algorithm>

#include "history.h"
#include "printprogress.h"
#include "myprinter.h"


MyPrinter::MyPrinter(QWidget* parent, History* pHist, MyPrinterData &prdata) :
    _parent(parent), _pHist(pHist), _data (prdata)
{
    if (_data.bExportPdf)
        _GetPdfPrinter();
    else
        _GetPrinterParameters();

    prdata = _data;        // send back to caller
}

bool MyPrinter::isValid() const 
{ 
    return _status == rsOk; 
}


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
MyPrinter::StatusCode MyPrinter::_GetPrinterParameters()
{
    _status = rsInvalidPrinter;
    if (!_data.printerName.isEmpty())
    {
        if(!_printer)
		    _printer = new QPrinter(QPrinter::HighResolution);
        if (!_printer->isValid())
        {
            delete _printer;
            _printer = nullptr;
            return _status;
        }
		_printer->setPrinterName(_data.printerName);
		_printer->setOrientation((_data.flags & pfLandscape) ? QPrinter::Landscape : QPrinter::Portrait);

        _pDlg = _DoPrintDialog();  // includes a _CalcPages() call

        if(!_pDlg)
        {
            delete _printer;
            _printer = nullptr;
            _status = rsCancelled;
            return _status;
        }
        // print dialog might have changed printer parameters
        // get actual _printer data (may be different from the one set in page setup)
		_data.printerName = _printer->printerName();
        _data.flags &= pfLandscape;
        if (_printer->orientation() == QPrinter::Landscape)
            _data.flags |= pfLandscape;
        _data.dpi = _printer->resolution();
        _data.printArea = _printer->pageRect();
        _data.printArea.moveTopLeft(QPoint(0, 0));    // margins are set by the printer

        _data.magn = (float)_data.printArea.width() / (float)_data.screenPageWidth;
        _data.screenPageHeight = (int)((float)_data.printArea.height() / _data.magn);

        _status = rsOk;
    }
    return _status;
}

MyPrinter::StatusCode MyPrinter::_GetPdfPrinter()
{
    _printer = new QPrinter(QPrinter::HighResolution);

    _status = rsInvalidPrinter;
    if (_printer)
    {
        if (!_printer->isValid())
        {
            delete _printer;
            _printer = nullptr;
            return _status;
        }
        QString docName = _data.directory + _data.docName.left(_data.docName.length() - 3) + "pdf";
        QString filename = QFileDialog::getSaveFileName(_parent, QMainWindow::tr("FalconBoard - Save PDF As"), docName, "Pdf File(*.pdf)");
        if (filename.isEmpty())
        {
            delete _printer;
            _printer = nullptr;
            _status = rsCancelled;
            return _status;
        }

        if (QFileInfo(filename).suffix().isEmpty())
            filename.append(".pdf");

        _data.fileName = filename;
        int posslash = filename.lastIndexOf('/');
        if (posslash >= 0)
            _data.directory = filename.left(posslash + 1);
        else
            _data.directory.clear();
        if (_data.pdir != &_data.directory)     // only for PDF export
            *_data.pdir = _data.directory;      // save PDF directory

        _data.printerName = "MyWhiteBoardPDF";
        _printer->setOutputFormat(QPrinter::PdfFormat);
        _printer->setPrinterName(_data.printerName);
        _printer->setOutputFileName(_data.fileName);
        _printer->setOrientation((_data.flags & pfLandscape) ? QPrinter::Landscape : QPrinter::Portrait);
        _printer->setResolution(_data.dpi);
        _printer->setPageMargins(QMarginsF(0,0,0,0));
        _data.screenPageHeight = (int)((float)_data.printArea.height() / _data.magn);

        _CalcPages();
        _status = rsOk;
    }
    return _status;
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
        if (pos == pgns.size())     // all elements were smaller
            pgns.push_back(pgn);
        else if (pgns[pos] != pgn)  // pos-th element is larger
            pgns.insert(pos, pgn);  // before it
        else            // existing point see if item already added
        {
            MyPrinter::YIndexVector &yindices = pgns[pos].yindices;
            int ix = std::lower_bound(yindices.begin(), yindices.end(), pgn.yindices[0]) - yindices.begin();
            if (ix == yindices.size())
                yindices.push_back(pgn.yindices[0]);
            else if(yindices[ix] != pgn.yindices[0])
                yindices.insert(ix, pgn.yindices[0]);
        }
    }
    PageNum2 &PageForPoint(const QPoint& p, int Yindex, PageNum2 &pgn)
    {
        pgn.yindices[0].yix = Yindex;

        pgn.ny = p.y() / screenPageRect.height();
        pgn.nx = p.x() / screenPageRect.width();
        return pgn;
    };

    void AddPoints(HistoryItem* phi, int Yindex, PageNum2 &pgn) // to 'pgns'
    {
        ScribbleItem* pscrbl;

        pscrbl = phi->GetVisibleScribble(0);    // only one element for scribble / printable
        Insert(PageForPoint(pscrbl->points[0], Yindex, pgn));
        // TODO when the line segment between 2 consecutive points goes through more than one page
        //          example: horizontal, vertical line, circle, etc *********

        for (int i = 1; i < pscrbl->points.size(); ++i)
            Insert(PageForPoint(pscrbl->points[i], Yindex, pgn) );
    }

} sortedPageNumbers;

bool MyPrinter::_AllocateResources()
{
    QImage::Format fmt = _data.flags & pfGrayscale ? QImage::Format_Grayscale8 : QImage::Format_ARGB32;

    _status = rsAllocationError;

    _pPageImage = new QImage((int)_data.printArea.width(), (int)_data.printArea.height(), fmt);
    _pItemImage = new QImage((int)_data.printArea.width(), (int)_data.printArea.height(), fmt);
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

    _printPainter = new QPainter;                   
                                                    // using begin() instead of constructor makes possible
                                                    // to check status
    _printer->setDocName(_data.docName);

    _status = rsOk;
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
    DELETEPTR(_pProgress);
#undef DELETEPTR
    
    return _printPainter->end();
}

int MyPrinter::_CalcPages()
{
    sortedPageNumbers.Init( QRect(0, 0, _data.screenPageWidth, _data.screenPageHeight) );

    int nSize = _pHist->CountOfScribble();

    // for each scribble determine pages it apperas on and prepare
    // a list of pages ordered first by y then by x page indices
    HistoryItem* phi;
    PageNum2 pgn;
    for (int yi = 0; yi < nSize; ++yi) 
    {
        phi = _pHist->atYIndex(yi);
        if (phi->Hidden())
            continue;

        if (phi->IsImage())   // then this item is in all page rectangles it intersects
        {
            PageNum2 lpgn;
            sortedPageNumbers.PageForPoint(phi->Area().topLeft(), yi, pgn);         
            sortedPageNumbers.PageForPoint(phi->Area().bottomRight(), yi, lpgn);
            pgn.yindices[0].zorder = lpgn.yindices[0].zorder = phi->ZOrder();       // screen shots below others
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
        else                            // scribbles, ereases and other (future) scribbles
        {                               // must be checked point by point
            switch (phi->type)
            {
                case heEraser:
                case heScribble:
                    pgn.yindices[0].zorder = phi->ZOrder();
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
    int siz = _pages.size();
    if (!siz)
        _status = rsNothingToPrint;
    else
        _status = rsOk;
    return siz;
}
QPrintDialog* MyPrinter::_DoPrintDialog()
{
    if (!_printer)
        return nullptr;

    int pages = _CalcPages(); // page data may change if new printer is selected

    _printer->setFromTo(1, pages);   // max page range

    _pDlg = new QPrintDialog(_printer);

    _pDlg->setPrintRange(QAbstractPrintDialog::Selection);
    _pDlg->setPrintRange(QAbstractPrintDialog::CurrentPage);

    if (_pDlg->exec())
    {
        QPrinter* p_actPrinterName = _pDlg->printer();
        if (_printer != p_actPrinterName)
        {
            delete _printer;
            _printer = p_actPrinterName;
            _data.printerName = _printer->printerName();
        }
        _status = rsOk;
        return _pDlg;    // must not delete here so printer remains valid even if it was changed in the printer dialog
        // print
    }
    delete _pDlg;
    _pDlg = nullptr;
    _status = rsCancelled;

    return nullptr;
}

int MyPrinter::_PageForPoint(const QPoint p)
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
    if(phi->IsImage())
    {                               // paint over background layer
        ScreenShotImage* psi = phi->GetScreenShotImage();
        QRect srcRect = phi->Area().intersected(_actPage.screenArea); // screen coordinates
        QPoint dp =  srcRect.topLeft() - _actPage.screenArea.topLeft(); // screen relative coord.
        srcRect.moveTopLeft(dp);  

        QRect dstRect = QRect( QPoint(srcRect.x()*_data.magn, srcRect.y() * _data.magn), QSize(srcRect.width() * _data.magn, srcRect.height() * _data.magn)) ;
        Qt::ImageConversionFlag flag = _data.flags & pfGrayscale ? Qt::MonoOnly : Qt::AutoColor; // ?? destination may be monochrome already
		if (_data.flags & pfDontPrintImages)    // print placeholder
        {
            _painterPage->setPen(QPen(drawColors[penYellow], 2, Qt::SolidLine));
            _painterPage->setPen(QPen(drawColors[penYellow], 2, Qt::SolidLine));
            _painterPage->drawLine(dstRect.topLeft(), dstRect.bottomRight());
            _painterPage->drawLine(dstRect.topRight(), dstRect.bottomLeft());
        }
        else
            _painterPage->drawPixmap(dstRect, psi->image, QRect(0, 0, psi->image.width(),psi->image.height()));
    }
    else if (phi->type == heScribble || phi->type == heEraser)
    {             // paint over transparent layer
        ScribbleItem* pscrbl = phi->GetVisibleScribble(0);
        FalconPenKind pk = pscrbl->penKind;
        int pw = pscrbl->penWidth * _data.magn;
        bool erasemode = pscrbl->type == heEraser ? true : false;

        QPoint actP = pscrbl->points[0] - _actPage.screenArea.topLeft(),
                nextP = actP + QPoint(1.0, 1.0);

        _painter->setPen(QPen(drawColors[pk], pw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (erasemode)
            _painter->setCompositionMode(QPainter::CompositionMode_Clear);
        else
            _painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
        _painter->setRenderHint(QPainter::Antialiasing);

        if(pscrbl->points.size() == 1)
            _painter->drawLine(actP * _data.magn, actP * _data.magn);
        else
            for (int i = 1; i < pscrbl->points.size(); ++i)
            {
                nextP = pscrbl->points[i] - _actPage.screenArea.topLeft();
                _painter->drawLine(actP * _data.magn, nextP * _data.magn);
                actP = nextP;
            }
    }

    return false;
}

void MyPrinter::_PrintGrid()
{
    qreal x, y,
        dx = _data.nGridSpacingX * _data.magn,
        dy = _data.nGridSpacingY * _data.magn;

    if (_data.gridIsFixed)
        x =  dx, y = dy;
    else
    {
        x = dx - ((int)_actPage.screenArea.x() % _data.nGridSpacingX) * _data.magn;
        y = dy - ((int)_actPage.screenArea.y() % _data.nGridSpacingY) * _data.magn;
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
    // bottom layer
    if (!b || (_data.flags & pfWhiteBackground) != 0)
    {
        drawColors.SetDarkMode(false);
        _pPageImage->fill(Qt::white);
    }
    else
        _pPageImage->fill(_data.backgroundColor);
    // background image layer
    if ((_data.flags & pfPrintBackgroundImage) != 0 && _data.pBackgroundImage) 
        _painter->drawImage(0, 0, *_data.pBackgroundImage);   // background image image always start at top left of page
    // grid layer
    if ((_data.flags & pfGrid) != 0)  // gris is below scribbles
        _PrintGrid();
    // items layer
    for (auto ix : _actPage.yindices)
    {
        _PrintItem(ix);
        if (_pProgress)
        {
            _pProgress->Progress(_actPage.pageNumber);
            QApplication::processEvents();
        }
    }
    // end composition
    _painterPage->drawImage(QPoint(0,0), *_pItemImage);

    drawColors.SetDarkMode(b);

}

// do not call this directly. call _Print(from = which, to=which) instead 
bool MyPrinter::_PrintPage(int which, bool last)
{
    _PreparePage(which);    // into _pPageImage
    _printPainter->drawImage(QPoint(_data.printArea.left(), _data.printArea.top()), *_pPageImage); // printable area may be less than paper size
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

bool MyPrinter::_Print(int from, int to)     // from, to: 1,...
{
    QVector<int> vec(to - from + 1);
    for (int i = 0; i <= to - from; ++i)
        vec[i] = from + i;
    return _Print(vec);
}

bool MyPrinter::Print()
{
    if (isValid() )
    {
        int n = _pages.size();
        if(!_data.bExportPdf)
            n = _CalcPages();       // again: printer might have been changed in dialog

    // now print
        if (!_AllocateResources())
            return false;

        _pProgress = new PrintProgressDialog(n, _pHist->CountOfVisible(), QString("FalconBoard - ") + (_data.bExportPdf ? "PDF Export" : "Printing") );
        _pProgress->show();

        int from = 1, to = n;

        QPrinter::PrintRange range = _printer->printRange();
        if (range == QPrinter::CurrentPage)
        {
            PageNum2 pgn;
            int nCurrent = _PageForPoint(_data.topLeftActPage);
            from = nCurrent; to = nCurrent;
        }
        else if (range == QPrinter::AllPages)
        {
            from = 1; to = _pages.size();
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
        return _Print(from, to);
            // cleanup
    }
    return false;
}

