#pragma once
#ifndef _HISTORY_H
#define _HISTORY_H


#include <algorithm>

#include <QtGui>
#include <QFile>
#include <QDataStream>
#include <QIODevice>

#include "common.h"

constexpr int DRAWABLE_ZORDER_BASE = 10000000;

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
    heItemsPastedBottom,// list of draw events added first drawn item is at index 'drawnItem'
    heItemsPastedTop    // list of draw events added first drawn item is at index 'drawnItem'
                        // last one is at index 'lastDrawnIndex'
                        // Undo: set _indexLastDrawnItem to that given in previous history item
                        // Redo: set _indexLastDrawnItem to 'lastDrawnIndex'
                };
enum MyRotation { rotNone, 
            rotR90, rotL90, rot180, rotFlipH, rotFlipV,     // these leave the top left corner in place
            rotAlpha                                        // alpha: around the center of the bounding box, increasescounterclockwise
                };
enum MyLayer { 
                mlyBackgroundImage,         // lowest layer: background image
                mlyScreenShot,              // screenshot layer: may write over it
                mlyDrawnItem,               // drawables are drawn here
                mlySprite                   // layer for sprites (moveable images)
             };

// stores the coordinates of line strokes from pen down to pen up:
struct DrawnItem        // drawn on layer mltDrawnItem
{                   
    HistEvent type = heNone;
    int zOrder = 0;
    bool isVisible = true;

    MyPenKind penKind = penBlack;
    int penWidth =1;
    QPolygonF points;         // coordinates are relative to logical origin (0,0) => canvas coord = points[i] - origin
    MyRotation rot = rotNone;       // used only when rotation item added
    float rAlpha = 0;               // for 'rotAlpha': rotation around  center of bounding box
    QRectF bndRect;                  // top left-bttom right coordinates of bounding rectangle
                                    // not saved on disk, recreated on read

    DrawnItem(HistEvent he = heNone, int zorder = 0) noexcept;       // default constructor
    DrawnItem(const DrawnItem& di);
    DrawnItem(const DrawnItem&& di);
    DrawnItem& operator=(const DrawnItem& di);

    DrawnItem& operator=(const DrawnItem&& di)  noexcept;

    void clear();       // clears points and sets type to heNone

    static bool IsExtension(const QPointF& p, const QPointF& p1, const QPointF& p2 = QPointF()); // vectors p->p1 and p1->p are parallel?

    void add(QPointF p);          // must use SetBoundingRectangle after all points adedde
    void add(int x, int y);      // - " - 
    void Smooth();               // points

    void SetBoundingRectangle(); // use after all points added
    bool intersects(const QRectF& arect) const;

    void Translate(QPointF dr, int minY);    // only if not deleted and top is above minY
    void Rotate(MyRotation rot, QRectF inThisrectangle, float alpha=0.0);    // alpha used only for 'rotAlpha'
};

inline QDataStream& operator<<(QDataStream& ofs, const DrawnItem& di);
inline QDataStream& operator>>(QDataStream& ifs, DrawnItem& di);


// ******************************************************
// image to shown on background
struct ScreenShotImage {           // shown on layer mlyScreenShot below the drawings
    QImage image;              // image from the disk or from screenshot
    QPointF topLeft;            // relative to (0,0) of 'paper roll' (widget coord: topLeft + DrawArea::_topLeft is used) 
    bool isVisible = true;
    int zOrder;     // of images is the index of this image on the pimages list in History
            // canvasRect relative to (0,0)
            // result: relative to image
            // isNull() true when no intersection
    QRectF Area(const QRectF& canvasRect) const;
    void Translate(QPointF p, int minY);
    void Rotate(MyRotation rot, QRectF encRect, float alpha=0.0);    // only used for 'rotAlpha'
};

inline QDataStream& operator<<(QDataStream& ofs, const ScreenShotImage& bimg);
inline QDataStream& operator>>(QDataStream& ifs, ScreenShotImage& bimg);

class  ScreenShotImageList : public  QList<ScreenShotImage>
{
    int _index = -1;
    QRectF _canvasRect;
public:
    void Add(QImage& image, QPointF pt, int zorder);
    // canvasRect and result are relative to (0,0)
    QRectF Area(int index, const QRectF& canvasRect) const;
    ScreenShotImage* ScreenShotAt(int index);
    ScreenShotImage* FirstVisible(const QRectF& canvasRect);
    ScreenShotImage* NextVisible();
    void Translate(int which, QPointF p, int minY);
    void Rotate(int which, MyRotation rot, QRectF encRect, float alpha=0.0);
    void Clear();
};

//*********************************************************
using DrawnItemVector = QVector<DrawnItem>;

/*========================================================
 * One history element
 *-------------------------------------------------------*/

using IntVector = QVector<int>;

struct History; // forward

struct HistoryItem      // base class
{
    History* pHist;
    HistEvent type;

    HistoryItem(History* pHist, HistEvent typ=heNone) : pHist(pHist), type(typ) {}
    virtual ~HistoryItem() {}

    virtual int ZOrder() const { return 0; }
    virtual QPointF TopLeft() const { return QPointF(); }
    virtual DrawnItem* GetVisibleDrawable(int index = 0) const { return nullptr; } // returns pointer to the index-th DrawnItem
    virtual DrawnItem* GetDrawable(int index = 0) const { return nullptr; } // returns pointer to the index-th DrawnItem even when it is not visible
    virtual ScreenShotImage* GetScreenShotImage() const { return nullptr; }

    virtual bool Translatable() const { return false;  }
    virtual void Translate(QPointF p, int minY) { } // translates if top is >= minY
    virtual void Rotate(MyRotation rot, QRectF encRect, float alpha=0.0) { ; }      // rotation or flip
    virtual int Size() const { return 0; }         // size of stored scribbles or erases
    virtual void SetVisibility(bool visible) { }

    // function prototypes for easier access
    virtual int RedoCount() const { return 1; } // how many elements from _redo must be moved back to _items
    virtual int Undo() { return 1; }        // returns amount _actItem in History need to be changed (go below 1 item)
    virtual int Redo() { return 0; }        // returns amount _actItem in History need to be changed (after ++_actItem still add this to it)
    virtual QRectF Area() const { return QRectF(); }  // encompassing rectangle for all points
    virtual bool IsDrawable() const { return false; }
    virtual bool Hidden() const { return true; }    // must not draw it
    virtual bool IsSaveable() const  { return !Hidden(); }    // use when items may be hidden

    virtual bool operator<(const HistoryItem& other);
};

//--------------------------------------------
// type heScribble, heEraser
struct HistoryDrawnItem : public HistoryItem
{
    DrawnItem drawnItem;                // store data into this
    HistoryDrawnItem(History* pHist, DrawnItem& dri);
    HistoryDrawnItem(const HistoryDrawnItem& other);
    HistoryDrawnItem(const HistoryDrawnItem&& other);

    void SetVisibility(bool visible) override;
    bool Hidden() const override;

    HistoryDrawnItem& operator=(const HistoryDrawnItem& other);
    HistoryDrawnItem& operator=(const HistoryDrawnItem&& other);

    int ZOrder() const override;
    QPointF TopLeft() const override { return drawnItem.bndRect.topLeft(); }
    DrawnItem* GetVisibleDrawable(int index = 0) const override;
    DrawnItem* GetDrawable(int index = 0) const override;
    QRectF Area() const override;
    int Size() const override { return 1; }
    bool Translatable() const override { return drawnItem.isVisible; }
    void Translate(QPointF p, int minY) override;
    void Rotate(MyRotation rot, QRectF encRect, float alpha=0.0) override;
    bool IsDrawable() const override { return true; }
    // no new undo/redo needed
};

//--------------------------------------------
struct HistoryDeleteItems : public HistoryItem
{
    IntVector deletedList;   // absolute indexes of drawable elements in '*pHist'
    int Undo() override;
    int Redo() override;
    HistoryDeleteItems(History* pHist, IntVector& selected);
    HistoryDeleteItems(HistoryDeleteItems& other);
    HistoryDeleteItems& operator=(const HistoryDeleteItems& other);
    HistoryDeleteItems(HistoryDeleteItems&& other);
    HistoryDeleteItems& operator=(const HistoryDeleteItems&& other);
    DrawnItem* GetDrawable(int index = 0) const override { return nullptr; }
    DrawnItem* GetVisibleDrawable(int index = 0) const override { return nullptr; }
};
//--------------------------------------------
// remove an empty rectangular region by moving
// elements at the right to left, elements at the bottom up
struct HistoryRemoveSpaceitem : HistoryItem // using _selectedRect
{
    IntVector modifiedList; // elements to be moved horizontally (elements on the right)
                            // empty for vertical shift by delta
    int first;              // index of first element just below the selection rectangle
    int delta;              // translate with this amount. if 0?

    int Undo() override;
    int Redo() override;
    HistoryRemoveSpaceitem(History* pHist, IntVector &toModify, int first, int distance);
    HistoryRemoveSpaceitem(HistoryRemoveSpaceitem& other);
    HistoryRemoveSpaceitem& operator=(const HistoryRemoveSpaceitem& other);
    HistoryRemoveSpaceitem(HistoryRemoveSpaceitem&& other);
    HistoryRemoveSpaceitem& operator=(const HistoryRemoveSpaceitem&& other);
};

//--------------------------------------------
// When pasting items into _items from bottom up 
//   'HistoryPasteItemBottom' item (its absolute index is in  
//          HistoryPasteItemTop::indexOfBottomItem
//   'HistoryDrawnItems' items athat were pasted
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
    QRectF boundingRect;

    HistoryPasteItemTop(History* pHist, int index, int count, QRectF& rect);

    HistoryPasteItemTop(HistoryPasteItemTop& other);
    HistoryPasteItemTop& operator=(const HistoryPasteItemTop& other);
    HistoryPasteItemTop(HistoryPasteItemTop&& other);
    HistoryPasteItemTop& operator=(const HistoryPasteItemTop&& other);

    int Size() const override { return count; }

    DrawnItem* GetDrawable(int index = 0) const override; // only for top get (count - index)-th below this one
    DrawnItem* GetVisibleDrawable(int index = 0) const override; // only for top get (count - index)-th below this one

    bool Hidden() const override;       // only for top: when the first element is hidden, all hidden
    // bool IsSaveable() const override;
    void SetVisibility(bool visible) override; // for all elements 
    bool Translatable() const override { return !Hidden(); }
    void Translate(QPointF p, int minY) override;
    void Rotate(MyRotation rot, QRectF encRect, float alpha=0.0) override;

    QPointF TopLeft() const override { return boundingRect.topLeft(); }
    QRectF Area() const override;

    int Undo() override;    // only for top item: hide 'count' items below this one
    int Redo() override;    // only for top item: reveal count items
};
//--------------------------------------------
struct HistoryReColorItem : HistoryItem
{
    IntVector selectedList;                 // indexes  to elements in '*pHist'
    QVector<MyPenKind> penKindList;         // colors for elements in selectedList
    MyPenKind pk;                           
    QRectF boundingRectangle;            // to scroll here when undo/redo

    int Undo() override;
    int Redo() override;
    HistoryReColorItem(History* pHist, IntVector &selectedList, MyPenKind pk);
    HistoryReColorItem(HistoryReColorItem& other);
    HistoryReColorItem& operator=(const HistoryReColorItem& other);
    HistoryReColorItem(HistoryReColorItem&& other);
    HistoryReColorItem& operator=(const HistoryReColorItem&& other);
    QRectF Area() const override;
};

//--------------------------------------------
struct HistoryInsertVertSpace : HistoryItem
{
    int minY = 0, heightInPixels = 0;

    HistoryInsertVertSpace(History* pHist, int top, int pixelChange);
    HistoryInsertVertSpace(const HistoryInsertVertSpace& other);
    HistoryInsertVertSpace& operator=(const HistoryInsertVertSpace& other);

    int Undo() override;
    int Redo() override;
    QRectF Area() const override;
};

//--------------------------------------------
struct HistoryScreenShotItem : public HistoryItem
{
    int which;      // index in History's screen shot image list

    HistoryScreenShotItem(History* pHist, int which);
    HistoryScreenShotItem(const HistoryScreenShotItem &other);
    HistoryScreenShotItem& operator=(const HistoryScreenShotItem& other);
    ~HistoryScreenShotItem();
    int Undo() override;
    int Redo() override;
    int ZOrder() const override;
    QPointF TopLeft() const override;
    QRectF Area() const override;
    bool Hidden() const override;       // when the first element is hidden, all hidden
    void SetVisibility(bool visible) override; // for all elements in list
    bool Translatable() const override;
    void Translate(QPointF p, int minY) override;
    void Rotate(MyRotation rot, QRectF encRect, float alpha=0.0) override;
    bool IsDrawable() const override { return true; }
    ScreenShotImage* GetScreenShotImage() const override;
};
//--------------------------------------------
struct HistoryRotationItem : HistoryItem
{
    MyRotation rot;
    bool flipH = false;
    bool flipV = false;
    float rAlpha = 0.0;
    IntVector nSelectedItemList;
    QRectF encRect;         // encompassing rectangle: all items inside

    HistoryRotationItem(History* pHist, MyRotation rotation, QRectF rect, IntVector selList, float alpha=0.0);
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
    QPointF topLeft;     // top,left: position of sprite rel. to top left of visible area of canvas
    QPointF dp;          // relative to top left of sprite: cursor coordinates when the sprite is selected
    QImage image;       // image of the sprite to paint over the canvas
    QRectF rect;         // selection rectangle 0,0, width,height
    bool itemsDeleted = true;   // unless Alt+

    IntVector nSelectedItemsList;     // indices into 'pHist->_items', that are completely inside the rubber band
    DrawnItemVector       items;      // each point of items[..] is relative to top-left of sprite (0,0)
    ScreenShotImageList   images;     // image top left is relative to top left of sprite (0,0)
};

using HistoryItemVector = QVector<HistoryItem*>;

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
    bool _inLoad = false;               // in function Load() / needed for correct z- order settings
    HistoryItemVector _items,           // items in the order added. Items need not be drawable.
                                        // drawable elements on this list may be either visible or hidden
                      _redo;            // from _items for redo. Items need not be drawable.
                                        // drawable elements on this list may be either visible or hidden
    IntVector   _yxOrder;               // indices of all drawables in '_items' ordered by y then x    
                                        // undrawable items have no indices in here
    friend class _yitems;
    int _yindex = -1;                   // index in ordered item list
                                        // used for iteration
    QRect _clpRect;                     // clipping rectangle for selecting points to draw
                                        // before searching operations set this when it is changed
    QStack<QRect> _savedClps;           // clipRect saves

    int _indexOfFirstVisible = -1;      // in _yxorder

    int _lastZorder = DRAWABLE_ZORDER_BASE;  // increased when drawable elements adedd to _items, does not get decreasd when they are taken off!
    int _lastImage = 0;                 // z-order of images

    DrawnItemVector  _copiedItems;      // copy items on _nSelectedList into this list for pasting anywhere even in newly opened documents
    ScreenShotImageList   _copiedImages;// copy images on _nSelectedList to this
    QRectF _copiedRect;                  // bounding rectangle for copied items used for paste operation

    IntVector _nSelectedItemsList,      // indices into '_items', that are completely inside the rubber band
              _nItemsRightOfList,       // -"- for elements that were at the right of the rubber band
              _nItemsLeftOfList;        // -"- for elements that were at the left of the rubber band
    int _nIndexOfFirstScreenShot;            // index of the first drawable (not image) element that is below a given y 
    QRectF _selectionRect;               // bounding rectangle for selected items OR rectangle in which there are no items
                                        // when _nSelectedItemList is empty
    bool _modified = false;

    HistoryItem* _AddItem(HistoryItem* p);

    int _YIndexForFirstVisible(bool onlyY = false);       // first visible drawable item in '_items' or -1 check area or only the y value
                                        // returns: 0 no item find, start at first item, -1: no items, (count+1) to next history item
    bool _IsSaveable(int i);

    int _YIndexForClippingRect(const QRectF &clip); // binary search in ordered list
    int _YIndexForXY(QPointF topLeft);        // index of top-left-most element below xy (y larger or equal to xy.y) 
                                        // in _items, using '_yOrder' (binary search)
//    int _YIndexWhichInside();// index of top most element whose area includes point xy
    QPointF _XYForIndex(int index)       // y coord of (physical) index-th element no check if thisa is valid!
    {
        return _items[index]->TopLeft();
    }
    int _YIndexForIndex(int index);      // returns index in _yxOrder
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

    History() {}
    History(const History& o);
    History(const History&& o);
    ~History();

    void SetClippingRect(const QRect& rect) { _clpRect = rect;  }

    void clear();
    int size() const;   // _items's size
    int CountOfVisible() const; // visible from yxOrder
    int CountOfDrawable() const { return _yxOrder.size(); }
    int SelectedSize() const { return _nSelectedItemsList.size(); }

    HistoryItem* operator[](int index);   // index: absolute index

    const qint32 MAGIC_ID = 0x53414d57; // "SAMW" - little endian !! MODIFY this and Save() for big endian processors!
    const qint32 MAGIC_VERSION = 0x56010000; // V 01.00.00

    /*========================================================
     * TASK:    save actual visible drawn items
     * PARAMS:  file name
     * GLOBALS:
     * RETURNS:
     * REMARKS: - drawn items follow each other in the order 
     *              they were used. 
     *          - if a clear screen was somewhere it contains
     *              the index of the last drawn item before it
     *              so only items after it are saved
     *-------------------------------------------------------*/
    bool Save(QString name);

    int Load(QString name);         // returns _ites.size() when Ok, -items.size()-1 when read error
    bool IsModified() const { return _modified & CanUndo(); }
    bool CanUndo() const { return _items.size(); }
    bool CanRedo() const { return _redo.size(); }

    HistoryItem* operator[](int index) const { return _items[index]; }                  // index: physical index in _items
    HistoryItem* atYIndex(int yindex) const        // yindex: index in _yxOrder
    {
        return _yitems(yindex);
    }
    void ReplaceItem(int index, HistoryItem* pi);     // index: in '_items'
    void RemoveItemsStartingAt(int index);

    int YIndexOfTopmost(bool onlyY = false);
    HistoryItem* TopmostItem();            // in ordered vector

    HistoryItem* NextVisibleItem();
    IntVector VisibleItemsBelow(int height = 0x7fffffff);

    QPointF BottomRightVisible(QSize screenSize) const;      // returns bottom right coordinate of last visible item


//--------------------- Add Items ------------------------------------------
    HistoryItem* addClearRoll();
    HistoryItem* addClearVisibleScreen();
    HistoryItem* addClearDown();
    HistoryItem* addDrawnItem(DrawnItem& dri);
    HistoryItem* addDeleteItems(Sprite* pSprite = nullptr);                  // using 'this' and _nSelectedItemsList a
    HistoryItem* addPastedItems(QPointF topLeft, Sprite *pSprite=nullptr);   // using 'this' and either '_copiedList'  or  pSprite->... lists
    HistoryItem* addRecolor(MyPenKind pk);
    HistoryItem* addInsertVertSpace(int y, int heightInPixels);
    HistoryItem* addScreenShot(int index);
    HistoryItem* addRotationItem(MyRotation rot);
    HistoryItem* addRemoveSpaceItem(QRect &rect);
    // --------------------- drawing -----------------------------------
    void Translate(int from, QPointF p, int minY);
    void Rotate(HistoryItem *forItem, MyRotation withRotation); // using _selectedRect
    void InserVertSpace(int y, int heightInPixels);

    int SetFirstItemToDraw();
    QRectF       Undo();        // returns top left after undo 
    HistoryItem* Redo();

    HistoryItem* GetOneStep();

    void AddToSelection(int index=-1);
    int CollectItemsInside(QRectF rect);
    void CopySelected(Sprite *forThisSprite = nullptr);      // copies selected scribbles into array. origin will be relative to (0,0)
                                                             // do the same with images

    const QVector<DrawnItem>& CopiedItems() const { return _copiedItems;  }
    int CopiedCount() const { return _copiedItems.size();  }
    const QRectF &BoundingRect() const { return _selectionRect; }
    const IntVector &Selected() const { return _nSelectedItemsList;  }
};

#endif