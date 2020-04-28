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
enum MyPenKind { penNone, penBlack, penRed, penGreen, penBlue, penEraser };

struct DrawnItem    // stores the freehand line strokes from pen down to pen up
{                   
    HistEvent type = heNone;
    bool isDeleted = false;

    MyPenKind penKind = penBlack;
    int penWidth;
    QVector<QPoint> points;         // coordinates are relative to logical origin (0,0) => canvas coord = points[i] - origin
    QRect rect;                     // top left-bttom right coordinates of encapsulating rectangle
                                    // not saved on disk, recreated on read

    DrawnItem(HistEvent he = heScribble) noexcept;
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
//-------------------------------------------------------------------------
// a list of drawn elements
struct DrawnItemList
{
    QVector<DrawnItem> dw;
    int lastDrawnIndex = -1;        // start adding items after this

    DrawnItem* Add(DrawnItem& dri);
    DrawnItem* Add(QVector<DrawnItem> dwother, QRect& Area, QPoint* pTopLeft = nullptr);

    DrawnItem& operator[](int index);

    void Clear();
    void Purge();
    int Size();
};
/*========================================================
 * One history element
 *-------------------------------------------------------*/

using IntVector = QVector<int>;

struct HistoryItem      // base class
{
    HistEvent type = heNone;
    bool isSaveable = false;            // can be saved into file (scriible and erase only?
    // function prototypes for easier access
    virtual bool Undo() { return true; }        // returns if process is complete, false: move to previous item
    virtual bool Redo() { return true; }        // returns if process is complete, false: move to next item
    virtual bool IsSaveable() const  { return isSaveable; }    // use when items may be deleetd
    virtual DrawnItem* GetDrawable(int index = 0) const { return nullptr; }
    virtual QRect Area() const { return QRect(); }  // encompassing rectangle for all points
    virtual bool Hidden() const { return true; }    // must not draw it
};

//--------------------------------------------
struct HistoryClearCanvasItem : HistoryItem
{
    int drawnIndex;
    HistoryClearCanvasItem(int drawnIndex);
};

//--------------------------------------------
// type heScribble, heEraser
struct HistoryDrawItem : public HistoryItem
{
    DrawnItemList* pDrawn = nullptr;    // pointer to base of array/list of drawn scribbles
    int drawnIndex = -1;                // index in 'History::_drawnItems' first drawnable
    QRect encompassingRect;             // same as for the drawable item
    HistoryDrawItem(DrawnItem& dri, DrawnItemList* pdri);
    HistoryDrawItem(const HistoryDrawItem& other);
    HistoryDrawItem(const HistoryDrawItem&& other);

    bool Hidden() const override;

    HistoryDrawItem& operator=(const HistoryDrawItem& other);
    HistoryDrawItem& operator=(const HistoryDrawItem&& other);
    bool IsSaveable() const override;
    DrawnItem* GetDrawable(int index = 0) const override;
    QRect Area() const override;
    // no new undo/redo needed
};

//--------------------------------------------
struct HistoryDeleteItems : public HistoryItem
{
    DrawnItemList* pDrawn = nullptr;        // pointer to base of array/list of drawn scribbles
    IntVector deletedList;              // indexes to element in '*pDrawn'
    bool Undo() override;
    bool Redo() override;
    HistoryDeleteItems(DrawnItemList* pdri, IntVector& selected);
    HistoryDeleteItems(HistoryDeleteItems& other);
    HistoryDeleteItems& operator=(const HistoryDeleteItems& other);
    HistoryDeleteItems(HistoryDeleteItems&& other);
    HistoryDeleteItems& operator=(const HistoryDeleteItems&& other);
};

//--------------------------------------------
struct HistoryPasteItem : HistoryItem
{
    DrawnItemList* pDrawn = nullptr;        // pointer to base of array/list of drawn scribbles
    int firstPastedIndex;
    int lastPastedIndex;
    QRect encompassingRect;

    bool Undo() override;
    bool Redo() override;
    HistoryPasteItem(DrawnItemList* pdri, QVector<DrawnItem>& pastedList, QPoint topLeft);

    HistoryPasteItem(HistoryPasteItem& other);
    HistoryPasteItem& operator=(const HistoryPasteItem& other);
    HistoryPasteItem(HistoryPasteItem&& other);
    HistoryPasteItem& operator=(const HistoryPasteItem&& other);

    bool IsSaveable() const override;
    DrawnItem* GetDrawable(int index = 0) const override;

    QRect Area() const override;
};

//--------------------------------------------
struct HistoryReColorItem : HistoryItem
{
    DrawnItemList* pDrawn = nullptr;        // pointer to base of array/list of drawn scribbles
    IntVector selectedList;                 // indexes  to elements in '*pDrawn'
    QVector<MyPenKind> penKindList;         // colors for elements in selectedList
    MyPenKind pk;                           
    QRect encompassingRectangle;            // to scroll here when undo/redo

    bool Undo() override;
    bool Redo() override;
    HistoryReColorItem(DrawnItemList* pdri, MyPenKind pk, IntVector& selectedList);
    HistoryReColorItem(HistoryReColorItem& other);
    HistoryReColorItem& operator=(const HistoryReColorItem& other);
    HistoryReColorItem(HistoryReColorItem&& other);
    HistoryReColorItem& operator=(const HistoryReColorItem&& other);
    QRect Area() const override;
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
    DrawnItemList       _drawnItems;          // all items drawn on screen
    QVector<DrawnItem>  _copiedItems;    // copy items into this list for pasting anywhere even in newly opened documents

    QVector<HistoryItem*> _items;
    int _actItem = -1;                  // in _items, index of actual history item, -1: no such item
    int _endItem = 0;                 // index until a redo can go (max value: _items.size()

    IntVector _nSelectedItemsList;   // indices into '_drawnItems', set when we want to delete a bunch of items all together
    QRect _selectionRect;               // encompassing rectangle for selected items used for paste operation


    bool _modified = false;

    int _index = -1;            // start writing undo records from here


    bool _redoAble = false;      // set to true if no new drawing occured after an undo

    HistoryItem* _AddItem(HistoryItem* p);

    int _GetStartIndex();        // find first drawable item in '_items' after a clear screen 
                               // returns: 0 no item find, start at first item, -1: no items, (count+1) to next history item

    bool _IsSaveable(HistoryItem& item);         // when n drawables in it are deleted
    bool _IsSaveable(int i);

public:
    History() {}
    ~History();
    void clear();
    int size() const;

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

    int Load(QString name);  // returns _ites.size() when Ok, -items.size()-1 when read error
    bool IsModified() const { return _modified & CanUndo(); }
    bool CanUndo() const { return _actItem >= 0; }
    bool CanRedo() const { return _redoAble; }

//--------------------- Add Items ------------------------------------------

    HistoryItem* addDrawnItem(DrawnItem& itm);           // may be after an undo, so
                                                      // delete all scribbles after the last visible one (items[lastItem].drawnIndex)
    HistoryItem* addDeleteItems();
    HistoryItem* addPastedItems(QPoint topLeft);
    HistoryItem* addClearCanvas();
    HistoryItem* addRecolor(MyPenKind pk);

    DrawnItem* DrawnItemAt(int index);
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
