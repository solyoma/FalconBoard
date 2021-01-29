#pragma once
#ifndef _HISTORY_H
#define _HISTORY_H


#include <algorithm>

#include <QtGui>
#include <QFile>
#include <QDataStream>
#include <QIODevice>

#include "common.h"

constexpr int DRAWABLE_ZORDER_BASE = 10000000;  // zOrder for all images is below this number

enum HistEvent {
    heNone,
        // these are types saved on disk
    heScribble,        // series of points from start to finish of scribble
    heEraser,          // eraser used
    heEllipse,         // ellipse as a series of points
        // these are not saved
    heRecolor,         // save old color, set new color
    heScreenShot,
    heItemsDeleted,         // store the list of items deleted in this event
    heSpaceDeleted,         // empty space is deleted
    heVertSpace,            // insert vertical space
    heClearCanvas,          // visible image erased
    heBackgroundLoaded,     // an image loaded into _background
    heBackgroundUnloaded,   // image unloaded from background
    heItemsPastedBottom,// list of draw events added first drawn item is at index 'scribbleItem'
    heItemsPastedTop    // list of draw events added first drawn item is at index 'scribbleItem'
                        // last one is at index 'lastScribbleIndex'
                        // Undo: set _indexLastScribbleItem to that given in previous history item
                        // Redo: set _indexLastScribbleItem to 'lastScribbleIndex'
                };
enum MyRotation { rotNone, 
            rotR90, rotL90, rot180, rotFlipH, rotFlipV,     // these leave the top left corner in place
            rotAlpha                                        // alpha: around the center of the bounding box, increasescounterclockwise
                };
enum MyLayer { 
                mlyBackgroundImage,         // lowest layer: background image
                mlyScreenShot,              // screenshot layer: may write over it
                mlyScribbleItem,               // scribbles are drawn here
                mlySprite                   // layer for sprites (moveable images)
             };

/*========================================================
 * Structure to hold item indices for one band or selection
 *  An ItemIndex may hold scribbles or screenshot images
 *  Screenshot items have zorder-s below DRAWABLE_ZORDER_BASE
 *-------------------------------------------------------*/
struct ItemIndex
{
    int zorder;                 // indices are ordered in ascending zorder
                                // if < DRAWABLE_ZORDER_BASE then image
    int index;                  // in pHist->_items
    bool operator<(const ItemIndex& other) { return zorder < other.zorder; }
};
using ItemIndexVector = QVector<ItemIndex>;  // ordered by 'zorder'

using IntVector = QVector<int>;


// stores the coordinates of line strokes from pen down to pen up:
struct ScribbleItem        // drawn on layer mltScribbleItem
{                   
    HistEvent type = heNone;
    int zOrder = 0;
    bool isVisible = true;

    MyPenKind penKind = penBlack;
    int penWidth =1;
    QPolygon points;         // coordinates are relative to logical origin (0,0) => canvas coord = points[i] - origin
    MyRotation rot = rotNone;       // used only when rotation item added
    float rAlpha = 0;               // for 'rotAlpha': rotation around  center of bounding box
    QRect bndRect;                  // top left-bttom right coordinates of bounding rectangle
                                    // not saved on disk, recreated on read

    ScribbleItem(HistEvent he = heNone, int zorder = 0) noexcept;       // default constructor
    ScribbleItem(const ScribbleItem& di);
    ScribbleItem(const ScribbleItem&& di) noexcept;
    ScribbleItem& operator=(const ScribbleItem& di);

    ScribbleItem& operator=(const ScribbleItem&& di)  noexcept;

    void clear();       // clears points and sets type to heNone

    static bool IsExtension(const QPoint& p, const QPoint& p1, const QPoint& p2 = QPoint()); // vectors p->p1 and p1->p are parallel?

    void add(QPoint p);          // must use SetBoundingRectangle after all points adedde
    void add(int x, int y);      // - " - 
    void Smooth();               // points

    void SetBoundingRectangle(); // use after all points added
    bool intersects(const QRect& arect) const;

    void Translate(QPoint dr, int minY);    // only if not deleted and top is > minY
    void Rotate(MyRotation rot, QRect inThisrectangle, float alpha=0.0);    // alpha used only for 'rotAlpha'
};

inline QDataStream& operator<<(QDataStream& ofs, const ScribbleItem& di);
inline QDataStream& operator>>(QDataStream& ifs, ScribbleItem& di);


// ******************************************************
// image to shown on background
struct ScreenShotImage {       // shown on layer mlyScreenShot below the drawings
    QImage image;              // image from the disk or from screenshot
    QPoint topLeft;            // relative to logical (0,0) of 'paper roll' (widget coord: topLeft + DrawArea::_topLeft is used) 
    bool isVisible = true;
    int itemIndex;             // in history::_item
    int zOrder;     // of images is the index of this image on the pimages list in History
    QRect Area() const { return QRect(topLeft, QSize(image.width(), image.height())); }
            // canvasRect relative to paper (0,0)
            // result: relative to image
            // isNull() true when no intersection with canvasRect
    QRect AreaOnCanvas(const QRect& canvasRect) const;
    void Translate(QPoint p, int minY); // only if not deleted and top is > minY
    void Rotate(MyRotation rot, QRect encRect, float alpha=0.0);    // only used for 'rotAlpha'
};

inline QDataStream& operator<<(QDataStream& ofs, const ScreenShotImage& bimg);
inline QDataStream& operator>>(QDataStream& ifs, ScreenShotImage& bimg);

class  ScreenShotImageList : public  QList<ScreenShotImage>
{
    int _index = -1;
    QRect _canvasRect;
public:
    void Add(QImage& image, QPoint pt, int zorder);
    // canvasRect and result are relative to (0,0)
    QRect AreaOnCanvas(int index, const QRect& canvasRect) const;
    ScreenShotImage* ScreenShotAt(int index);
    ScreenShotImage* FirstVisible(const QRect& canvasRect);
    ScreenShotImage* NextVisible();
    void Translate(int which, QPoint p, int minY);  // only if not deleted and top is > minY
    void Rotate(int which, MyRotation rot, QRect encRect, float alpha=0.0);
    void Clear();
    int ImageIndexFor(QPoint &p) const; // -1: no such image else index in 'pImages'
};

//*********************************************************
using ScribbleItemVector = QVector<ScribbleItem>;

/*========================================================
 * One history element
 *-------------------------------------------------------*/

struct History; // forward

struct HistoryItem      // base class
{
    History* pHist;
    HistEvent type;

    HistoryItem(History* pHist, HistEvent typ=heNone) : pHist(pHist), type(typ) {}
    virtual ~HistoryItem() {}

    virtual int ZOrder() const { return 0; }

    virtual QPoint TopLeft() const { return QPoint(); }
    virtual ScribbleItem* GetVisibleScribble(int index = 0) const { return nullptr; } // returns pointer to the index-th ScribbleItem
    virtual ScribbleItem* GetScribble(int index = 0) const { return nullptr; } // returns pointer to the index-th ScribbleItem even when it is not visible
    virtual ScreenShotImage* GetScreenShotImage() const { return nullptr; }
    virtual ItemIndex ItxFrom(int index) const { ItemIndex itx; itx.index = index; itx.zorder = ZOrder(); return itx; }

    virtual bool Translatable() const { return false;  }
    virtual void Translate(QPoint p, int minY) { } // translates if top is >= minY
    virtual void Rotate(MyRotation rot, QRect encRect, float alpha=0.0) { ; }      // rotation or flip
    virtual int Size() const { return 0; }         // size of stored scribbles or erases
    virtual void SetVisibility(bool visible) { }

    // function prototypes for easier access
    virtual int RedoCount() const { return 1; } // how many elements from _redo must be moved back to _items
    virtual int Undo() { return 1; }        // returns amount _actItem in History need to be changed (go below 1 item)
    virtual int Redo() { return 0; }        // returns amount _actItem in History need to be changed (after ++_actItem still add this to it)
    virtual QRect Area() const { return QRect(); }  // encompassing rectangle for all points
    virtual bool IsScribble() const { return false; }
    virtual bool IsImage() const { return type == heScreenShot; }
    virtual bool Hidden() const { return true; }    // must not draw it
    virtual bool IsSaveable() const  { return !Hidden(); }    // use when items may be hidden

    virtual bool operator<(const HistoryItem& other);
};

//--------------------------------------------
// type heScribble, heEraser
struct HistoryScribbleItem : public HistoryItem
{
    ScribbleItem scribbleItem;                // store data into this
    HistoryScribbleItem(History* pHist, ScribbleItem& dri);
    HistoryScribbleItem(const HistoryScribbleItem& other);
    HistoryScribbleItem(const HistoryScribbleItem&& other) noexcept;

    void SetVisibility(bool visible) override;
    bool Hidden() const override;

    HistoryScribbleItem& operator=(const HistoryScribbleItem& other);
    HistoryScribbleItem& operator=(const HistoryScribbleItem&& other) noexcept;

    int ZOrder() const override;
    QPoint TopLeft() const override { return scribbleItem.bndRect.topLeft(); }
    ScribbleItem* GetVisibleScribble(int index = 0) const override;
    ScribbleItem* GetScribble(int index = 0) const override;
    QRect Area() const override;
    int Size() const override { return 1; }
    bool Translatable() const override { return scribbleItem.isVisible; }
    void Translate(QPoint p, int minY) override; // only if not deleted and top is > minY
    void Rotate(MyRotation rot, QRect encRect, float alpha=0.0) override;
    bool IsScribble() const override { return true; }
    // no new undo/redo needed
};

//--------------------------------------------
struct HistoryDeleteItems : public HistoryItem
{
    ItemIndexVector deletedList;   // absolute indexes of scribble elements in '*pHist'
    int Undo() override;
    int Redo() override;
    HistoryDeleteItems(History* pHist, ItemIndexVector& selected);
    HistoryDeleteItems(HistoryDeleteItems& other);
    HistoryDeleteItems& operator=(const HistoryDeleteItems& other);
    HistoryDeleteItems(HistoryDeleteItems&& other) noexcept;
    HistoryDeleteItems& operator=(const HistoryDeleteItems&& other) noexcept;
    ScribbleItem* GetScribble(int index = 0) const override { return nullptr; }
    ScribbleItem* GetVisibleScribble(int index = 0) const override { return nullptr; }
};
//--------------------------------------------
// remove an empty rectangular region by moving
// elements at the right to left, elements at the bottom up
struct HistoryRemoveSpaceItem : HistoryItem // using _selectedRect
{
    ItemIndexVector modifiedList; // elements to be moved horizontally (elements on the right)
                            // empty for vertical shift by delta
    int delta;              // translate with this amount. if 0?
    int y;                  // topmost coordinate to remove space from

    int Undo() override;
    int Redo() override;
    HistoryRemoveSpaceItem(History* pHist, ItemIndexVector&toModify, int y, int distance);
    HistoryRemoveSpaceItem(HistoryRemoveSpaceItem& other);
    HistoryRemoveSpaceItem& operator=(const HistoryRemoveSpaceItem& other);
    HistoryRemoveSpaceItem(HistoryRemoveSpaceItem&& other) noexcept;
    HistoryRemoveSpaceItem& operator=(const HistoryRemoveSpaceItem&& other) noexcept;
};

//--------------------------------------------
// When pasting items into _items from bottom up 
//   'HistoryPasteItemBottom' item (its absolute index is in  
//          HistoryPasteItemTop::indexOfBottomItem
//   'HistoryScribbleItems' items athat were pasted
//   'HistoryPasteItemTop' item
struct HistoryPasteItemBottom : HistoryItem
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

struct HistoryPasteItemTop : HistoryItem
{
    int indexOfBottomItem;      // abs. index of 'HistoryPastItemBottom' item
    int count;                  // of items pasted  
                                // these items came in one group in *pHist's '_items'
    int moved = 0;              // 0: copy / paste, 1: moved with mouse / pen
                                //  in both bottom and top items
    QRect boundingRect;

    HistoryPasteItemTop(History* pHist, int index, int count, QRect& rect);

    HistoryPasteItemTop(HistoryPasteItemTop& other);
    HistoryPasteItemTop& operator=(const HistoryPasteItemTop& other);
    HistoryPasteItemTop(HistoryPasteItemTop&& other) noexcept;
    HistoryPasteItemTop& operator=(const HistoryPasteItemTop&& other) noexcept;

    int Size() const override { return count; }

    ScribbleItem* GetScribble(int index = 0) const override; // only for top get (count - index)-th below this one
    ScribbleItem* GetVisibleScribble(int index = 0) const override; // only for top get (count - index)-th below this one

    bool Hidden() const override;       // only for top: when the first element is hidden, all hidden
    // bool IsSaveable() const override;
    void SetVisibility(bool visible) override; // for all elements 
    bool Translatable() const override { return !Hidden(); }
    void Translate(QPoint p, int minY) override;    // only if not deleted and top is > minY
    void Rotate(MyRotation rot, QRect encRect, float alpha=0.0) override;

    QPoint TopLeft() const override { return boundingRect.topLeft(); }
    QRect Area() const override;

    int Undo() override;    // only for top item: hide 'count' items below this one
    int Redo() override;    // only for top item: reveal count items
};
//--------------------------------------------
struct HistoryReColorItem : HistoryItem
{
    ItemIndexVector selectedList;                 // indexes  to elements in '*pHist'
    QVector<MyPenKind> penKindList;         // colors for elements in selectedList
    MyPenKind pk;                           
    QRect boundingRectangle;            // to scroll here when undo/redo

    int Undo() override;
    int Redo() override;
    HistoryReColorItem(History* pHist, ItemIndexVector&selectedList, MyPenKind pk);
    HistoryReColorItem(HistoryReColorItem& other);
    HistoryReColorItem& operator=(const HistoryReColorItem& other);
    HistoryReColorItem(HistoryReColorItem&& other) noexcept;
    HistoryReColorItem& operator=(const HistoryReColorItem&& other) noexcept;
    QRect Area() const override;
};

//--------------------------------------------
struct HistoryInsertVertSpace : HistoryItem
{
    int y = 0, heightInPixels = 0;

    HistoryInsertVertSpace(History* pHist, int top, int pixelChange);
    HistoryInsertVertSpace(const HistoryInsertVertSpace& other);
    HistoryInsertVertSpace& operator=(const HistoryInsertVertSpace& other);

    int Undo() override;
    int Redo() override;
    QRect Area() const override;
};

//--------------------------------------------
struct HistoryScreenShotItem : public HistoryItem
{
    int which;      // index in History's screen shot image list
                    // originally zOrder of image equals to this
                    // when images order change (TODO) this may not be true
                    // must be smaller than DRAWABLE_ZORDER_BASE and no check is made!
    HistoryScreenShotItem(History* pHist, int which);
    HistoryScreenShotItem(const HistoryScreenShotItem &other);
    HistoryScreenShotItem& operator=(const HistoryScreenShotItem& other);
    ~HistoryScreenShotItem();
    int Undo() override;
    int Redo() override;
    int ZOrder() const override;
    QPoint TopLeft() const override;
    QRect Area() const override;
    bool Hidden() const override;       // when the first element is hidden, all hidden
    void SetVisibility(bool visible) override; // for all elements in list
    bool Translatable() const override;
    void Translate(QPoint p, int minY) override; // only if not deleted and top is > minY
    void Rotate(MyRotation rot, QRect encRect, float alpha=0.0) override;
    bool IsScribble() const override { return true; }
    ScreenShotImage* GetScreenShotImage() const override;
};
//--------------------------------------------
struct HistoryRotationItem : HistoryItem
{
    MyRotation rot;
    bool flipH = false;
    bool flipV = false;
    float rAlpha = 0.0;
    ItemIndexVector nSelectedItemList;
    QRect encRect;         // encompassing rectangle: all items inside

    HistoryRotationItem(History* pHist, MyRotation rotation, QRect rect, ItemIndexVector selList, float alpha=0.0);
    HistoryRotationItem(const HistoryRotationItem& other);
    HistoryRotationItem& operator=(const HistoryRotationItem& other);
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

    History* pHist;
    QPoint topLeft;     // top,left: position of sprite rel. to top left of visible area of canvas
    QPoint dp;          // relative to top left of sprite: cursor coordinates when the sprite is selected
    QImage image;       // image of the sprite to paint over the canvas
    QRect rect;         // selection rectangle 0,0, width,height
    bool itemsDeleted = true;   // unless Alt+

    ItemIndexVector       nSelectedItemsList;     // indices into 'pHist->_items', that are completely inside the rubber band rectangle
    ScribbleItemVector       items;      // each point of items[..] is relative to top-left of sprite (0,0)
    ScreenShotImageList   images;     // image top left is relative to top left of sprite (0,0)
};


using HistoryItemVector = QVector<HistoryItem*>;
/*========================================================
 * bands on screen
 *  all items are categorized into bands of height of 
 *  _screenHeight, each band contains indices of each 
 *  element which intersects that band, so one element may
 *  appear on more than one band (currently 2)
 *  a band may contain images and scribbles both
 * 
 * Band for any y coord is y / _bandHeight
 *-------------------------------------------------------*/
struct Bands 
{
    Bands() {}
    void SetParam(History* pHist, int bandHeight );
    void Add(int ix);    // existing and visible drawnable in pHist->_items[ix]
    void Remove(int ix);
    int  ItemsStartingInBand(int bandIndex, ItemIndexVector &iv);
    void ItemsVisibleForArea(QRect &rect, ItemIndexVector & hv, bool onlyInsider=false);
    void SelectItemsForArea(QRect &rect, ItemIndexVector& hvLeft, ItemIndexVector& hvInside, ItemIndexVector& hvRight, QRect &unionRect);
    void Clear() { _bands.clear(); }
    int constexpr BandHeight() const { return _bandHeight; }
    bool IsEmpty() const { return _bands.isEmpty(); }
    int Size() const { return _bands.size(); }
    int ItemCount() const
    {
        //int cnt = 0;
        //for (int ib = 0; ib < _bands.size(); ++ib)
        //    cnt += _bands[ib].indices.size();
        //return cnt;
        return _itemCount;
    }
    int IndexForItemNumber(int num) // returns -1 if no such index
    {
        if (num < 0)
            return num;

        static int
            _ib = 0,
            _iprev = -2,        // so that _prev+1 != 0
            _ibase = 0;
                    
        if (num != _iprev + 1)         // then must go through _bands
        {
            for (_ib = 0; _ib < _bands.size(); ++_ib)
            {
                int isize = _bands[_ib].indices.size();
                if (num < isize)    // found the band
                {
                    _iprev = num;
                    return _bands[_ib].indices[num].index;
                }
                num -= isize;
                _ibase += isize;
            }
            return -1;  // no such item
        }
        else                   // next item
        {
            if (++_iprev == _bands[_ib].indices.size())  // end of this band
            {
                _ibase += _bands[_ib].indices.size();
                if (++_ib == _bands.size())
                    return -1;
            }
            return _bands[_ib].indices[num - _ibase].index;
        }
        return -1;
    }
private:
    struct Band
    {

        int band_index;           // needed because _bands is not contiguous  
        int top;                  // y coordinate of topmost item in band
        ItemIndexVector indices;  // z-ordered indices in _items for visible items
        bool operator<(const Band &other) const { return band_index < other.band_index;  }
        bool operator<(int n) const { return band_index < n;  };

        void Remove(int index);
    };

    History* _pHist=nullptr;
    int _bandHeight=-1;
    QPoint _visibleBands;   // x() top, y() bottom, any of them -1: not used
    QList<Band> _bands;     // Band-s ordered by ascending band index
    int _itemCount = 0;    // count of all individual items in all bands

    int _ItemStartsInBand(int band_index, HistoryItem *phi);
    int _AddBandFor(int y);  // if present returns existing, else inserts new band, or appends when above all bands
    int _GetBand(int y);     // if present returns existing, else return -1
    int _FindBandFor(int y) const;  // binary search: returns index of  the band which have y inside
    void _ItemsForBand(int bandIndex, QRect& rect, ItemIndexVector& hv, bool insidersOnly);
    void _SelectItemsFromBand(int bandIndex, QRect &rect, ItemIndexVector& hvLeft, ItemIndexVector& hvInside, ItemIndexVector& hvRight, QRect& unionRect);
    void _Insert(Band &band, int index); // insert into indices at correct position
};

/*========================================================
 * Class for storing history of editing
 *  contains data for items drawn on screen, 
 *      item deleteion, canvas movement, etc
 *
 *     _items:   generalized list of all items
 *          members are accessed by either added order
 *              or ordered by first y then x coordinates of
 *              their top left rectangle
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
    friend class Bands;

    bool _inLoad = false;               // in function Load() / needed for correct z- order settings
    HistoryItemVector _items,           // items in the order added. Items need not be scribble.
                                        // scribble elements on this list may be either visible or hidden
                      _redo;            // from _items for redo. Items need not be scribble.
                                        // scribble elements on this list may be either visible or hidden
    IntVector   _yxOrder;               // indices of all scribbles in '_items' ordered by y then x    
    Bands _bands;                       // used to cathegorize visible _items
    int _readCount = 0;                 // undo works until this index is reached
                                        // unscribble items have no indices in here
    friend class _yitems;
    QRect _clpRect;                     // clipping rectangle for selecting points to draw
                                        // before searching operations set this when it is changed
    QStack<QRect> _savedClps;           // clipRect saves

    int _indexOfFirstVisible = -1;      // in _yxorder

    int _lastZorder = DRAWABLE_ZORDER_BASE;  // increased when scribble elements adedd to _items, does not get decreasd when they are taken off!
    int _nextImageZorder = 0;                 // z-order of images

    ScribbleItemVector     _copiedItems;   // copy items on _nSelectedList into this list for pasting anywhere even in newly opened documents
    ScreenShotImageList _copiedImages;  // copy images on _nSelectedList to this
    QRect _copiedRect;                 // bounding rectangle for copied items used for paste operation

    ItemIndexVector _nSelectedItemsList,      // indices into '_items', that are completely inside the rubber band (includes screenshots - zorder < DRAWABLE_ZORDER_BASE)
                    _nItemsRightOfList,       // -"- for elements that were at the right of the rubber band
                    _nItemsLeftOfList;        // -"- for elements that were at the left of the rubber band
    int _nIndexOfFirstScreenShot;       // index of the first scribble (not image) element that is below a given y (always calulate)
    QRect _selectionRect;              // bounding rectangle for selected items OR rectangle in which there are no items
                                        // when _nSelectedItemList is empty
    bool _modified = false;

    HistoryItem* _AddItem(HistoryItem* p);

    bool _IsSaveable(int i);

    int _YIndexForXY(QPoint topLeft);        // index of top-left-most element below xy (y larger or equal to xy.y) 
                                        // in _items, using '_yOrder' (binary search)
//    int _YIndexWhichInside();// index of top most element whose area includes point xy
    QPoint _XYForIndex(int index)       // y coord of (physical) index-th element no check if thisa is valid!
    {
        return _items[index]->TopLeft();
    }
    int _YIndexForIndex(int index);
    void _push_back(HistoryItem* pi);

    HistoryItem* _yitems(int yindex) const
    { 
        if(yindex < 0 || yindex >= _yxOrder.size() )
            return nullptr;
        return _items[_yxOrder[yindex]]; 
    }
    void _SaveClippingRect();
    void _RestoreClippingRect();

public:
    ScreenShotImageList* pImages = nullptr;  // set in DrawArea constructor

    History() noexcept { _bands.SetParam(this, -1);  }
    History(const History& o);
    History(const History&& o) noexcept;
    ~History();

    void SetBandHeight(int h) { _bands.SetParam(this, h); }
    void SetClippingRect(const QRect& rect) { _clpRect = rect;  }
    void SetVisibility(int index, bool visible)
    {
        if (_items[index]->Hidden() == !visible)    // when it does not change
            return;

        _items[index]->SetVisibility(visible);
        if (visible)
            _bands.Add(index);
        else
            _bands.Remove(index); 
    }

    void clear();
    int size() const;   // _items's size
    int CountOfVisible() const; // visible from yxOrder
    int CountOfScribble() const { return _yxOrder.size(); }
    int SelectedSize() const { return _nSelectedItemsList.size(); }

    HistoryItem* operator[](int index);   // index: absolute index

    const qint32 MAGIC_ID = 0x53414d57; // "SAMW" - little endian !! MODIFY this and Save() for big endian processors!
    const qint32 MAGIC_VERSION = 0x56010000; // V 01.00.00

    bool Save(QString name);

    int Load(QString name);         // returns _ites.size() when Ok, -items.size()-1 when read error
    bool IsModified() const { return _modified & CanUndo(); }
    bool CanUndo() const { return _items.size() > _readCount; } // only undo until last element read
    bool CanRedo() const { return _redo.size(); }
    void ClearUndo() { _readCount = _items.size(); }

    HistoryItem* operator[](int index) const { return _items[index]; }                  // index: physical index in _items
    HistoryItem* atYIndex(int yindex) const        // yindex: index in _yxOrder - only scribbles
    {
        return _yitems(yindex);
    }
    int IndexForYIndex(int yix);
    void ReplaceItem(int index, HistoryItem* pi);     // index: in '_items'
    void RemoveItemsStartingAt(int index);

    QPoint BottomRightVisible(QSize screenSize) const;      // returns bottom right coordinate of last visible item

//--------------------- Add Items ------------------------------------------
    HistoryItem* addClearRoll();
    HistoryItem* addClearVisibleScreen();
    HistoryItem* addClearDown();
    HistoryItem* addScribbleItem(ScribbleItem& dri);
    HistoryItem* addDeleteItems(Sprite* pSprite = nullptr);                  // using 'this' and _nSelectedItemsList a
    HistoryItem* addPastedItems(QPoint topLeft, Sprite *pSprite=nullptr);   // using 'this' and either '_copiedList'  or  pSprite->... lists
    HistoryItem* addRecolor(MyPenKind pk);
    HistoryItem* addInsertVertSpace(int y, int heightInPixels);
    HistoryItem* addScreenShot(int index);
    HistoryItem* addRotationItem(MyRotation rot);
    HistoryItem* addRemoveSpaceItem(QRect &rect);
    // --------------------- drawing -----------------------------------
    void TranslateAllItemsBelow(QPoint delta, int belowY);
    void Rotate(HistoryItem *forItem, MyRotation withRotation); // using _selectedRect
    void InserVertSpace(int y, int heightInPixels);

    QRect       Undo();        // returns top left after undo 
    HistoryItem* Redo();

    int History::GetScribblesInside(QRect rect, HistoryItemVector& hv);
    int ImageIndexFor(QPoint& p) const { return pImages->ImageIndexFor(p); } // -1: no such image else index in 'pImages'

    void AddToSelection(int index=-1);
    int CollectItemsInside(QRect rect);
    int SelectTopmostImageFor(QPoint& p);
    void CopySelected(Sprite *forThisSprite = nullptr);      // copies selected scribbles into array. origin will be relative to (0,0)
                                                             // do the same with images

    const QVector<ScribbleItem>& CopiedItems() const { return _copiedItems;  }
    int CopiedCount() const { return _copiedItems.size();  }
    const QRect BoundingRect() const { return _selectionRect; }
    const ItemIndexVector& Selected() const { return _nSelectedItemsList;  }
};

#endif