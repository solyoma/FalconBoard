#pragma once
#ifndef _HISTORY_H
#define _HISTORY_H


#include <algorithm>
#include <limits>

#include <QApplication>
#include <QtGui>
#include <QFile>
#include <QDataStream>
#include <QIODevice>

#include "common.h"

#include "quadtree.h"

// for versions 1.x
enum HistEvent1 {
    heNone,
        // these are types saved on disk
    heScribble,        // series of points from start to finish of scribble
    heEraser,          // eraser used
    heScreenShot,
    heText,
        // these are not saved
    heRecolor,         // save old color, set new color
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
 // determined from the number of points
enum ScribbleSubType { sstScribble,     // any number
                       sstLine,         // only two points
                       sstQuadrangle     // 4 points, where the last and first are equal
                     };    
enum MyRotation { rotNone, 
                  rotR90, rotL90, rot180, rotFlipH, rotFlipV,     // these leave the top left corner in place
                  rotAlpha                                        // alpha: around the center of the bounding box, increases counterclockwise
                };
enum MyLayer { 
                mlyBackgroundImage,         // lowest layer: background image
                mlyScreenShot,              // screenshot layer: may write over it
                mlyScribbleItem,            // scribbles are drawn here
                mlySprite                   // layer for sprites (moveable images)
             };

// stores the coordinates of line strokes from pen down to pen up:
struct ScribbleItem        // drawn on layer mltScribbleItem
{                   
    HistEvent1 type = heNone;
    int zOrder = 0;
    bool isVisible = true;

    FalconPenKind penKind = penBlack;
    int penWidth =1;
    QPolygon points;         // coordinates are relative to logical origin (0,0) => canvas coord = points[i] - origin
    bool filled = false;            // wheather closed polynom (ellipse or rectangle) is filled
    MyRotation rot = rotNone;       // used only when rotation item added
    float rAlpha = 0;               // for 'rotAlpha': rotation around  center of bounding box
    QRect bndRect;                  // top left-bttom right coordinates of bounding rectangle
                                    // not saved on disk, recreated on read
    QPainterPath pPath;         // used for faster successive displays

    ScribbleItem(HistEvent1 he = heNone, int zorder = 0) noexcept;       // default constructor
    ScribbleItem(const ScribbleItem& di);
    ScribbleItem(const ScribbleItem&& di) noexcept;
    ScribbleItem& operator=(const ScribbleItem& di);

    ScribbleItem& operator=(const ScribbleItem&& di)  noexcept;

    ScribbleSubType SubType()
    {
        switch (points.size())
        {
            case 2: return sstLine;
            case 4: return points[0] == points[3] ? sstQuadrangle : sstScribble;
            default: return sstScribble;
        }
    }

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

struct TextItem {
    QString _text;                  // so that a text() fucntion can be created
    QPoint topLeft;                 // just left-to-right drawing
    QRect bndRect;                  // top left-bttom right coordinates of bounding rectangle
                                    // not saved on disk, recreated on read
    QString fontAsString;           // list of all properties separated by commas

    TextItem(HistEvent1 he = heNone, int zorder = 0) noexcept;       // default constructor
    TextItem(const TextItem& di);
    TextItem(const TextItem&& di) noexcept;
    TextItem& operator=(const TextItem& di);

    TextItem& operator=(const TextItem&& di)  noexcept;

    void setText(QString txt);
    QString text() const { return _text; }

    inline void setFont(QString font) { fontAsString = font; }
};

// ******************************************************
// image to shown on background
struct ScreenShotImage {        // shown on layer lyScreenShot below the drawings
    QPixmap image;              // image from the disk or from screenshot (transparency included)
    QPoint topLeft;             // relative to logical (0,0) of 'paper roll' (widget coord: topLeft + DrawArea::_topLeft is used) 
    int itemIndex;              // in history::_items
    bool isVisible = true;
    int zOrder = 0;             // of images is the index of this image on the pimages list in History
            // hack: because new fields added for transparency of screenshot pixels

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


/*========================================================
 * A List of screenshots loaded for a history
 *-------------------------------------------------------*/
class  ScreenShotImageList : public  QList<ScreenShotImage>
{
    int _index = -1;
    QRect _canvasRect;  // documentum (not screen) relative rectangle
public:
    void Add(QPixmap& image, QPoint pt, int zorder);
    void Drop(int index) { /* TODO */ }   // removes from list without changing the index of the other images
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
    History* pHist=nullptr;
    HistEvent1 type;

    HistoryItem(History* pHist, HistEvent1 typ=heNone) : pHist(pHist), type(typ) {}
    virtual ~HistoryItem() {}

    virtual int ZOrder() const { return 0; }

    virtual QPoint TopLeft() const { return QPoint(); }
    virtual ScribbleItem* GetVisibleScribble(int index = 0) const { return nullptr; } // returns pointer to the index-th ScribbleItem
    virtual ScribbleItem* GetScribble(int index = 0) const { return nullptr; } // returns pointer to the index-th ScribbleItem even when it is not visible
    virtual ScreenShotImage* GetScreenShotImage() const { return nullptr; }

    virtual bool Translatable() const { return false;  }
    virtual void Translate(QPoint p, int minY) { } // translates if top is >= minY
    virtual void Rotate(MyRotation rot, QRect encRect, float alpha=0.0) { ; }      // rotation or flip
    virtual int Size() const { return 0; }         // size of stored scribbles or erases
    virtual void SetVisibility(bool visible) { }

    // function prototypes for easier access
    virtual int RedoCount() const { return 1; } // how many elements from _redoList must be moved back to _items
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
    IntVector deletedList;   // absolute indexes of scribble elements in '*pHist'
    int Undo() override;
    int Redo() override;
    HistoryDeleteItems(History* pHist, IntVector& selected);
    HistoryDeleteItems(HistoryDeleteItems& other);
    HistoryDeleteItems& operator=(const HistoryDeleteItems& other);
    HistoryDeleteItems(HistoryDeleteItems&& other) noexcept;
    HistoryDeleteItems& operator=(const HistoryDeleteItems&& other) noexcept;
    ScribbleItem* GetScribble(int index = 0) const override { return nullptr; }
    ScribbleItem* GetVisibleScribble(int index = 0) const override { return nullptr; }
};
//--------------------------------------------
// remove an empty rectangular region 
//  before creating this item collect all items to
//  the left and right of this rectangle into two
//  lists: 'left' and 'right' then
// 
//  if 'right' is not empty moves them to the left
//      by the width of the rectangle
//  if 'right' is empty and 'left' is not do nothing
//  if both 'right' and 'left' are empty
//      move all items below the rectangle up by the
//      height of the rectangle
struct HistoryRemoveSpaceItem : public HistoryItem // using _selectedRect
{
    IntVector modifiedList; // elements to be moved horizontally (elements on the right)
                                  // if empty move all below y up
    int delta;              // translate with this amount left OR up
    int y;                  // topmost coordinate to remove space from

    int Undo() override;
    int Redo() override;
    HistoryRemoveSpaceItem(History* pHist, IntVector&toModify, int distance, int y);
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
    IntVector selectedList;               // indexes  to elements in '*pHist'
    QVector<FalconPenKind> penKindList;         // colors for elements in selectedList
    FalconPenKind pk;                           
    QRect boundingRectangle;            // to scroll here when undo/redo

    int Undo() override;
    int Redo() override;
    HistoryReColorItem(History* pHist, IntVector&selectedList, FalconPenKind pk);
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
    IntVector nSelectedItemList;
    QRect encRect;         // encompassing rectangle: all items inside

    HistoryRotationItem(History* pHist, MyRotation rotation, QRect rect, IntVector selList, float alpha=0.0);
    HistoryRotationItem(const HistoryRotationItem& other);
    HistoryRotationItem& operator=(const HistoryRotationItem& other);
    int Undo() override;
    int Redo() override;
};

struct HistorySetTransparencyForAllScreenshotsItems : HistoryItem
{
    IntVector affectedIndexList;   // these images were re-created with transparent color set
    int firstIndex = -1;            // top of pHist's screenShotImageList before this function was called
    QColor transparentColor;
    qreal fuzzyness = 0.0;
    HistorySetTransparencyForAllScreenshotsItems(History* pHist, QColor transparentColor, qreal fuzzyness);
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
    Sprite(QRect &rect, ScreenShotImageList *pimg, ScribbleItemVector *pitems);

    History* pHist;
    QPoint topLeft;     // top,left: position of sprite rel. to top left of visible area of canvas
    QPoint dp;          // relative to top left of sprite: cursor coordinates when the sprite is selected
    QImage image;       // image of the sprite to paint over the canvas
    QRect rect;         // selection rectangle 0,0, width,height
    bool itemsDeleted = true;   // unless Alt+
    bool visible = true;// copying should create new invisible sprite


    IntVector       nSelectedItemsList;     // indices into 'pHist->_items', that are completely inside the rubber band rectangle
    ScribbleItemVector    items;      // each point of items[..] is relative to top-left of sprite (0,0)
    ScreenShotImageList   images;     // image top left is relative to top left of sprite (0,0)
};

// vector of pointers to dynamically allocated history items
using HistoryItemVector = QVector<HistoryItem*>;

class HistoryList;

using HistoryItemPointer = HistoryItem*;
QuadArea AreaForItem(const int &index);
bool IsItemsEqual(const int& index1, const int &index2);

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

    HistoryList* _parent=nullptr;       // need for copy and paste
    QPoint _topLeft;                    // top left of the visible part of this history

    QString _fileName,                  // file for history
            _loadedName;                // set only after the file is loaded, used to check reloads
    bool _inLoad = false;               // in function Load() / needed for correct z- order settings
    HistoryItemVector _items,           // items in the order added. Items need not be scribble.
                                        // scribble elements on this list may be either visible or hidden
                      _redoList;        // from _items for redo. Items need not be scribbles.
                                        // scribble elements on this list may be either visible or hidden
        // Quad tree for storing indices in History::_items *same as pointers)
    QuadTree<int, decltype(AreaForItem), decltype(IsItemsEqual)> *_pItemTree = nullptr;
    int _readCount = 0;                 // undo works until this index is reached
                                        // unscribble items have no indices in here
    bool _isSaved = false;              // clear after every change!
    QRect _clpRect;                     // clipping rectangle for selecting points to draw
                                        // before searching operations set this when it is changed
    QStack<QRect> _savedClps;           // clipRect saves

    int _indexOfFirstVisible = -1;      // in _yxorder

    int _lastZorder = DRAWABLE_ZORDER_BASE;  // increased when scribble elements adedd to _items, does not get decreasd when they are taken off!
    int _nextImageZorder = 0;                 // z-order of images

                                             // copy items on _nSelectedItemsList into this list for pasting anywhere even in newly opened documents
    ScreenShotImageList  _screenShotImageList;     // one or more images from screenshots

    IntVector _nSelectedItemsList,      // indices into '_items', that are completely inside the rubber band (includes screenshots - zorder < DRAWABLE_ZORDER_BASE)
                    _nItemsRightOfList,       // -"- for elements that were at the right of the rubber band
                    _nItemsLeftOfList;        // -"- for elements that were at the left of the rubber band
    int _nIndexOfFirstScreenShot=0;       // index of the first scribble (not image) element that is below a given y (always calulate)
    QRect _selectionRect;              // bounding rectangle for selected items OR rectangle in which there are no items
                                        // when _nSelectedItemList is empty

    IntVector _images;      // indices into _screenShotImageList

    bool _modified = false;

    ScribbleSubType _GetSubType(ScribbleItem* psi)  // needed for finding nearest scribble
    {
        switch (psi->points.size())
        {
            case 2: return sstLine;
            case 4: return psi->points[0] == psi->points[3] ? sstQuadrangle : sstScribble;
            default: return sstScribble;
        }
    }

    HistoryItem* _AddItem(HistoryItem* p);

    bool _IsSaveable(int i);

    void _push_back(HistoryItem* pi);

    void _SaveClippingRect();
    void _RestoreClippingRect();
    void _ClearSelectLists() 
    { 
        _nSelectedItemsList.clear();
        _nItemsRightOfList.clear();
        _nItemsLeftOfList.clear();
    }

public:
    History(HistoryList* parent) noexcept;
    History(const History& o);
    History(History&& o) noexcept;
    virtual ~History();

    IntVector GetItemIndexesInRect(QRect r);
    QSize UsedArea();   // of all points and images
    int CountOnPage(int px, int py, QSize pageSize, bool &getAreaSize); // -1: invalid page for px, -2: invalid page for py i.e. outside used area. First call with getAreaSize=true, others with false

    HistoryItemPointer Item(int index) const 
    { 
        return _items[index]; 
    }

    constexpr QPoint TopLeft() const { return _topLeft; }
    void SetTopLeft(QPoint& topLeft) { _topLeft = topLeft; }

    int RightMostInBand(QRect rect);    // only top and bottom of rect is used

    void SetClippingRect(const QRect& rect) { _clpRect = rect;  }
    void SetVisibility(int index, bool visible)
    {
        if (_items[index]->Hidden() == visible)    // when it does change
            _items[index]->SetVisibility(visible);
        if (visible)
            _pItemTree->Add(index);
        else
            _pItemTree->Remove(index);
    }

    void ClearImageList() { _images.clear(); }
    int Countimages() { return _images.size(); }
    void AddImage(int index) { _images.push_back(index); }
    ScreenShotImage& Image(int index) { return _screenShotImageList[index]; }
    ScreenShotImageList &ScreenShotList() { return _screenShotImageList; }

    void Clear();
    void ClearName() { if (_items.isEmpty()) _fileName.clear(); }

    int Size() const;   // _items's size
    int CountOfVisible() const; // visible 
    int CountOfScribble() const;
    int SelectedSize() const { return _nSelectedItemsList.size(); }
    IntVector& SelectedItemsList() { return _nSelectedItemsList; }


    const qint32 MAGIC_ID = 0x53414d57; // "SAMW" - little endian !! MODIFY this and Save() for big endian processors!
    const qint32 MAGIC_VERSION = 0x56010108; // V 01.00.00      Cf.w. common.h

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

    int Load(bool force=false);       // from '_fileName', returns _items.size() when Ok, -items.size()-1 when read error
    bool IsModified() const 
    { 
        return _modified & CanUndo(); 
    }
    bool CanUndo() const { return _items.size() > _readCount; } // only undo until last element read
    bool CanRedo() const { return _redoList.size(); }
    void ClearUndo() { _readCount = _items.size(); }

    HistoryItem* LastScribble() const;
    HistoryItem* operator[](int index);   // index: absolute index
    HistoryItem* operator[](int index) const { return _items[index]; }  // index: physical index in _items
    void ReplaceItem(int index, HistoryItem* pi);     // index: in '_items'

    QPoint BottomRightVisible(QSize screenSize) const;      // returns bottom right coordinate of last visible item

//--------------------- Add Items ------------------------------------------
    HistoryItem* AddClearRoll();
    HistoryItem* AddClearVisibleScreen();
    HistoryItem* AddClearDown();
    HistoryItem* AddScribbleItem(ScribbleItem& dri);
    HistoryItem* AddDeleteItems(Sprite* pSprite = nullptr);                  // using 'this' and _nSelectedItemsList a
    HistoryItem* AddPastedItems(QPoint topLeft, Sprite *pSprite=nullptr);    // using 'this' and either '_copiedList'  or  pSprite->... lists
    HistoryItem* AddRecolor(FalconPenKind pk);
    HistoryItem* AddInsertVertSpace(int y, int heightInPixels);              // height < 0: delete space
    HistoryItem* AddScreenShot(ScreenShotImage &bimg);                       // to _screenShotImageList
    HistoryItem* AddRotationItem(MyRotation rot);
    HistoryItem* AddRemoveSpaceItem(QRect &rect);
    HistoryItem* AddScreenShotTransparencyToLoadedItems(QColor trColor, qreal fuzzyness);
    // --------------------- drawing -----------------------------------
    void VertShiftItemsBelow(int belowY, int deltaY);
    void Rotate(HistoryItem *forItem, MyRotation withRotation); // using _selectedRect
    void InserVertSpace(int y, int heightInPixels);

    HistoryItem* Undo();        // returns top item after undo or nullptr
    HistoryItem* Redo();

    int GetScribblesInside(QRect rect, HistoryItemVector& hv);
    int ImageIndexFor(QPoint& p) const { return _screenShotImageList.ImageIndexFor(p); } // -1: no such image else index in 'pImages'

    void AddToSelection(int index=-1);
    QRect SelectScribblesFor(QPoint& p, bool addToPrevious);      // selects clicked (if any) into _nSelectedItemsList, and clears right and left items list
    int CollectItemsInside(QRect rect);
    int SelectTopmostImageFor(QPoint p);
    void CopySelected(Sprite *forThisSprite = nullptr);      // copies selected scribbles into array. origin will be relative to (0,0)
                                                             // do the same with images
    void SetSelectionRect(QRect& rect)
    {
        _selectionRect = rect;
    }

    void CollectPasted(const QRect &rect);   // if items pasted copies them into '_nSelectedItemsList'

    const QRect BoundingRect() const { return _selectionRect; }
    const IntVector& Selected() const { return _nSelectedItemsList;  }
};

class HistoryList : public std::vector<History*>
{
    friend class History;
    QClipboard* _pClipBoard;
    QUuid _copyGUID;
    ScreenShotImageList* _pCopiedImages = nullptr; // put copied images on this list 
    ScribbleItemVector* _pCopiedItems = nullptr;   // points to list in Draw Area 
    QRect *_pCopiedRect;                           // bounding rectangle for copied items used for paste operation
    int _actual = -1;

public:
    HistoryList() { _pClipBoard = QApplication::clipboard(); }
        // called to initialize pointers to  data in 'DrawArea'
    void SetCopiedLists(ScreenShotImageList* pImgList, ScribbleItemVector* pItemList, QRect *pCopiedRect)
    {
        _pCopiedImages  = pImgList;
        _pCopiedItems   = pItemList;
        _pCopiedRect    = pCopiedRect;
    }
    const QVector<ScribbleItem>& CopiedItems() const { return *_pCopiedItems;  }
    int CopiedCount() const { return _pCopiedItems->size();  }
    QRect const *CopiedRect() const { return _pCopiedRect; }
    History*& operator[](int index)     // will throw if index < 0 && _actual < 0
    {
        if (index < 0)
            return std::vector<History*>::operator[](_actual);
		else
		{
			_actual = index;
			return std::vector<History*>::operator[](index);
		}
    }

    void CopyToClipboard();    // uses _pCopiedItems and _pCopiedImages so these must be setup first
    void PasteFromClipboard(); // only if the clipboard data is not the same as data in memory gets data from clipboard
};

QBitmap MyCreateMaskFromColor(QPixmap& pixmap, QColor color, qreal fuzzyness = 0.0, Qt::MaskMode mode = Qt::MaskInColor);

extern HistoryList historyList;

#endif