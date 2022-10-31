#pragma once
#ifndef _DRAWAREA_H
#define _DRAWAREA_H

#include <QColor>
#include <QImage>
#include <QBitmap>
#include <QPointF>
#include <QWidget>
#include <QRubberBand>
#include <QTabletEvent>

#include <thread>
#include <vector>

#include "common.h"
#include "drawables.h"
#include "history.h"
#include "pagesetup.h"
#include "myprinter.h"
#include "myevent.h"

// DEBUG
//#include "screenshotTransparency.h"


using namespace std::chrono_literals;

#ifdef _VIEWER
    #define WindowTitle  "FalconBoard Viewer "
#else
    #define WindowTitle  "FalconBoard "
#endif

// ******************************************************

class DrawArea : public QWidget
{       
    Q_OBJECT

public:

    DrawArea(QWidget* parent = nullptr);
    virtual ~DrawArea()
    {
        for (auto ph : historyList)
            delete ph;
        historyList.clear();
#ifndef _VIEWER
        delete _rubberBand;
#endif
    }

    void SetScreenSize(QSize screenSize);

    void SetMyPrinterData(const MyPrinterData& prdata);

    void ClearBackground();
             //  **************** history handling **************
    int AddHistory(const QString name = QString(), bool loadIt = true, int indexAt = 1000000); // with name it may load it as well
    bool SwitchToHistory(int index, bool redraw, bool invalidate=false);   // use this before others
    int RemoveHistory(int index);
    void MoveHistory(int from, int to);   // from _currentHistoryIndex

    QString HistoryName(QString qsDefault) const
    {
        return HistoryName(-1, qsDefault);
    }

    QString HistoryName(int index = -1, QString qsDefault=QString()) const       // may be empty!
    {
        if (index < 0)
            return _history ? (_history->Size() ? _history->Name() : qsDefault) : qsDefault;
            
        if (index >= HistoryListSize())
            return QString();
        return historyList[index]->Name(); // may be empty!
    }

    int SameFileAlreadyUsed(QString& name)
    {
        for (int i=0; i < (int)historyList.size(); ++i)	// no list so big to require unsigned long
            if (historyList[i]->Name() == name)
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
    SaveResult Save(QString name, int index=-1) 
    { 
        HideRubberBand(true);

        if (index < 0) 
            index = _currentHistoryIndex;
        return  historyList[index]->Save(name); 
    }

    bool OpenBackgroundImage(const QString& fileName);
    bool SaveVisibleImage(const QString& fileName, const char* fileFormat);
//    void SetBackgroundImage(QImage& image);
    void InsertVertSpace();         // from top left of rubber band with height of rubber rectangle
    FalconPenKind PenKindFromKey(int key);  // keyboard press/menu click
    bool RecolorSelected(int key); // true: recolored
    void SetLimitedPage(bool limited) { _limited = limited; }

    void SynthesizeKeyEvent(Qt::Key key, Qt::KeyboardModifier mod=Qt::NoModifier);
/*
    void RotateImage(MyRotation rot);
    void FlipImage(bool horiz, bool vert);
    void DrawRect();    // use _rubberBand
    void DrawEllipse();
*/
    void HideRubberBand(bool del=false);
    void ApplyTransparencyToLoadedScreenshots(QColor trcolor, qreal fuzzyness);
#endif

    void GotoPage(int page);

    void SetMode(bool darkMode, QString color, QString gridColor, QString pageGuideColor);
    void SetBackgroundColor(QColor bck) { _backgroundColor = bck;  }    // light/ dark / black mode
    void SetPenKind(FalconPenKind newKind, int newWidth);

    void SetOrigin() { _topLeft = QPointF(); }

    void AddScreenShotImage(QPixmap& image);

    int HistoryListSize() const { return (int)historyList.size(); }

    int IsModified(int fromIndex=-1, bool any = false) const;
    int ActHistoryIndex() const { return _currentHistoryIndex; }
    int AnyHistoryToSave() const
    {
        return historyList.size() > 1 || (historyList.size() == 1 && !_history->IsModified()) ? historyList.size() : -1;
    }

    FalconPenKind PenKind() const { return _actPenKind;  }
    int PenWidth() const { return    _actPenWidth; }

    void SetCursor(DrawCursorShape cs);

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
// DEBUG
    //void _SelectTransparentPixelColor()
    //{
    //    QColor c; bool b;
    //    ScreenShotTransparencyDialog* dlg = new ScreenShotTransparencyDialog(this, c, b);
    //    dlg->exec();
    //}
    bool PageSetup(PageSetupDialog::WhatToDo what);
signals:
    void CanUndo(bool state);     // state: true -> can undo
    void CanRedo (bool  state);   // state: true -> can redo
    void WantFocus();
    void PointerTypeChange(QTabletEvent::PointerType pt);
    void TextToToolbar(QString text);
    void IncreaseBrushSize(int quantity);
    void DecreaseBrushSize(int quantity);
    void RubberBandSelection(bool on);  // used when a selection is active
    void PenKindChange(FalconPenKind pk);
    void CloseTab(int tab);
    void TabSwitched(int direction);

public slots:
    void NewData();
    void ClearRoll();
    void ClearVisibleScreen();
    void ClearDown();
    void ClearHistory();
    void Print(QString fileName, QString *pdir=nullptr);
    void ExportPdf(QString fileName, QString &directory);   // filename w.o. directory
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
    History *_history=nullptr;      // actual history (every scribble and image element with undo/redo)
    int _currentHistoryIndex = -1,  // actual history index, -1: none
        _previousHistoryIndex = -1; // this was the current index before something happened

    bool _mustRedrawArea = true;    // else no redraw
    bool _redrawPending = false;    // redraw requested when it was not enabled
           // page setup
    int // _screenWidth = 1920,        // screen width ==> pageWidth (portrait) / pageHeight (landscape)  
        _screenHeight = 1080;
    bool _limited = false;          // true: page width is fixed (= screen width)
    float _ppi = 96;                // pixels per screen inches
        // printer
    MyPrinterData _prdata;
    MyPrinter* _printer = nullptr;

            // key states used
    bool    _spaceBarDown = false;  // when true canvas is moved with the mouse or the pen
    Qt::KeyboardModifiers _mods;

    bool    _drawStarted = false;   // calculate start vector
    bool    _isHorizontal;          // when _shiftKeyDown, calculated from first 2 point: (y1-y0 > x1-x0,) +> vertical, etc
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
    int     _actPenWidth = 1;
    FalconPenKind _actPenKind = penBlack;

    bool    _bPageSetupValid;
    bool    _bPageSetupUsed = false;
    bool    _openPDFInViewerAfterPrint = false;

    QPointF _actMousePos;   // set in mouse/tablet events


    std::chrono::milliseconds _msecs = 10ms;   // when delayed playback
    bool _delayedPlayback = false; // ???? to make it work not user drawing should be done in an other thread
   
    // final image is shown on the widget in paintEvent(). 
    // These images are the logical layers (the order the objects are drown) 
    // to display on the widget
    // Layers from top to bottom:
    //      _sprite         - only when moving graphs by the mouse, transparent image just holds the sprite
    //      _canvas         - transparent QImage with drawings
    //      _belowImage(es) - non-transparent screenshots or image loaded from file
    //      _grid           - not an image. Just drawn after background when it must be shown moves together with the canvas window
    //      _background     - image loaded from file
    //      the DrawArea widget - background color
    //DrawableList _copiedItems;              // replaced by  'HistoryList::CopiedItems other items to this list 
    // QRectF _copiedRect;                    // replaced by  'HistoryList::CopiedRect()' bounding rectangle for copied items used for paste operation

    QImage  _background,
            _canvas1, _canvas2;      // transparent layer, draw on this then show background and this on the widget
                            // origin: (0,0) point on canvas first shown
    QImage *_pActCanvas, *_pOtherCanvas;        // point to actual canvas to be painted and next canvas to draw on
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
    bool   _bPageGuidesOn = false;          // true: draw line for page breaks on printer depending on _pageHeight and _pageWidth
    int    _nGridSpacingX = 64,
           _nGridSpacingY = 64;
    QColor _gridColor = "#d0d0d0";          // for white system color scheme
    QColor _pageGuideColor = "#fcd475";     // - " - r 

    QPointF _topLeft,   // actual top left of visible canvas, relative to origin  either (0,0) or positive x and/or y values
            _lastMove;  // value of last canvas move 

    const int SmallStep = 1,           // used ehen the shift is pressed with the arrow keys
              NormalStep = 10,         // shift screen this many pixels on one press on arrow key 
              LargeStep = 100;         // or with this many pixels if holding down the Ctrl key 

    QPointF _firstPointC, // canvas relative first point drawn
            _lastPointC;  // canvas relative last point drawn relative to visible image
    DrawableItem*   _pLastDrawableItem = nullptr; // set to either of the following:
    DrawableCross   _lastDrawableCross;
    DrawableDot      _lastDotItem;
    DrawableEllipse  _lastEllipseItem;
    DrawableLine     _lastLineItem;
    DrawableRectangle _lastRectangleItem;
    DrawableScribble _lastScribbleItem;
    DrawableText     _lastTextItem;

    QCursor _savedCursor;
    bool _cursorSaved = false;

    QPointF _lastCursorPos;  // set in MyMoveEvent()


    QRectF   _canvasRect;    // document (0,0) relative rectangle
    QRectF   _clippingRect;  // this too, only draw strokes inside this

#ifndef _VIEWER
    enum class _ScrollDirection {scrollNone, scrollUp, scrollDown, scrollLeft,scrollRight };
    _ScrollDirection _scrollDir = _ScrollDirection::scrollNone;
    QTimer* _pScrollTimer = nullptr;     // page scroll timer Set when there's a sprite and the 
                                         // mouse position is near any edge and it is allowed
                                         // to scroll in that direction
    void _AddScrollTimer();
    void _RemoveScrollTimer();
    _ScrollDirection _AutoScrollDirection(QPointF pt);    // sets and returns _scrollDir


private:
    QRubberBand* _rubberBand = nullptr;	// mouse selection with right button
    QPointF   _rubber_origin,            // position where we stated to draw the rubber band
             _topLeftWhenRubber;        // top left when the rubbar band was hidden
                                        // used to re-show rubberband after a scroll
                                        // or during move paper 
    QRectF   _rubberRect;        // used to select histoy items

    void _InitRubberBand( MyPointerEvent* event);
    void _ReshowRubberBand();

    void _InitiateDrawingIngFromLastPos();   // from _lastPoint
    void _InitiateDrawing(MyPointerEvent* event);
#endif
    void _ClearCanvas();

    void _SetTopLeftFromItem(HistoryItem *phi);   // possibly sets _topLeft. Must _redraw after it
    int CollectDrawables(IntVector &hv); // for actual clipping rect
#ifndef _VIEWER
    void _SetLastPointPosition();           // for actual _history
    bool _CanSavePoint(QPointF &endpoint);    //used for constrained drawing using _lastScribbleItem.points[0]
    QPointF _CorrectForDirection(QPointF &newp);     // using _drawStarted and _isHorizontal
    void _CreatePens();
#endif

    bool _DrawFreehandLineTo(QPointF endPoint); // uses _DrawLineTo but checks for special lines (vertical or horizontal)
    void _DrawLineTo(QPointF endPoint);   // from _lastPointC to endPoint, on _canvas then sets _lastPoint = endPoint
                                         // returns true if new _lastPointC should be saved, otherwise line was not drawn yet
    void _DrawAllPoints(DrawableItem* pscrbl);
    void _ResizeImage(QImage* image, const QSize& newSize, bool isTransparent);

    bool _ReplotDrawableItem(DrawableItem* pscrbl); 
    void _SetCanvasAndClippingRect();
    void _Redraw(bool clear=true);   // before plot
    void _DrawGrid(QPainter &painter);
    void _DrawPageGuides(QPainter& painter);
    QColor _PenColor();
    void _SaveCursorAndReplaceItWith(QCursor newCursor);
    void _RestoreCursor();
    QPainter *_GetPainter(QImage *pCanvas);
#ifndef _VIEWER
    void _ModifyIfSpecialDirection(QPointF & qp);   // modify qp by multiplying with the start vector
#endif
    void _SetOrigin(QPointF qp);  // sets new topleft and displays it on label
    void _ShiftOrigin(QPointF delta);    // delta changes _topLeft, delta.x < 0: scroll right, delta y < 0 scroll down
    void _ShiftRectangle(QPointF delta, QRectF &clip1, QRectF &clip2);
    void _ShiftAndDisplayBy(QPointF delta, bool smooth = false);
    void _PageUp();
    void _PageDown();
    void _Home(bool toTop);     // else just position x=0 
    void _End(bool toBottom);   // else just position: rightmost pixel ofscribble
    void _Up(int distance = 10);
    void _Down(int distance = 10);
    void _Left(int distance = 10);
    void _Right(int distance = 10);
    bool _NoPrintProblems();           // false: some problems
#ifndef _VIEWER
    void _ShowCoordinates(const QPointF& qp);   // and sets mouse position into _actMousePos
    Sprite * _CreateSprite(QPointF cursorPos, QRectF& rect, bool itemsDeleted, bool setVisible=true);
    Sprite * _PrepareSprite(Sprite *pSprite, QPointF cursorPos, QRectF rect, bool itemsDeleted, bool setVisible=true);
    Sprite * _SpriteFromLists(); // from _copiedImages and _copiedItems lists and _copiedRect sprite will not be visible
    void _MoveSprite(QPointF pt);
    void _PasteSprite();


    // DEBUG
    bool _bSmoothDebug = true;
    // end DEBUG

private slots:
    void _ScrollTimerSlot();
#endif
};

#endif
