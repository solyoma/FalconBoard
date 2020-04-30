#pragma once

#include <QtGui>
#include <QFile>
#include <QDataStream>
#include <QIODevice>

enum HistEvent {
    heNone,
    heScribble,        // series of points from start to finish of scribble
    heEraser,          // eraser used
    heRecolor,         // save old color, set new color
    heVisibleCleared,  // visible image erased
    heBackgroundLoaded,     // an image loaded into _background
    heBackgroundUnloaded,   // image unloaded from background
    heItemsDeleted,     // store the list of items deleted in this event
    heItemsPasted       // list of draw events added first drawn item is at index 'drawnItem'
                        // last one is at index 'lastDrawnIndex'
                        // Undo: set _indexLastDrawnItem to that given in previous history item
                        // Redo: set _indexLastDrawnItem to 'lastDrawnIndex'
                };
enum MyPenKind { penNone, penBlack, penRed, penGreen, penBlue, penEraser, penYellow };

struct DrawnItem    // stores the freehand line strokes from pen down to pen up
{                   
    HistEvent type = heNone;
    bool isDeleted = false;

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

    static bool IsExtension(QPoint& p, QPoint& p1, QPoint& p2 = QPoint()); // vectors p->p1 and p1->p are parallel?
    void add(QPoint p);

    void add(int x, int y);

    bool intersects(const QRect& arect) const;

    void Translate(QPoint dr);
};

inline QDataStream& operator<<(QDataStream& ofs, const DrawnItem& di);

inline QDataStream& operator>>(QDataStream& ifs, DrawnItem& di);

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

    virtual DrawnItem* GetDrawable(int index = 0) const = 0; // returns pointer to the index-th DrawnItem
    virtual int Size() const { return 0; }         // size of stored scribbles or erases
    virtual void SetVisibility(bool visible) { }

    // function prototypes for easier access
    virtual bool Undo() { return true; }        // returns if process is complete, false: move to previous item
    virtual bool Redo() { return true; }        // returns if process is complete, false: move to next item
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
    // no new undo/redo needed
};

//--------------------------------------------
struct HistoryDeleteItems : public HistoryItem
{
    IntVector deletedList;   // indexes to element in '*pDrawn'
    bool Undo() override;
    bool Redo() override;
    HistoryDeleteItems(History* pHist, IntVector& selected);
    HistoryDeleteItems(HistoryDeleteItems& other);
    HistoryDeleteItems& operator=(const HistoryDeleteItems& other);
    HistoryDeleteItems(HistoryDeleteItems&& other);
    HistoryDeleteItems& operator=(const HistoryDeleteItems&& other);
    DrawnItem* GetDrawable(int index = 0) const override { return nullptr; }
};

//--------------------------------------------
struct HistoryPasteItem : HistoryItem
{
    QRect encompassingRect;
    DrawnItemVector pastedList;

    HistoryPasteItem(History* pHist, DrawnItemVector& pastedList, QRect& rect, QPoint topLeft);

    HistoryPasteItem(HistoryPasteItem& other);
    HistoryPasteItem& operator=(const HistoryPasteItem& other);
    HistoryPasteItem(HistoryPasteItem&& other);
    HistoryPasteItem& operator=(const HistoryPasteItem&& other);

    int Size() const override { return pastedList.size(); }

    bool Undo() override;
    bool Redo() override;

    DrawnItem* GetDrawable(int index = 0) const override;

    bool Hidden() const override;       // when the first element is hidden, all hidden
    // bool IsSaveable() const override;
    void SetVisibility(bool visible) override; // for all elements in list

    QRect Area() const override;
};

//--------------------------------------------
struct HistoryReColorItem : HistoryItem
{
    IntVector selectedList;                 // indexes  to elements in '*pHist'
    QVector<MyPenKind> penKindList;         // colors for elements in selectedList
    MyPenKind pk;                           
    QRect encompassingRectangle;            // to scroll here when undo/redo

    bool Undo() override;
    bool Redo() override;
    HistoryReColorItem(History* pHist, IntVector &selectedList, MyPenKind pk);
    HistoryReColorItem(HistoryReColorItem& other);
    HistoryReColorItem& operator=(const HistoryReColorItem& other);
    HistoryReColorItem(HistoryReColorItem&& other);
    HistoryReColorItem& operator=(const HistoryReColorItem&& other);
    QRect Area() const override;
    DrawnItem* GetDrawable(int index = 0) const override { return nullptr; }
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
    QRect _copiedRect;               // encompassing rectangle for copied items used for paste operation
    IntVector _nSelectedItemsList;      // indices into '_items', set when we want to delete a bunch of items all together
    QRect _selectionRect;               // encompassing rectangle for selected items


    bool _modified = false;

    int _index = -1;                    // start writing undo records from here


    bool _redoAble = false;             // set to true if no new drawing occured after an undo

    HistoryItem* _AddItem(HistoryItem* p);

    int _GetStartIndex();               // find first drawable item in '_items' after a clear screen 
                                        // returns: 0 no item find, start at first item, -1: no items, (count+1) to next history item
    bool _IsSaveable(int i);

public:
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
    HistoryItem* addClearCanvasItem();
    HistoryItem* addDrawnItem(DrawnItem& dri);
    HistoryItem* addDeleteItems();                  // using 'this' and _nSelectedItemsList a
    HistoryItem* addPastedItems(QPoint topLeft);    // using 'this' and _copiedList 
    HistoryItem* addClearCanvas();
    HistoryItem* addRecolor(MyPenKind pk);

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
