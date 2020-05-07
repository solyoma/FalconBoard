#pragma once
#ifndef _HISTORY_H
#define _HISTORY_H

#include <QtGui>
#include <QFile>
#include <QDataStream>
#include <QIODevice>

enum HistEvent {
    heNone,
    heScribble,        // series of points from start to finish of scribble
    heEraser,          // eraser used
    heRecolor,         // save old color, set new color
    heScreenShot,
    heItemsDeleted,     // store the list of items deleted in this event
    heSpaceDeleted,     // empty space is deleted
    heVertSpace,       // insert vertical space
    heVisibleCleared,  // visible image erased
    heBackgroundLoaded,     // an image loaded into _background
    heBackgroundUnloaded,   // image unloaded from background
    heItemsPasted       // list of draw events added first drawn item is at index 'drawnItem'
                        // last one is at index 'lastDrawnIndex'
                        // Undo: set _indexLastDrawnItem to that given in previous history item
                        // Redo: set _indexLastDrawnItem to 'lastDrawnIndex'
                };
enum MyPenKind { penNone, penBlack, penRed, penGreen, penBlue, penEraser, penYellow };

struct DrawnItem    // stores the freehand line strokes from pen down to pen up
{                   
    HistEvent type = heNone;
    bool isVisible = true;

    MyPenKind penKind = penBlack;
    int penWidth =1;
    QVector<QPoint> points;         // coordinates are relative to logical origin (0,0) => canvas coord = points[i] - origin
    QRect rect;                     // top left-bttom right coordinates of encapsulating rectangle
                                    // not saved on disk, recreated on read

    DrawnItem(HistEvent he = heNone) noexcept;       // default constructor
    DrawnItem(const DrawnItem& di);
    DrawnItem(const DrawnItem&& di);
    DrawnItem& operator=(const DrawnItem& di);

    DrawnItem& operator=(const DrawnItem&& di)  noexcept;

    void clear();

    static bool IsExtension(const QPoint& p, const QPoint& p1, const QPoint& p2 = QPoint()); // vectors p->p1 and p1->p are parallel?
    void add(QPoint p);

    void add(int x, int y);

    bool intersects(const QRect& arect) const;

    void Translate(QPoint dr, int minY);    // only if not deleted and top is above minY
};

inline QDataStream& operator<<(QDataStream& ofs, const DrawnItem& di);
inline QDataStream& operator>>(QDataStream& ifs, DrawnItem& di);

// ******************************************************
// image to shown on background
struct BelowImage {           // shown below the drawings
    QImage image;              // image from the disk or from screenshot
    QPoint topLeft;            // relative to (0,0) of 'paper roll' (widget coord: topLeft + DrawArea::_topLeft is used) 
    bool isVisible = true;
            // canvasRect relative to (0,0)
            // result: relative to image
            // isNull() true when no intersection
    QRect Area(const QRect& canvasRect) const
    {
        return QRect(topLeft, QSize(image.width(), image.height())).intersected(canvasRect);
    }
    void Translate(QPoint p, int minY)
    {
        if(topLeft.y() >= minY)
            topLeft += p;
    }
};

inline QDataStream& operator<<(QDataStream& ofs, const BelowImage& bimg);
inline QDataStream& operator>>(QDataStream& ifs, BelowImage& bimg);

class  BelowImageList : public  QList<BelowImage>
{
    int _index = -1;
    QRect _canvasRect;
public:
    void Add(QImage& image, QPoint pt)
    {
        BelowImage img;
        img.image = image;
        img.topLeft = pt;
        (*this).push_back(img);
    }

    // canvasRect and result are relative to (0,0)
    QRect Area(int index, const QRect& canvasRect) const
    {
        return (*this)[index].Area(canvasRect);
    }

    BelowImage* NextVisible()
    {
        if (_canvasRect.isNull())
            return nullptr;

        while (++_index < size())
        {
            if ((*this)[_index].isVisible)
            {
                if (!Area(_index, _canvasRect).isNull())
                    return &(*this)[_index];
            }
        }
        return nullptr;
    }

    BelowImage* FirstVisible(const QRect& canvasRect)
    {
        _index = -1;
        _canvasRect = canvasRect;
        return NextVisible();
    }

    void Translate(int which, QPoint p, int minY)
    {
        if (which < 0 || which >= size() || (*this)[which].isVisible)
            return;
        (*this)[which].Translate(p, minY);
    }

    void Clear()
    {
        QList<BelowImage>::clear();
    }
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
    HistEvent type = heNone;

    HistoryItem(History* pHist) : pHist(pHist) {}
    virtual ~HistoryItem() {}

    virtual DrawnItem* GetDrawable(int index = 0) const { return nullptr; } // returns pointer to the index-th DrawnItem

    virtual bool Translatable() const { return false;  }
    virtual void Translate(QPoint p, int minY) { } // translates if top is >= minY
    virtual int Size() const { return 0; }         // size of stored scribbles or erases
    virtual void SetVisibility(bool visible) { }

    // function prototypes for easier access
    virtual int Undo() { return 1; }        // returns # of items it changed
    virtual int Redo() { return 1; }        // returns # of items it changed
    virtual QRect Area() const { return QRect(); }  // encompassing rectangle for all points
    virtual bool Hidden() const { return true; }    // must not draw it
    virtual bool IsSaveable() const  { return ! Hidden(); }    // use when items may be deleetd
};



//--------------------------------------------
struct HistoryClearCanvasItem : HistoryItem
{
    DrawnItem* GetDrawable(int index = 0) const override { return nullptr; }
    HistoryClearCanvasItem(History* pHist);
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
//    bool IsSaveable() const override;
    DrawnItem* GetDrawable(int index = 0) const override;
    QRect Area() const override;
    int Size() const override { return 1; }
    bool Translatable() const override { return drawnItem.isVisible; }
    void Translate(QPoint p, int minY) override;
    // no new undo/redo needed
};

//--------------------------------------------
struct HistoryDeleteItems : public HistoryItem
{
    IntVector deletedList;   // indexes to element in '*pHist'
    int Undo() override;
    int Redo() override;
    HistoryDeleteItems(History* pHist, IntVector& selected);
    HistoryDeleteItems(HistoryDeleteItems& other);
    HistoryDeleteItems& operator=(const HistoryDeleteItems& other);
    HistoryDeleteItems(HistoryDeleteItems&& other);
    HistoryDeleteItems& operator=(const HistoryDeleteItems&& other);
    DrawnItem* GetDrawable(int index = 0) const override { return nullptr; }
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
struct HistoryPasteItem : HistoryItem
{
    QRect encompassingRect;
    DrawnItemVector pastedList;

    HistoryPasteItem(History* pHist, DrawnItemVector& pasteList, QRect& rect, QPoint topLeft);

    HistoryPasteItem(HistoryPasteItem& other);
    HistoryPasteItem& operator=(const HistoryPasteItem& other);
    HistoryPasteItem(HistoryPasteItem&& other);
    HistoryPasteItem& operator=(const HistoryPasteItem&& other);

    int Size() const override { return pastedList.size(); }

    int Undo() override;
    int Redo() override;

    DrawnItem* GetDrawable(int index = 0) const override;

    bool Hidden() const override;       // when the first element is hidden, all hidden
    // bool IsSaveable() const override;
    void SetVisibility(bool visible) override; // for all elements in list
    bool Translatable() const override { return !Hidden(); }
    void Translate(QPoint p, int minY) override;

    QRect Area() const override;
};
//--------------------------------------------
struct HistoryReColorItem : HistoryItem
{
    IntVector selectedList;                 // indexes  to elements in '*pHist'
    QVector<MyPenKind> penKindList;         // colors for elements in selectedList
    MyPenKind pk;                           
    QRect encompassingRectangle;            // to scroll here when undo/redo

    int Undo() override;
    int Redo() override;
    HistoryReColorItem(History* pHist, IntVector &selectedList, MyPenKind pk);
    HistoryReColorItem(HistoryReColorItem& other);
    HistoryReColorItem& operator=(const HistoryReColorItem& other);
    HistoryReColorItem(HistoryReColorItem&& other);
    HistoryReColorItem& operator=(const HistoryReColorItem&& other);
    QRect Area() const override;
    DrawnItem* GetDrawable(int index = 0) const override { return nullptr; }
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
    QRect Area() const override;
};

//--------------------------------------------
struct HistoryScreenShotItem : public HistoryItem
{
    int which;
    bool isVisible = true;

    HistoryScreenShotItem(History* pHist, int which);
    HistoryScreenShotItem(const HistoryScreenShotItem &other);
    HistoryScreenShotItem& operator=(const HistoryScreenShotItem& other);
    int Undo() override;
    int Redo() override;
    QRect Area() const override;
    bool Hidden() const override;       // when the first element is hidden, all hidden
    void SetVisibility(bool visible) override; // for all elements in list
    bool Translatable() const override { return isVisible; }
    void Translate(QPoint p, int minY) override;
};

/*========================================================
 * Class for storing history of editing
 *  contains a list of items drawn on screen, 
 *      screen deletion, item deleteion, canvas movement
 *  contains 
 *      list of history item
 *      list of drawn objects
 *           later history items point to later 'drawnList' items
 *           then earlier history items!
 *
 *      list of screen clearing event indices
 *      _actItem: index of the the last history item adedd
 *          new items are added after this index
 *          during undo: this index is decreased, at redo 
 *              it is increased
 *          if a new item is added after an undo it will be inserted
 *              after this position i.e. other existing items in
 *              '_items' will be invalid, similarly all items put
 *              into '_drawnItems' will also be invalid
 *-------------------------------------------------------*/
class History  // stores all drawing sections and keeps track of undo and redo
{
    QVector<HistoryItem*> _items;
    int _actItem = -1;                  // in _items, index of actual history item, -1: no such item
    int _endItem = 0;                   // index until a redo can go (max value: _items.size()

    DrawnItemVector  _copiedItems;      // copy items on _nSelectedList into this list for pasting anywhere even in newly opened documents
    BelowImageList   _copiedImages;
    QRect _copiedRect;                  // encompassing rectangle for copied items used for paste operation

    IntVector _nSelectedItemsList,      // indices into '_items', that are completely inside the rubber band
              _nItemsRightOfList,       // -"- for elements that were at the right of the rubber band
              _nItemsLeftOfList;        // -"- for elements that were at the left of the rubber band
    int _nIndexOfFirstBelow;            // index of the first element that is below
    QRect _selectionRect;               // encompassing rectangle for selected items OR rectangle in which there are no items
                                        // when _nSelectedItemList is empty

    bool _modified = false;

    int _index = -1;                    // start writing undo records from here


    bool _redoAble = false;             // set to true if no new drawing occured after an undo

    HistoryItem* _AddItem(HistoryItem* p);

    int _GetStartIndex();               // find first drawable item in '_items' after a clear screen 
                                        // returns: 0 no item find, start at first item, -1: no items, (count+1) to next history item
    bool _IsSaveable(int i);

public:
    BelowImageList* pImages = nullptr;  // set in DrawArea constructor

    History() {}
    ~History();

    void clear();
    int size() const;

    HistoryItem* operator[](int index);

    const qint32 MAGIC_ID = 0x53414d57; // "SAMW" - little endian !! MODIFY this and Save() for big endian processors!


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

    int Load(QString name, QPoint &lastPosition);  // returns _ites.size() when Ok, -items.size()-1 when read error
    bool IsModified() const { return _modified & CanUndo(); }
    bool CanUndo() const { return _actItem >= 0; }
    bool CanRedo() const { return _redoAble; }

//--------------------- Add Items ------------------------------------------
    HistoryItem* addClearCanvas();
    HistoryItem* addDrawnItem(DrawnItem& dri);
    HistoryItem* addDeleteItems();                  // using 'this' and _nSelectedItemsList a
    HistoryItem* addPastedItems(QPoint topLeft);    // using 'this' and _copiedList 
    HistoryItem* addRecolor(MyPenKind pk);
    HistoryItem* addInsertVertSpace(int y, int heightInPixels);
    HistoryItem* addScreenShot(int index);
    HistoryItem* addRemoveSpaceItem(QRect &rect);
    // --------------------- drawing -----------------------------------
    void Translate(int from, QPoint p, int minY);
    void InserVertSpace(int y, int heightInPixels);

    int SetFirstItemToDraw();
    QRect  Undo();        // returns top left after undo 
    HistoryItem* Redo();

    HistoryItem* GetOneStep();

    int CollectItemsInside(QRect rect);

    void CopySelected();      // copies selected scribbles into array. origin will be relative to (0,0)
    const QVector<DrawnItem>& CopiedItems() const { return _copiedItems;  }
    int CopiedCount() const { return _copiedItems.size();  }
    const QRect& EncompassingRect() const { return _selectionRect; }
    const QVector<int> &Selected() const { return _nSelectedItemsList;  }
};

#endif