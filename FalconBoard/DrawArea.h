#pragma once
#ifndef _DRAWAREA_H
#define _DRAWAREA_H

#include <QColor>
#include <QImage>
#include <QBitmap>
#include <QPoint>
#include <QWidget>
#include <QRubberBand>
#include <QTabletEvent>

#include <thread>
#include <vector>

#include "history.h"
#include "pagesetup.h"
#include "pdfsetup.h"
#include "myprinter.h"
#include "myevent.h"

using namespace std::chrono_literals;

#ifdef _VIEWER
    #define WindowTitle  "FalconBoard Viewer "
#else
    #define WindowTitle  "FalconBoard "
#endif

inline void SleepFor(std::chrono::milliseconds mseconds)
{
    std::this_thread::sleep_for(mseconds); // Delay a bit
}
// ******************************************************

class DHistory : public History
{
public:
    QPoint topLeft;

    DHistory() noexcept : History() {}
    DHistory(const DHistory& o) : History(o), topLeft(o.topLeft) {}
    DHistory(const DHistory&& o) noexcept : History(o), topLeft(o.topLeft) {};
    ~DHistory() {}

};

using HistoryList = std::vector<DHistory*>;

class DrawArea : public QWidget
{       
    Q_OBJECT

public:
        // cursors for drawing: arrow, cross for draing, opena and closed hand for moving, 
    enum CursorShape {csArrow, csCross, csOHand, csCHand, csPen, csEraser };

    DrawArea(QWidget* parent = nullptr);
    virtual ~DrawArea()
    {
        for (auto ph : _historyList)
            delete ph;
        _historyList.clear();
    }

    void SetPrinterData(const MyPrinterData& prdata);

    void ClearBackground();
             //  **************** history handling **************
    int AddHistory(const QString name = QString(), bool loadIt = true, int indexAt = 1000000); // with name it may load it as well
    bool SwitchToHistory(int index, bool redraw);   // use this before others
    int RemoveHistory(int index);
    void MoveHistory(int to);   // from _currentHistoryIndex

    QString HistoryName(QString default) const
    {
        return HistoryName(-1, default);
    }

    QString HistoryName(int index = -1, QString default=QString()) const       // may be empty!
    {
        if (index < 0)
            return _history ? _history->Name() : default;
            
        if (index >= HistoryListSize())
            return QString();
        return _historyList[index]->Name(); // may be empty!
    }

    int SameFileAlreadyUsed(QString& name)
    {
        for (int i=0; i < _historyList.size(); ++i)
            if (_historyList[i]->Name() == name)
                return i;
        return -1;
    }

    void SetHistoryName(QString fileName)
    {
        _history->SetName(fileName);
    }

    //------------------------------------------------------
    int Load();         // into current history
    bool EnableRedraw(bool value);
#ifndef _VIEWER
    bool Save(QString name, int index=-1) 
    { 
        if (index < 0) 
            index = _currentHistoryIndex;
        return  _historyList[index]->Save(name); 
    }

    bool OpenBackgroundImage(const QString& fileName);
    bool SaveVisibleImage(const QString& fileName, const char* fileFormat);
//    void SetBackgroundImage(QImage& image);
    void InsertVertSpace();         // from top left of rubber band with height of rubber rectangle
    MyPenKind PenKindFromKey(int key);  // keyboard press/menu click
    bool RecolorSelected(int key); // true: recolored
    void SetLimitedPage(bool limited) { _limited = limited; }
#endif

    void SetMode(bool darkMode, QString color, QString gridColor, QString pageGuideColor);
    void SetBackgroundColor(QColor bck) { _backgroundColor = bck;  }    // light/ dark / black mode
    void SetPenKind(MyPenKind newKind, int newWidth);

    void SetOrigin() { _topLeft = QPoint(); }

    void AddScreenShotImage(QImage& image);

    int HistoryListSize() const { return (int)_historyList.size(); }

    int IsModified(bool any = false) const;
    int IsModified(int index) const;

    MyPenKind PenKind() const { return _myPenKind;  }
    int PenWidth() const { return    _actPenWidth; }

    void SetCursor(CursorShape cs, QIcon* icon = nullptr);
    void SetEraserCursor(QIcon *icon = nullptr);

    void SetGridOn(bool on, bool fixed);
    void SetPageGuidesOn(bool on);

    void Print(int index = -1)
    {
        if (index < 0)
            index = _currentHistoryIndex;
        if (index <0 || index > HistoryListSize())
            return;
        Print(HistoryName(index));
    }

signals:
    void CanUndo(bool state);     // state: true -> can undo
    void CanRedo (bool  state);   // state: true -> can redo
    void WantFocus();
    void PointerTypeChange(QTabletEvent::PointerType pt);
    void TextToToolbar(QString text);
    void IncreaseBrushSize(int quantity);
    void DecreaseBrushSize(int quantity);
    void RubberBandSelection(bool on);  // used when a selection is active
    void PenKindChange(MyPenKind pk);
    void CloseTab(int tab);

public slots:
    void NewData();
    void ClearRoll();
    void ClearVisibleScreen();
    void ClearDown();
    void ClearHistory();
    void Print(QString fileName, QString *pdir=nullptr);
    void ExportPdf(QString fileName, QString &directory);   // filename w.o. directory
    void PageSetup();
    void SlotForPrimaryScreenChanged(QScreen*);
    void SlotForGridSpacingChanged(int);
#ifndef _VIEWER
    void Undo();
    void Redo();
    void ChangePenColorSlot(int key);
#endif

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    void wheelEvent(QWheelEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void tabletEvent(QTabletEvent* event) override;

    // MyEvent type common handlers for the above group
    void MyButtonPressEvent(MyPointerEvent* pe);
    void MyMoveEvent(MyPointerEvent* pe);
    void MyButtonReleaseEvent(MyPointerEvent* pe);

#ifndef _VIEWER
private:
    void ChangePenColorByKeyboard(int key);
#endif
private:
    DHistory *_history=nullptr;            // actual history (every scribble element with undo/redo)
    HistoryList _historyList;   // many histories are possible
    int _currentHistoryIndex = -1,  // actual history index, -1: none
        _previousHistoryIndex = -1;  // this was the current index before something happened

    bool _mustRedrawArea = true;    // else no redraw
    bool _redrawPending = false;    // redraw requested when it was not enabled
           // page setup
    int _screenWidth = 1920,        // screen width ==> _pageWidth (portrait) / _pageHeight (landscape)  
        _screenHeight = 1080;
    bool _limited = false;          // true: page width is fixed (= screen width)
    float _ppi = 96;                // pixels per screen inches
        // printer
    MyPrinterData _prdata;
    MyPrinter* _printer = nullptr;

            // key states used
    bool    _spaceBarDown = false;  // true when canvas is moved with the mouse or the pen
    Qt::KeyboardModifiers _mods;

    bool    _startSet = false;      // calculate start vector
    bool    _isHorizontal;          // when _shitKeyDown, calculated from first 2 point: (y1-y0 > x1-x0,) +> vertical, etc
                                    // and used in line drawing

    bool    _allowPen = true;       // true if message is only accepted from the pen
    bool    _allowMouse = true;     // true if message is only accepted from the mouse
    bool    _scribbling = false;    // true for mouse drawing (_spaceBarDown == false)
    bool    _pendown = false;       // true for pen
    bool    _erasemode = false;
    bool    _debugmode = false;     // can only be toggled when debug compile and Ctrl+D is pressed 
    bool    _pencilmode = false;    // pen width is not used
    bool    _itemsCopied = false;   // copy/copy & delete : list of scribbles into 'history::_nSelectedItems';

    int     _penWidth = 1;
    QCursor _eraserCursor;
    int     _actPenWidth = 1;
    MyPenKind _myPenKind = penBlack;
    bool    _bPageSetupValid;
    bool    _bPageSetupUsed = false;


    std::chrono::milliseconds _msecs = 10ms;   // when delayed playback
    bool _delayedPlayback = false; // ???? to make it work not user drawing should be done in an other thread
   
    // final image is shown on the widget in paintEvent(). 
    // These images are the layers to display on the widget
    // Layers from top to bottom:
    //      _sprite         - only when moving graphs by the mouse, transparent image just holds the sprite
    //      _canvas         - transparent QImage with drawings
    //      _belowImage(es) - non-transparent screenshots or image loaded from file
    //      _grid           - not an image. Just drawn after background when it must be shown moves together with the canvas window
    //      _background     - image loaded from file
    //      the DrawArea widget - background color
    ScreenShotImageList _copiedImages;     // copy images on _nSelectedList to this
    ScribbleItemVector _copiedItems;       // other items to this list 
    QRect _copiedRect;                     // bounding rectangle for copied items used for paste operation

    QImage  _background,
            _canvas;        // transparent layer, draw on this then show background and this on the widget
                            // origin: (0,0) point on canvas first shown
    // to move selected area around
    Sprite * _pSprite = nullptr;         // copy of elements in _rubberRect to move around

    bool    _isBackgroundSet = false;      // an image has been loaded into _background
    bool    _fixedBackground = true; // background will not scroll with image

    bool _darkMode = false;
    QColor  _backgroundColor = "#FFFFFF",
        _textColor = "#000000";
                                // grid 
    bool   _bGridOn = false;
    bool   _gridIsFixed = false;
    bool   _bPageGuidesOn = false;          // true: draw line on page breaks depending on _pageHeight and _pageWidth
    int    _nGridSpacingX = 64,
           _nGridSpacingY = 64;
    QColor _gridColor = "#d0d0d0";          // for white system color scheme
    QColor _pageGuideColor = "#fcd475";     // - " - r 

    QPoint  _topLeft,   // actual top left of visible canvas, relative to origin  either 0 or positive values
            _lastMove;  // value of last canvas move 

    QPoint  _firstPointC, // canvas relative first point drawn
            _lastPointC; // canvas relative last point drawn relative to visible image
    ScribbleItem _lastScribbleItem;
    QCursor _savedCursor;
    bool _cursorSaved = false;


    QRect   _canvasRect;    // 0,0 relative rectangle
    QRect   _clippingRect;  // only need to draw here

#ifndef _VIEWER
    QRubberBand* _rubberBand = nullptr;	// mouse selection with right button
    QPoint   _rubber_origin;
    QRect   _rubberRect;        // used to select histoy items
    void  _RemoveRubberBand();
    void _InitiateDrawingIngFromLastPos();   // from _lastPoint
    void _InitiateDrawing(MyPointerEvent* event);
    void _InitRubberBand( MyPointerEvent* event);
#endif
    void _ClearCanvas();

    void _MoveToActualPosition(QRect rect);
    int _CollectScribbles(HistoryItemVector &hv); // for actual clipping rect
#ifndef _VIEWER
    bool _CanSavePoint(QPoint &endpoint);    //used for constrained drawing using _lastScribbleItem.points[0]
    QPoint _CorrectForDirection(QPoint &newp);     // using _startSet and _isHorizontal
#endif

    bool _DrawFreehandLineTo(QPoint endPoint); // uses _DrawLineTo but checks for special lines (vertical or horizontal)
    void _DrawLineTo(QPoint endPoint);   // from _lastPointC to endPoint, on _canvas then sets _lastPoint = endPoint
                                         // returns true if new _lastPointC should be saved, otherwise line was not drawn yet
    void _DrawAllPoints(ScribbleItem* pscrbl);
    void _ResizeImage(QImage* image, const QSize& newSize, bool isTransparent);

    bool _ReplotScribbleItem(HistoryItem* pscrbl); 
    void _SetCanvasRect();
    void _Redraw(bool clear=true);   // before plot
    void _DrawGrid(QPainter &painter);
    void _DrawPageGuides(QPainter& painter);
    QColor _PenColor();
    void _SaveCursorAndReplaceItWith(QCursor newCursor);
    void _RestoreCursor();
#ifndef _VIEWER
    void _ModifyIfSpecialDirection(QPoint & qp);   // modify qp by multiplying with the start vector
#endif
    void _SetOrigin(QPoint qp);  // sets new topleft and displays it on label
    void _ShiftOrigin(QPoint delta);    // delta changes _topLeft, delta.x < 0: scroll right, delta y < 0 scroll down
    void _ShiftAndDisplayBy(QPoint delta, bool smooth = false);
    void _PageUp();
    void _PageDown();
    void _Home(bool toTop);
    void _End();
    void _Up(int distance = 10);
    void _Down(int distance = 10);
    void _Left(int distance = 10);
    void _Right(int distance = 10);
    bool _PdfPageSetup();               // false: cancelled
    bool _NoPrintProblems();           // false: some problems
#ifndef _VIEWER
    void _ShowCoordinates(const QPoint& qp);
    Sprite * _CreateSprite(QPoint cursorPos, QRect& rect, bool itemsDeleted);
    void _MoveSprite(QPoint pt);
    void _PasteSprite();
#endif
};

#endif
