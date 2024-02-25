#include <QApplication>
#include <QMainWindow>
#include <QMessageBox>
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

#include "config.h"
#include "history.h"
#include "printprogress.h"
#include "pagesetup.h"
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

bool MyPrinter::isValid()  
{ 
    if (!_printer || !_printer->isValid())
        _status = rsInvalidPrinter;
    else
        _status = rsOk; 
    return _status == rsOk; 
}

void MyPrinterData::SetDpi(int odpi, bool calcScreenHeight)
{
    _dpi = odpi;
    _ConvertAreaToDots();
    CalcScreenPageHeight(calcScreenHeight);
}

//void MyPrinterData::Save(QSettings *s) const
//{
//    s->setValue(PFLAGS, flags);		        // bit 0: print background image, bit 1: white background
//    s->setValue(PDFMLR, _hMargin);
//    s->setValue(PDFMTB, _vMargin); 
//    s->setValue(PDFGUT, _gutterMargin);
//    int resi = s->value(RESI, 6).toInt(); // 1920 x 1080
//    if (s->value(USERI, true).toBool())
//        PageParams::screenPageWidth = 
//  
//    PageParams::SetDpi(resos[s->value(PDFDPI, 0).toInt()], false);      // recalculates screen area for page and also sets 'screenPageHeight'
//    PageParams::SetPrintArea(s->value(PDFPGS, 3).toInt(), true);       // using default DPI value
//}
//
//void MyPrinterData::Restore(QSettings* s)
//{
//    flags = s->value(PFLAGS, 0).toInt();		        // bit 0: print background image, bit 1: white background
//    SetMargins(s->value(PDFMLR, 1.0).toFloat(), s->value(PDFMTB, 1.0).toFloat(), s->value(PDFGUT, 0.0).toFloat(), false);
//    int resi = s->value(RESI, 6).toInt(); // 1920 x 1080
//    if (s->value(USERI, true).toBool())
//        screenPageWidth = myScreenSizes[resi].w;
//    else
//        screenPageWidth = s->value(HPXS, 1920).toInt();
//
//    SetDpi(resos[s->value(PDFDPI, 0).toInt()], false);      // recalculates screen area for page and also sets 'screenPageHeight'
//    SetPrintArea(s->value(PDFPGS, 3).toInt(), true);       // using default DPI value
//
//}

void MyPrinterData::SetPrintArea(const QRectF area, bool calcScreenHeight)    // 'area' (in inches) does not include the margins
{
    // Screen width of data will be
    // printed on a region with these dimension
    // set total page sizes in pixels

    _printAreaInInches = area;
    _ConvertAreaToDots();
    CalcScreenPageHeight(calcScreenHeight);
}
void MyPrinterData::SetPrintArea(int paperIndex, bool calcScreenHeight)    // area does not include the margins
{
    QRectF rpa = QRectF(0, 0, myPageSizes[paperIndex].w, myPageSizes[paperIndex].h);
    SetPrintArea(rpa, calcScreenHeight);
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
    if (!PageParams::actPrinterName.isEmpty())
    {
        if(!_printer)
		    _printer = new QPrinter(QPrinter::HighResolution);
        if (!_printer->isValid())
        {
            delete _printer;
            _printer = nullptr;
            return _status;
        }
		_printer->setPrinterName(PageParams::actPrinterName);
        _printer->pageLayout().setUnits(QPageLayout::Inch);
		_printer->setPageOrientation((PageParams::flags & landscapeModeFlag) ? QPageLayout::Landscape : QPageLayout::Portrait);
        _printer->setPageSize(QPageSize(myPageSizes[PageParams::paperSizeId].pid));
        _printer->setPageMargins({ qreal(_data.HMargin()), qreal(_data.VMargin()), qreal(_data.HMargin()), qreal(_data.VMargin()) });

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
        PageParams::actPrinterName = _printer->printerName();

        PageParams::flags &= ~landscapeModeFlag;
        PageParams::flags |=_printer->pageLayout().orientation() == QPageLayout::Landscape ? landscapeModeFlag : 0;

        _data.SetDpi(_printer->resolution(),false);
        _data.SetPrintArea(_printer->pageRect(QPrinter::Inch), true);

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

        _data.pdfFileName = filename;
        int posslash = filename.lastIndexOf('/');
        if (posslash >= 0)
            _data.directory = filename.left(posslash + 1);
        else
            _data.directory.clear();
        if (_data.pdir != &_data.directory)     // only for PDF export
            *_data.pdir = _data.directory;      // save PDF directory

        PageParams::actPrinterName = QStringLiteral("FalconBoardPDF");

        _printer->setPrinterName(PageParams::actPrinterName);
        _printer->setOutputFileName(_data.pdfFileName);
        _printer->setOutputFormat(QPrinter::PdfFormat);
        _printer->setPageSize(QPageSize(PageParams::paperSizeId));
        _printer->setPageOrientation((PageParams::flags & landscapeModeFlag) ? QPageLayout::Landscape : QPageLayout::Portrait);
        _printer->setResolution(_data.Dpi());

#if _DEBUG
        QRectF rf = _printer->pageLayout().fullRectPixels(_data.Dpi());
        QPageSize psiz = _printer->pageLayout().pageSize();
        QPageLayout::Orientation orientation = _printer->pageLayout().orientation();
        qDebug("page size: %g x %g (%s), orientation:%s", rf.width(), rf.height(),psiz.name().toStdString().c_str(),
                    orientation== QPageLayout::Landscape ? "landscape" : "portrait");
#endif
        _CalcPages();
    }
    return _status;
}
//----------------------------------------------
struct PageNum2
{
    int ny=0, nx=0;     // ordinal of page vertically and horizontally
    IntVector yindices; 

    PageNum2() { yindices.resize(1); }

    void ClearList() {yindices.clear(); yindices.resize(1);}

    bool operator<(const PageNum2& o) { return ny < o.ny ? true : (ny == o.ny && nx < o.nx) ? true :  false; }
    bool operator==(const PageNum2& o) { return (nx == o.nx && ny == o.ny); }
    bool operator!=(const PageNum2& o) { return (nx != o.nx || ny != o.ny); }
};

/*=============================================================
 * Sorted Page Numbers - of 2D 'PageNum2' data
 *   One screen width of data will be printed on one page
 *      independent of the printer margins
 *  Because the width of a page is set from the screen width 
 *  in pixels but the width of the area to print may be wider,
 *  it is possible that the area requires more than one page 
 *  to print this class returns pages in the correct order
 *  example: screen width 1920 pixel, 'paper' width 3 screen width
 *  then the first screen width of data is printed on page 1
 *  until the page height is reached, then the second screenwidth
 *  of data is on page 2, the 3rd on page 3 and the
 *  next print height of data is started to print on page 4
 *------------------------------------------------------------*/
static struct SortedPageNumbers
{
    QVector<PageNum2> pgns;
    QRectF screenPageRect;

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
    void Init(QRectF screenRect)
    {
        Clear();
        screenPageRect = screenRect; 
    }
    void Insert(PageNum2 &pgn)
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
    //PageNum2 &PageForPoint(const QPointF& p, int Yindex, PageNum2 &pgn)
    //{
    //    pgn.yindices[0] = Yindex;

    //    pgn.ny = p.y() / screenPageRect.height();
    //    pgn.nx = p.x() / screenPageRect.width();
    //    return pgn;
    //};

    //void AddPoints(HistoryItem* phi, int Yindex, PageNum2 &pgn) // to 'pgns'
    //{
    //    DrawableItem* pDrwbl;

    //    pDrwbl = phi->GetDrawable(true);    // only one element for scribble / printable
    //    Insert(PageForPoint(pDrwbl->points[0], Yindex, pgn));
    //    // TODO when the line segment between 2 consecutive points goes through more than one page
    //    //          example: horizontal, vertical line, circle, etc *********

    //    for (int i = 1; i < pDrwbl->points.size(); ++i)
    //        Insert(PageForPoint(pDrwbl->points[i], Yindex, pgn) );
    //}

} sortedPageNumbers;


#define DELETEPTR(a) delete a; a = nullptr

bool MyPrinter::_AllocateResources()
{
    QImage::Format fmt = PageParams::flags & printGrayScaleFlag ? QImage::Format_Grayscale8 : QImage::Format_ARGB32;

    _status = rsAllocationError;

    _pPageImage = new QImage(PageParams::screenPageWidth, PageParams::screenPageHeight, fmt);
    _pItemImage = new QImage(PageParams::screenPageWidth, PageParams::screenPageHeight, fmt);
    if (!_pPageImage || _pPageImage->isNull() || !_pItemImage || _pItemImage->isNull())
    {
        DELETEPTR(_pPageImage);
        DELETEPTR(_pItemImage);
        return false;
    }

    _pImagePainter    =  new QPainter(_pPageImage);      // always print on this then to the printer
    _pDrawablePainter =  new QPainter(_pItemImage);      // always print on this then to the printer

    _pPrintPainter = new QPainter;                   
                                                    // using begin() instead of constructor makes possible
                                                    // to check status
    _printer->setDocName(_data.docName);

    _status = rsOk;
    return _pPrintPainter->begin(_printer);          // must close()
}

bool MyPrinter::_FreeResources()
{
    DELETEPTR(_pDlg);
    DELETEPTR(_pImagePainter);
    DELETEPTR(_pDrawablePainter);
    DELETEPTR(_pItemImage);
    DELETEPTR(_pPageImage);
    DELETEPTR(_pProgress);
    
    return _pPrintPainter->end();
}
#undef DELETEPTR

/*=============================================================
 * TASK:    determine the pages needed to print the data and 
 *          organize pages into print order.
 * PARAMS:
 * GLOBALS:
 * RETURNS:
 * REMARKS: - aspect ratio of printed data may be different 
 *            from that of the screen
 *          - one screenwidth of data is printed on one page
 *              this routine calculates how many pages will be
 *              to print
 *------------------------------------------------------------*/
int MyPrinter::_CalcPages()
{
    sortedPageNumbers.Init( QRectF(0, 0, PageParams::screenPageWidth, PageParams::screenPageHeight) );

 //   int nSize = _pHist->CountOfVisible();

    // for each scribble determine pages it apperas on and prepare
    // a list of pages ordered first by y then by x page indices
    PageNum2 pgn;

    QSizeF usedArea = _pHist->UsedArea();
    int maxY = usedArea.height();
    int maxX;   // on a PageParams::screenPageHeight high band
    pgn.ny = 0;     // page y index

    for (int y = 0; y < maxY; y += PageParams::screenPageHeight, ++pgn.ny)
    {
        maxX = _pHist->RightMostInBand(QRectF(0, y, usedArea.width(), PageParams::screenPageHeight));
        pgn.ClearList();

        // must "print" empty pages too
        QRectF rect(0, pgn.ny * PageParams::screenPageHeight, PageParams::screenPageWidth, PageParams::screenPageHeight);
        pgn.nx = 0;
        _pHist->GetDrawablesInside(rect, pgn.yindices);
        sortedPageNumbers.Insert(pgn);
        // then insert pages to the right if they contain any data
        for (pgn.nx = 1; pgn.nx * PageParams::screenPageWidth < maxX; ++pgn.nx)
        {
            QRectF rect(pgn.nx * PageParams::screenPageWidth, pgn.ny * PageParams::screenPageHeight, PageParams::screenPageWidth, PageParams::screenPageHeight);
            _pHist->GetDrawablesInside(rect, pgn.yindices);
            sortedPageNumbers.Insert(pgn);
        }
    }
        
    // add pages to linear page list
    _pages.clear();
    Page pg;
    pg.screenArea = QRectF(_data.topLeftOfCurrentPage.x(), _data.topLeftOfCurrentPage.y(), PageParams::screenPageWidth, PageParams::screenPageHeight);

    for (int i = 0; i < sortedPageNumbers.Size(); ++i)
    {
        pg.pageNumber = i;
        const PageNum2 &pgn2 = sortedPageNumbers[i];
        pg.yindices = pgn2.yindices;
        pg.screenArea.moveTo(pgn2.nx * PageParams::screenPageWidth, pgn2.ny* PageParams::screenPageHeight);
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

//    _pDlg->setPrintRange(QAbstractPrintDialog::Selection);
//    _pDlg->setPrintRange(QAbstractPrintDialog::CurrentPage);

    if (_pDlg->exec())
    {
        QPrinter* p_actPrinterName = _pDlg->printer();
        if (_printer != p_actPrinterName)
        {
            delete _printer;
            _printer = p_actPrinterName;
            PageParams::actPrinterName = _printer->printerName();
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

int MyPrinter::_PageForPoint(const QPointF p)
{
    for (int i = 0; i < _pages.size(); ++i)
        if (_pages[i].screenArea.contains(p))
            return i;
    return -1;
}

/*========================================================
 * TASK:   Print a drawable on '_pItemImage'
 * PARAMS:      yi: y coord and z-order
 * GLOBALS: _actPage
 *          _pDrawablePainter, _pItemImage
 * RETURNS:
 * REMARKS: - item may start and/or end on an other page
 *          - there may be pages where no point 
 *              of this item is stored (long lines through
 *                  page boundaries)
 *          - screenshot images may extend to more than one pages
 *-------------------------------------------------------*/
bool MyPrinter::_PrintItem(int yi, const QRectF& clipR)
{
//    HistoryItem * phi = _pHist->Item(yi);
    DrawableItem* pTmp = _pHist->Drawable(yi);
    if (pTmp->IsVisible())
        pTmp->Draw(_pDrawablePainter, clipR.topLeft(), clipR);

    return false;
/*
    if(phi->IsImage())
    {                               // paint over background layer
        DrawableScreenShot* psi = dynamic_cast<DrawableScreenShot*>(phi->GetDrawable(true) );
        QPointF dp = -_actPage.screenArea.topLeft();
        QRectF imgRect = phi->Area().translated(dp),   // actual screen relative coordinates for whole image
            visibleRect = imgRect.intersected(_actPage.screenArea.translated(dp)); // screen relative coordinates of part of image rectangle on this page

        dp = visibleRect.topLeft() - imgRect.topLeft();
        QRectF srcRect = QRectF(dp,  visibleRect.size());
        Qt::ImageConversionFlag flag = _data.flags & pfGrayscale ? Qt::MonoOnly : Qt::AutoColor; // ?? destination may be monochrome already
		if (_data.flags & pfDontPrintImages)    // print placeholder
        {
            _pImagePainter->setPen(QPen(drawColors[penYellow], 2, Qt::DotLine));
            _pImagePainter->drawRect(imgRect);  // part may be outside of page
        }
        else
            _pImagePainter->drawPixmap(visibleRect, psi->Image(), srcRect);
    }
    else if (phi->IsDrawable() && phi->IsVisible())
    {             // paint over transparent layer
        DrawableItem* pDrwbl = phi->GetDrawable(true);    // visible only, dot, ellipse, rectangle, scribble, text
        FalconPenKind pk = pDrwbl->PenKind();
        int pw = pDrwbl->penWidth;

        pDrwbl->Draw(_pDrawablePainter, _actPage.screenArea.topLeft());
    }
    */

}

void MyPrinter::_PrintGrid()
{
    qreal x, y,
        dx = _data.nGridSpacingX,
        dy = _data.nGridSpacingY;

    if (_data.gridIsFixed)
        x =  dx, y = dy;
    else
    {
        x = dx - ((int)_actPage.screenArea.x() % _data.nGridSpacingX);
        y = dy - ((int)_actPage.screenArea.y() % _data.nGridSpacingY);
    }
        
    _pImagePainter->setPen(QPen(_data.gridColor, 2, Qt::SolidLine));
    
    int w = PageParams::screenPageWidth, 
        h = PageParams::screenPageHeight;
    _pImagePainter->drawRect(QRectF(0,0,w, h));

    for (; y <= h; y += dy)
        _pImagePainter->drawLine(0, y, w, y);    
    for (; x <= w; x += dx)
        _pImagePainter->drawLine(x, 0, x, h);
}

void MyPrinter::_PreparePage(int page)
{
    _actPage = _pages[page];

    auto compareByZorder = [&](int& left, int& right)
    {
        return _pHist->Item(left)->ZOrder() < _pHist->Item(right)->ZOrder();
    };
    std::sort(_actPage.yindices.begin(), _actPage.yindices.end(), compareByZorder);     // re- sort by z-order

    _pItemImage->fill(Qt::transparent);

    ScreenMode m = globalDrawColors.ActualMode();
    // bottom layer
    if (globalDrawColors.IsDarkMode() || (PageParams::flags & printWhiteBackgroundFlag) != 0)
    {
        globalDrawColors.SetDarkMode(ScreenMode::smWhite);
        _pPageImage->fill(Qt::white);
    }
    else
        _pPageImage->fill(_data.backgroundColor);
    // background image layer
    if ((PageParams::flags & printBackgroundImageFlag) != 0 && _data.pBackgroundImage)
        _pImagePainter->drawImage(0, 0, *_data.pBackgroundImage);   // background image image always start at top left of page
    // grid layer
    if ((PageParams::flags & printGridFlag) != 0)  // grid is below any scribbles
        _PrintGrid();                 // on _pPageImage
    // items layer
    QRectF clipR = _actPage.screenArea;
    for (auto ix : _actPage.yindices)
    {
        _PrintItem(ix, clipR);
        if (_pProgress)
        {
            _pProgress->Progress(_actPage.pageNumber);
            QApplication::processEvents();
        }
    }
    _pImagePainter->drawImage(QPointF(0,0), *_pItemImage);   // all items to pPageImage at top of images
    globalDrawColors.SetDarkMode(m);

}

// do not call this directly. Call _Print(from = which, to=which) instead 
bool MyPrinter::_PrintPage(int page, bool last)
{
    _PreparePage(page);    // into _pPageImage
    // end composition
    // we apply the margins here:
    QRectF target = _data.TargetRectInDots((_actPage.pageNumber & 1) == 0);
    _pPrintPainter->drawImage(target, *_pPageImage);
    // page number if any
    if (PageParams::flags & usePageNumbersFlag)
    {
        const int fontHeightInPixels = 40;
        QFont pgNumFont;
        pgNumFont.setFamilies({ "Tms Rmn","Times", "Times New Roman"});
        pgNumFont.setPixelSize(fontHeightInPixels); //  setPointSize(26);
        pgNumFont = QFont(pgNumFont, _pItemImage);
        QString sNum; 
        sNum.setNum(page + PageParams::startPageNumber);
        QFontMetrics fm(pgNumFont);
        QSize size = fm.size(Qt::TextSingleLine, sNum);
            // get number position
        int x, y;
        QRectF rectp = _data.PrintAreaInDots();
        // qreal d4p = PageParams::screenPageWidth / _data.PrintAreaInDots().width(); // dots for pixel
        y = 4 * fontHeightInPixels; //_data.ConvertToDots(0.05) * d4p;   // top / bottom / left / center / right margin for page number 

        unsigned pos = PageParams::pgPositionOnPage;

        if (!pos || pos == 3)            // left
            x = y;
        else if (pos == 1||pos == 4)      // center
            x = (rectp.width() - size.width()) / 2;
        else                            // right
            x = rectp.width() - size.width() - y;

        if (pos >2)                     // bottom
            y = rectp.height() - size.height() - y;   // bottom

        _pPrintPainter->setFont(pgNumFont);
        _pPrintPainter->drawText(QPoint(x, y), sNum);
    }
    // DEBUG
    //_pPageImage->save(QString("pageImage_%1.png").arg(which));
    //_pItemImage->save(QString("itemImage_%1.png").arg(which));
    // /DEBUG
    if(!last)
        _printer->newPage();
    
    return true;
}

 
/*=============================================================
 * TASK:    this is the main print function. Frees resources afterwards
 * PARAMS:  pages: vector of pages to print
 * GLOBALS:
 * RETURNS:
 * REMARKS:
 *------------------------------------------------------------*/
bool MyPrinter::_Print(IntVector& pages)
{
    bool res = true;
	for (int i = 0; i < pages.size() && res; ++i)
		res &= _PrintPage(i, i == pages.size() - 1);
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
            int nCurrent = _PageForPoint(_data.topLeftOfCurrentPage);
            from = nCurrent; to = nCurrent;
        }
        else if (range == QPrinter::AllPages)
        {
            from = 1; to = _pages.size();
        }
        else if (range == QPrinter::PageRange)
        {
            from = _printer->fromPage()-1;   
            to =   _printer->toPage()-1;
        }
        else          // selection
        {
            QMessageBox::warning(nullptr, FB_WARNING, QObject::tr("This option is not yet implemented"));
            _FreeResources();
            return true;
        }
        return _Print(from, to);   // calls FreeResources() at end
            // cleanup
    }
    return false;
}

