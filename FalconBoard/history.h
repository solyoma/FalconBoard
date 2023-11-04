#pragma once
#ifndef _HISTORY_H
#define _HISTORY_H

#include <algorithm>

#include "pagesetup.h"
#include "drawables.h"

/*========================================================
 *  special options to save in file, needed for viewer
 --------------------------------------------------------*/
struct GridOptions
{
    enum opt { optNone=0x0000, optGridOn = 0x1000, optFixedGrid=0x2000 }; // max grid size is 400 (0x190) px
    bool gridOn = false;
    bool fixedGrid = false;
    uint16_t gridSpacing = 100;      // max 400

    operator uint16_t()
    {
        uint16_t u = gridSpacing;
        if(gridOn)
            u |= (uint16_t)optGridOn;
        if (fixedGrid)
            u |= (uint16_t)optFixedGrid;
        return u;
    }
    void operator=(uint16_t u)
    {
        gridOn = (u & (uint16_t)optGridOn);
        fixedGrid = u & (uint16_t)optFixedGrid;
        gridSpacing = u & ~((uint16_t)optGridOn | (uint16_t)optFixedGrid);
    }
};

/*========================================================
 * One history element
 *-------------------------------------------------------*/

struct History; // forward declaration it contains the 'DrawableList' common for this history

struct HistoryItem      // base class
{
    History* pHist;
    HistEvent type;
    DrawableList* pDrawables = nullptr;         // to pHist's _drawables

    HistoryItem(History* pHist, HistEvent typ = HistEvent::heNone);
    HistoryItem(const HistoryItem &other);
    virtual ~HistoryItem() {}

    virtual int ZOrder() const { return 0; }

    virtual QPointF TopLeft() const { return QPoint(); }
    virtual DrawableItem* GetDrawable(bool onlyVisible = false, int* pIndex = nullptr) const 
    {     
        if (pIndex)
            *pIndex = -1;
        return nullptr; 
    }

    virtual bool Translatable() const { return false;  }
    virtual void Translate(QPointF p, int minY) { } // translates if top is >= minY
    virtual void Rotate(MyRotation rot, QPointF center) { ; }      // rotation or flip
    virtual int Size() const { return 0; }         // size of stored scribbles or erases
    virtual void SetVisibility(bool visible) { }

    // function prototypes for easier access
    virtual int RedoCount() const { return 1; } // how many elements from _redoList must be moved back to _items
    virtual int Undo() { return 1; }        // returns amount _actItem in History need to be changed (go below 1 item)
    virtual int Redo() { return 0; }        // returns amount _actItem in History need to be changed (after ++_actItem still add this to it)
    virtual QRectF Area() const { return QRectF(); }  // encompassing rectangle for all points
    virtual bool IsVisible() const { return false; }
    virtual bool IsHidden() const { return !IsVisible(); }    // must not draw it
    virtual bool IsSaveable() const  { return IsVisible(); }    // use when items may be hidden
    virtual bool IsDrawable() const { return false; }
    virtual DrawableType Type() const { return DrawableType::dtNone; }
    virtual bool IsImage() const { return false; }
    virtual bool Intersects(QRectF& rect) const { return GetDrawable(true)->Intersects(rect);  }
    virtual void Draw(QPaintDevice* canvas, QPointF &topLeftOfVisibleArea, const QRectF& clipR) {}                     // sets painter at every draw
    virtual void Draw(QPainter* painter, QPointF& topLeftOfVisibleArea, const QRectF& clipR) {} // when painter is known

    virtual bool operator<(const HistoryItem& other);
};

            //--------------------------------------------
            //      HistoryDrawableItem
            //--------------------------------------------
struct HistoryDrawableItem : public HistoryItem
{
    int indexOfDrawable;          // into History's _drawables'

    HistoryDrawableItem(History* pHist, DrawableItem& dri);   // adds a copy of 'dri'
    HistoryDrawableItem(History* pHist, DrawableItem* pdri);  // 'pDrawableItem is set to be 'pdri'
    HistoryDrawableItem(const HistoryDrawableItem& other);
    HistoryDrawableItem(HistoryDrawableItem&& other) noexcept;
    ~HistoryDrawableItem();

    void SetVisibility(bool visible) override;
    bool IsDrawable() const override { return true; }
    bool IsVisible() const override;
    virtual DrawableType Type() const override { return _Drawable()->dtType; }

    HistoryDrawableItem& operator=(const HistoryDrawableItem& other);
    HistoryDrawableItem& operator=(HistoryDrawableItem&& other) noexcept;

    int ZOrder() const override;
    QPointF TopLeft() const override { return _Drawable()->Area().topLeft(); }
    DrawableItem* GetDrawable(bool onlyVisible = false, int *pIndex=nullptr) const override;
    QRectF Area() const override;
    int Size() const override { return 1; }

    int Undo() override;        // returns amount _actItem in History need to be changed (go below 1 item)
    int Redo() override;        // returns amount _actItem in History need to be changed (after ++_actItem still add this to it)

    bool Translatable() const override { return _Drawable()->isVisible; }
    void Translate(QPointF p, int minY) override; // only if not deleted and top is > minY
    void Rotate(MyRotation rot, QPointF center) override;
    // no new undo/redo needed
    bool IsImage() const override { return _Drawable()->dtType == DrawableType::dtScreenShot ? true: false; }
    void Draw(QPainter* painter, QPointF& topLeftOfVisibleArea, const QRectF& clipR) override
    { 
        _Drawable()->Draw(painter, topLeftOfVisibleArea, clipR); 
    }
    void Draw(QPaintDevice *canvas, QPointF &topLeftOfVisibleArea, const QRectF& clipR) override
    { 
        QPainter painter(canvas);
        _Drawable()->Draw(&painter, topLeftOfVisibleArea, clipR); 
    }
private:
    inline DrawableItem* _Drawable() const { return (*pDrawables)[indexOfDrawable]; }
};


            //--------------------------------------------
            //      HistoryDeleteItems
            //--------------------------------------------

//--------------------------------------------
struct HistoryDeleteItems : public HistoryItem
{
    DrawableIndexVector deletedList;   // absolute indexes of drawable elements in 'pHist->_drawables'
    int Undo() override;
    int Redo() override;
    HistoryDeleteItems(History* pHist, DrawableIndexVector& selected);
    HistoryDeleteItems(HistoryDeleteItems& other);
    HistoryDeleteItems& operator=(const HistoryDeleteItems& other);
    HistoryDeleteItems(HistoryDeleteItems&& other) noexcept;
    HistoryDeleteItems& operator=(const HistoryDeleteItems&& other) noexcept;
};

            //--------------------------------------------
            //      HistoryRemoveSpaceItem
            //--------------------------------------------

//--------------------------------------------
// remove a rectangular region (RR)
//  There may be items whose area overlaps or fully
//  inside the horizontal band (HB) determined by the top 
//  and bottom of this region or overlaps or fully inside
//  this rectangle.
// 
// 1. no item's area overlaps this HB 
//      => move all items which are below HB 
//          up by the height of RR
// 2.  no items inside this HB, but there are overlaps
//      => find lowest y coordinate for overlapping items and
//          change the height of RR just above it then apply 1.
// 3.  no items overlap or inside RR, and none overlaps HB partially, 
//          but there are items to the right of RR inside HB
//      => move items at the right to the left by the width of RR
//          and store their indices for undo
// 4.  no item overlaps or inside RR, but there are items inside
//          and overlapping HB
//      =>  find lowest y coordinate for overlapping items and
//          change the height of RR and HB just above it then
//          apply 3.
// 5.   there are items overlapping or inside HB but only at
//          left of RR
//      => do nothing (maybe warn)
// 
struct HistoryRemoveSpaceItem : public HistoryItem // using _selectedRect
{
    DrawableIndexVector lstItemsRight;  // items to be moved horizontally (elements on the right)
                                        // if empty move all below y up
        //                                    // usid in UNDO: do not move items on this list
    QRectF rr;                          // RR and determines HB
                  // translate by rr.width() to the left (lstItemsRight) not empty
                  // OR up by rr.height() when lstItemsRight is empty
                  // topmost coordinate to remove space from is rr.bottom()

    int Undo() override;
    int Redo() override;
    HistoryRemoveSpaceItem(History* pHist, DrawableIndexVector&toModify, QRectF rr);
    HistoryRemoveSpaceItem(const HistoryRemoveSpaceItem& other);
    HistoryRemoveSpaceItem& operator=(const HistoryRemoveSpaceItem& other);
    HistoryRemoveSpaceItem(HistoryRemoveSpaceItem&& other) noexcept;
    HistoryRemoveSpaceItem& operator=(HistoryRemoveSpaceItem&& other) noexcept;
};

//--------------------------------------------
// When pasting items into _items from bottom up 
//   'HistoryPasteItemBottom' item (its absolute index is in  
//          HistoryPasteItemTop::indexOfBottomItem
//   'HistoryScribbleItems' items athat were pasted
//   'HistoryPasteItemTop' item
struct HistoryPasteItemBottom : public HistoryItem
{
    int index;          // in pHist
    int count;          // of items pasted above this item
                        // these items came in one group in *pHist's '_items'
    int moved = 0;      // 0: copy / paste, 1: moved with mouse / pen
                        //  set into both bottom and top items

    HistoryPasteItemBottom(History* pHist, int index, int count);

    HistoryPasteItemBottom(HistoryPasteItemBottom& other);
    HistoryPasteItemBottom& operator=(const HistoryPasteItemBottom& other);
    int RedoCount() const override { return count + moved + 2; }
};

            //--------------------------------------------
            //      HistoryPasteItemTop
            //--------------------------------------------

struct HistoryPasteItemTop : public HistoryItem
{
    int indexOfBottomItem;      // abs. index of 'HistoryPastItemBottom' item
    int count;                  // of items pasted  
                                // these items came in one group in *pHist's '_items'
    int moved = 0;              // 0: copy / paste, 1: moved with mouse / pen
                                //  in both bottom and top items
    QRectF boundingRect;

    HistoryPasteItemTop(History* pHist, int index, int count, QRectF& rect);

    HistoryPasteItemTop(const HistoryPasteItemTop& other);
    HistoryPasteItemTop& operator=(const HistoryPasteItemTop& other);
    HistoryPasteItemTop(HistoryPasteItemTop&& other) noexcept;
    HistoryPasteItemTop& operator=(HistoryPasteItemTop&& other) noexcept;

    int Size() const override { return count; }

    DrawableItem* GetNthDrawable(int n) const; // only for top get (count - index)-th below this one
    DrawableItem* GetNthVisibleDrawable(int n) const; // only for top get (count - index)-th below this one

    bool IsVisible() const override;      // only for top: when the first element is visible, all visible
    // bool IsSaveable() const override;
    void SetVisibility(bool visible) override; // for all elements 
    bool Translatable() const override { return IsVisible(); }
    void Translate(QPointF p, int minY) override;    // only if not deleted and top is > minY
    void Rotate(MyRotation rot, QPointF center) override;

    QPointF TopLeft() const override { return boundingRect.topLeft(); }
    QRectF Area() const override;

    int Undo() override;    // only for top item: hide 'count' items below this one
    int Redo() override;    // only for top item: reveal count items
};

            //--------------------------------------------
            //      HistoryRecolorItem
            //--------------------------------------------

struct HistoryReColorItem : public HistoryItem
{
    DrawableIndexVector selectedList;     // indices to drawable elements in '*pHist'
    QVector<FalconPenKind> penKindList;   // colors for elements in selectedList
    FalconPenKind pk;                           
    QRectF boundingRectangle;             // to scroll here when undo/redo

    int Undo() override;
    int Redo() override;
    HistoryReColorItem(History* pHist, DrawableIndexVector&selectedList, FalconPenKind pk);
    HistoryReColorItem(HistoryReColorItem& other);
    HistoryReColorItem& operator=(const HistoryReColorItem& other);
    HistoryReColorItem(HistoryReColorItem&& other) noexcept;
    HistoryReColorItem& operator=(const HistoryReColorItem&& other) noexcept;
    QRectF Area() const override;
};

            //--------------------------------------------
            //      HistoryInsertVertSpace
            //--------------------------------------------

struct HistoryMoveItems : public HistoryItem
{
    QPointF dr; // displacement
    DrawableIndexVector movedItemList;   // absolute indexes of drawable elements in 'pHist->_drawables'

    HistoryMoveItems(History* pHist, QPointF displacement, DrawableIndexVector &selection); // uses _
    HistoryMoveItems(const HistoryMoveItems& other);
    HistoryMoveItems& operator=(const HistoryMoveItems& other);

    int Undo() override;
    int Redo() override;
    QRectF Area() const override;
};

struct HistoryInsertVertSpace : public HistoryItem
{
    int y = 0, heightInPixels = 0;

    HistoryInsertVertSpace(History* pHist, int top, int pixelChange);
    HistoryInsertVertSpace(const HistoryInsertVertSpace& other);
    HistoryInsertVertSpace& operator=(const HistoryInsertVertSpace& other);

    int Undo() override;
    int Redo() override;
    QRectF Area() const override;
};
            //--------------------------------------------
            //      HistoryRotationItem
            //--------------------------------------------

//--------------------------------------------
struct HistoryRotationItem : public HistoryItem
{
    MyRotation rot;
    DrawableIndexVector driSelectedDrawables;
    QRectF encRect;         // encompassing rectangle: before rotation all items are inside this
                            // after rotation they are usually not 
    QPointF center;
    HistoryRotationItem(History* pHist, MyRotation rotation, QRectF rect, DrawableIndexVector selList);
    HistoryRotationItem(const HistoryRotationItem& other);
    HistoryRotationItem& operator=(const HistoryRotationItem& other);
    int Undo() override;
    int Redo() override;
};

            //--------------------------------------------
            //      HistorySetTransparencyForAllScreenshotsItems
            //--------------------------------------------

struct HistorySetTransparencyForAllScreenshotsItems : public HistoryItem
{
    DrawableIndexVector affectedIndexList;   // these images were re-created with transparent color set
    int undoBase = -1;            // top of pHist's screenShotImageList before this function was called
    QColor transparentColor;
    qreal fuzzyness = 0.0;
    
    HistorySetTransparencyForAllScreenshotsItems(History* pHist, QColor transparentColor, qreal fuzzyness);
    int Undo() override;
    int Redo() override;
};


            //--------------------------------------------
            //      HistoryEraserStrokeItem
            //--------------------------------------------

struct HistoryEraserStrokeItem : public HistoryItem
{
    int eraserPenWidth=0;       
    QPolygonF   eraserStroke;
    DrawableIndexVector affectedIndexList;   // these images were re-created with transparent color set
    IntVector subStrokesForAffected;         // number of strokes added to the it-th item on affectedIndexList

    HistoryEraserStrokeItem(History* pHist, DrawableItem& dri);
    HistoryEraserStrokeItem(const HistoryEraserStrokeItem &o);
    HistoryEraserStrokeItem& operator=(const HistoryEraserStrokeItem& o);

    int Undo() override;
    int Redo() override;
};

            //--------------------------------------------
            //      HistoryPenWidthChangeItem
            //--------------------------------------------

struct HistoryPenWidthChangeItem : public HistoryItem
{
    DrawableIndexVector affectedIndexList;
    qreal dw;          // pen width change
    HistoryPenWidthChangeItem(History* pHist, qreal changeWidthBy);
    HistoryPenWidthChangeItem(const HistoryPenWidthChangeItem& o);
    HistoryPenWidthChangeItem& operator=(const HistoryPenWidthChangeItem& o);
    int Undo() override;
    int Redo() override;
};

            //--------------------------------------------
            //      HistoryPenColorChangeItem
            //--------------------------------------------
struct HistoryPenColorChangeItem : public HistoryItem
{
    DrawColors original, redefined;
    HistoryPenColorChangeItem(History* pHist, const DrawColors &origc, const DrawColors &newc);
    HistoryPenColorChangeItem(const HistoryPenColorChangeItem& o);
    HistoryPenColorChangeItem& operator=(const HistoryPenColorChangeItem& o);
    int Undo() override;
    int Redo() override;
};

// ******************************************************
class History;
        // declare here so that CopySelected can use it, but 
        // create and use it in 'DrawArea'

/*========================================================
 * describes an image with transparent background to show 
 *  on top of everything, while containing all of the items
 *  to paste into the list
 *-------------------------------------------------------*/
struct Sprite
{
    Sprite(History* ph);
    Sprite(History* ph, const QRectF &copiedRect, const DrawableIndexVector &dri);
    Sprite(History* ph, const QRectF &copiedRect, const DrawableList &lstDri);

    const int margin = 1; // pixel

    History* pHist;
    QPointF topLeft;     // top,left: position of sprite rel. to top left of visible area of canvas
    QPointF dp;          // relative to top left of sprite: cursor coordinates when the sprite is selected
    QImage image;        // image of the sprite to paint over the canvas larger than rect because of borders
    QRectF rect;         // selection rectangle 0,0, width,height
    bool itemsDeleted = true;   // unless Alt was used when started moving the sprite
    bool visible = true; // copying should create new invisible sprite


    DrawableIndexVector       driSelectedDrawables;   // indices into 'pHist->_drawables' of items to be copied, that are completely inside the rubber band rectangle
                                                      // needed for hiding source items when required
    DrawableList              drawables;              // copied items are duplicates of drawables in 'driSelectedDrawables'
                                                      // each point of items[..] is relative to top-left of sprite (0,0)
};

// vector of pointers to dynamically allocated history items
using HistoryItemVector = QVector<HistoryItem*>;

class HistoryList;

using HistoryItemPointer = HistoryItem*;

/*========================================================
 * Class for storing history of editing
 *  contains data for items drawn on screen, 
 *      item deletion, canvas movement, etc
 *
 *     _items:   generalized list of all items
 *          members are accessed by either added order
 *              or ordered by first y then x coordinates of
 *              their top left rectangle
 *     _redoList: contains undo-ed items
 *     _actItem: index of the the last history item added.
 *              At the same time index of the last element that
 *              must be processed when drawing
 *               new items are added after this index
 *          during undo: this index is decreased, at redo 
 *              it is increased
 *          if a new item is added after an undo it will be inserted
 *              after this position i.e. other existing items in
 *              '_items' will be invalid
 *-------------------------------------------------------*/
class History  // stores all drawing sections and keeps track of undo and redo
{
    friend class HistoryScreenShotItem;
    friend class HistorySetTransparencyForAllScreenshotsItems;

    HistoryList* _parent;           // needed for copy and paste

    HistoryItemVector _items,           // items in the order added. Items need not be drawables.
                                        // drawables use _drables._pItems
                      _redoList;        // from _items for redo. Items need not be scribbles.
                                        // drawable elements on this list may be either visible or hidden
    DrawableList _drawables;            // contains each type, including screenshots
                                        // drawable elements on this list may be either visible or hidden
    int  _resolutionIndex = 6;           // gives PageSetup::resolutionIndex -> full HD
    int  _pageWidthInPixels = 1920;     // full HD
    bool _useResInd = true;              // what to do
                                        // if the value is above 100 it is resolution index

    QuadTreeDelegate _quadTreeDelegate; // for fast display, set into _drawables

    ZorderStore _zorderStore;           // max. zorder 

    QPointF _topLeft;                   // temporary, top left of the visible part of this history
                                        // document relative
    QString _fileName,                  // file for history
            _loadedName;                // set only after the file is loaded, used to check reloads
    bool _inLoad = false;               // in function Load() / needed for correct z- order settings
    int _readCount = 0;                 // undo works until this index is reached
    int _lastSaved = 0;                 // the count of _items when data was saved, after read it is _readCount

                                        // unscribble items have no indices in here
    bool _isSaved = false;              // clear after every change!

    QRectF _clpRect;                    // clipping rectangle for selecting points to draw
                                        // before searching operations set this when it is changed
    QStack<QRectF> _savedClps;          // clipRect saves

    int _indexOfFirstVisible = -1;      // in _yxorder

                // drawable selection

    DrawableIndexVector _driSelectedDrawables,    // indices into '_drawables', that are completely inside the rubber band (includes screenshots - zorder < DRAWABLE_ZORDER_BASE)
              _driSelectedDrawablesAtRight,       // -"- for elements that were at the right of the rubber band
              _driSelectedDrawablesAtLeft;        // -"- for elements that were at the left of the rubber band
    QRectF _selectionRect;              // bounding rectangle for selected items OR rectangle in which there are no items
                                        // when _driSelectedDrawables is empty



    HistoryItem* _AddItem(HistoryItem* p);

    bool _IsSaveable(int i);

    void _SaveClippingRect();
    void _RestoreClippingRect();
    void _ClearSelectLists() 
    { 
        _driSelectedDrawables.clear();
        _driSelectedDrawablesAtRight.clear();
        _driSelectedDrawablesAtLeft.clear();
    }

    int _ReadV1(QDataStream &ifs,DrawableItem &di, qint32 version); // reads items from version 1.X files and returns count of read
    int _ReadV2(QDataStream &ifs,DrawableItem &di);                 // reads items from version 2.X files and returns count of read

    int _LoadV1(QDataStream &ifs, qint32 version);          // load version 1.X files
    int _LoadV2(QDataStream &ifs, qint32 version_loaded);   // load version 2.X files

    void _CantRotateWarning() const;
public:
    GridOptions gridOptions;

    DrawColors drawColors;         // global for all drawables in this history

    History(HistoryList* parent) noexcept;
    History(const History& o);
    History(History&& o) noexcept;
    virtual ~History();


    QSizeF UsedArea();   // of all points and images
    int CountOnPage(int px, int py, QSize pageSize, bool &getAreaSize); // -1: invalid page for px, -2: invalid page for py i.e. outside used area. First call with getAreaSize=true, others with false

    HistoryItemPointer Item(int index) const 
    { 
        return _items[index]; 
    }

    HistoryItemPointer LastItem() const 
    { 
        int n = _items.size();
        return n ? _items[n-1] : nullptr;
    }

    int GetZorder(bool isscreenshot, bool increment=true) { return _zorderStore.GetZorder(isscreenshot, increment); }
    constexpr DrawableList* Drawables()  { return &_drawables; }
    DrawableItem* Drawable(int i) { return _drawables[i]; }
    constexpr const DrawableIndexVector &SelectedDrawables() const 
    { 
        return _driSelectedDrawables; 
    }

    bool CanRotateSelected(MyRotation rot);
    bool RotateSelected(MyRotation rot);

    constexpr QPointF TopLeft() const { return _topLeft; }
    void SetTopLeft(QPointF& topLeft) { _topLeft = topLeft; }

    int RightMostInBand(QRectF rect);    // only top and bottom of rect is used

    void SetClippingRect(const QRectF& rect) { _clpRect = rect;  }

    //DrawableItemIndex AddToDrawables(DrawableItem* pdr)
    int AddToDrawables(DrawableItem* pdr)
    {
        return _drawables.AddDrawable(pdr);
    }

    DrawableScreenShot* ScreenShot(int index) { return (DrawableScreenShot*)_drawables[index]; }
    int /*DrawableItemIndex*/ ScreenShotAtPos(QPointF& p) const { return _drawables.IndexOfTopMostItemUnder(p, 1, DrawableType::dtScreenShot); } // -1: no such image else index in '_drawables'
    DrawableScreenShot* FirstVisibleScreenShot(const QRectF& canvasRect) { return _drawables.FirstVisibleScreenShot(canvasRect); }
    DrawableScreenShot* NextVisibleScreenShot() { return _drawables.NextVisibleScreenShot(); }

    void Clear(bool andDeleteQuadTree = false);
    void ClearName() { if (_items.isEmpty()) _fileName.clear(); }

    int Size() const;   // _items's size
    int CountOfVisible(); // visible 
    int CountButScreenShots();
    int SelectedSize() const { return _driSelectedDrawables.size(); }

    QString Name() const 
    { 
        return _fileName; 
    }
    SaveResult Save(QString name);

    void SetName(QString name)
    {
        _fileName = name;
        Clear();
    }

    int Load(quint32& version_loaded, bool force=false, int fromY=0);       // from '_fileName', returns _items.size() when Ok, -items.size()-1 when read error
    int Append(QString fileName, quint32& version_loaded);
    void SetPageParamsFromHistory() const
    {
        PageParams::resolutionIndex = _resolutionIndex;
        PageParams::horizPixels     = _pageWidthInPixels;
        PageParams::useResInd       = _useResInd;     
    }
    void GetPageParamsToHistory()
    { 
        _resolutionIndex    = PageParams::resolutionIndex   ;
        _pageWidthInPixels  = PageParams::horizPixels ;
        _useResInd          = PageParams::useResInd         ;
    }

    bool IsModified() const 
    { 
        return _lastSaved != _items.size();
    }

    void SetModifiedState(int resInd, int pageWidth, bool useResInd) // so that IsModified will return true
    {
        if(resInd != _resolutionIndex || pageWidth != _pageWidthInPixels || useResInd != _useResInd)
            _lastSaved = _items.size() - 1;
    }
    bool CanUndo() const { return _items.size() > _readCount; } // only undo until last element read
    bool CanRedo() const { return _redoList.size(); }
    void ClearUndo() { _readCount = _items.size(); }

    HistoryItem* LastScribble() const;
    HistoryItem* operator[](int index);   // index: absolute index in _items
    HistoryItem* operator[](int index) const { return _items[index]; }  // index: physical index in _items
    HistoryItem* operator[](DrawableItemIndex dri);   // index: absolute index
    HistoryItem* operator[](DrawableItemIndex dri) const { return _items[dri.index]; }  // index: physical index in _items

    QPointF BottomRightLimit(QSize screenSize);      // returns bottom right coordinate of last visible item

//--------------------- Add Items ------------------------------------------
    //int AddExistingDrawableItem(DrawableItem* pdrh);  // only add drawable Items here
    HistoryItem* AddClearRoll();
    HistoryItem* AddClearVisibleScreen();
    HistoryItem* AddClearDown();
    HistoryItem* AddCopiedItems(QPointF topLeft, Sprite *pSprite);    // using 'this' and either '_copiedList'  or  pSprite->... lists
    HistoryItem* AddDeleteItems(Sprite* pSprite = nullptr);                  // using 'this' and _driSelectedDrawables a
    HistoryItem* AddDrawableItem(DrawableItem& dri);
    HistoryItem* AddEraserItem(DrawableItem& dri);  // add to all scribbles which it intersects
    HistoryItem* AddInsertVertSpace(int y, int heightInPixels);       // height < 0: delete space
    HistoryItem* AddMoveItems(QPointF displacement);
    HistoryItem* AddPenColorChange(const DrawColors& drwclr);
    HistoryItem* AddPenWidthChange(int increment);  // for all selected drawables increment can be negative
    HistoryItem* AddRecolor(FalconPenKind pk);
    HistoryItem* AddRotationItem(MyRotation rot);
    HistoryItem* AddRemoveSpaceItem(QRectF &rect);
    HistoryItem* AddScreenShotTransparencyToLoadedItems(QColor trColor, qreal fuzzyness);
    // --------------------- drawing -----------------------------------
    bool MoveItems(QPointF displacement, const DrawableIndexVector& driv);  // returns false when move is not possible
    void Rotate(HistoryItem *forItem, MyRotation withRotation); // using _selectedRect
    void Rotate(int drawableIndex, MyRotation withRotation, QPointF center);
    void InserVertSpace(int y, int heightInPixels);

    HistoryItem* Undo();        // returns top item after undo or nullptr
    HistoryItem* Redo();

    int GetDrawablesInside(QRectF rect, IntVector& hv);

    void AddToSelection(int drawableIndex, bool clearSelections = false);
    QRectF SelectDrawablesUnder(QPointF& p, bool addToPrevious);      // selects clicked (if any) into _driSelectedDrawables, and clears right and left items list
    int /*DrawableItemIndex*/ SelectTopmostImageUnder(QPointF p);
    int CollectDrawablesInside(QRectF rect);
    void CopySelected(Sprite *forThisSprite = nullptr);      // copies selected scribbles into array. origin will be relative to (0,0)
                                                             // do the same with images
    void SetSelectionRect(QRectF& rect)
    {
        _selectionRect = rect;
    }
    QRectF SelectionRect() const { return _selectionRect; }

    void CollectPasted(const QRectF &rect);   // if items pasted copies their drawable indices into '_driSelectedDrawables'
    void CollectDeleted(HistoryDeleteItems* phd);   // used when a sprite paste is undone

    const QRectF BoundingRect() const { return _selectionRect; }
};

            //--------------------------------------------
            //      HistoryList - for each tab a different history
            //--------------------------------------------

class HistoryList : public std::vector<History*>
{
    friend class History;
    QClipboard* _pClipBoard = nullptr;
    QUuid _copyGUID;
    DrawableList _copiedItems;          // from actual History's driSelectedDrawables 
    QRectF _copiedRect;                 // bounding rectangle for copied items used for paste operation
    int _actualHistoryIndex = -1;

public:
    HistoryList() {}

    void SetupClipBoard()   // setup before use! needed, because _historyList is created before QApplication
    { 
        _pClipBoard = QApplication::clipboard(); 
    }
        // called to initialize pointers to  data in 'DrawArea'
    const DrawableList& CopiedItems() const { return _copiedItems;  }
    QRectF const CopiedRect() const { return _copiedRect; }
    History*& operator[](int index)     // will throw if index < 0 && _actualHistoryIndex < 0
    {
        if (index < 0)
            return std::vector<History*>::operator[](_actualHistoryIndex);
		else
		{
			_actualHistoryIndex = index;
			return std::vector<History*>::operator[](_actualHistoryIndex);
		}
    }

    void CopyToClipboard();    // uses _pCopiedItems and _pCopiedImages so these must be setup first
    void GetFromClipboard(); // only if the clipboard data is not the same as data in memory gets data from clipboard
};

QBitmap MyCreateMaskFromColor(QPixmap& pixmap, QColor color, qreal fuzzyness = 0.0, Qt::MaskMode mode = Qt::MaskInColor);

extern HistoryList historyList;

#endif