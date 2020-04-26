#pragma once

#include <QFile>
#include <QDataStream>
#include <QIODevice>

enum HistEvent {
    heNone,
    heScribble,        // series of points from start to finish of scribble
    heEraser,          // eraser used
    heRecolor,         // save old color, set new color
    heVisibleCleared,  // visible image erased
    heTopLeftChanged,  // store new top and left in two element of a HistoryItems's deletedList
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

    DrawnItem(HistEvent he = heScribble) noexcept : type(he) {}
    DrawnItem(const DrawnItem& di)  { *this = di; }
    DrawnItem(const DrawnItem&& di) noexcept { *this = di; }
    DrawnItem& operator=(const DrawnItem& di)
    {
        type = di.type;
        penKind = di.penKind;
        penWidth = di.penWidth;
        points = di.points;
        rect = di.rect;
        return *this;
    }

    DrawnItem& operator=(const DrawnItem&& di)  noexcept
    {
        type = di.type;
        penKind = di.penKind;
        penWidth = di.penWidth;
        points = di.points;
        rect = di.rect;
        return *this;
    }

    void clear() 
    { 
        points.clear(); 
        rect = QRect();
    }

    static bool IsExtension(QPoint& p, QPoint& p1, QPoint& p2 = QPoint()) // vectors p->p1 and p1->p are parallel?
    {
        //return false;       // DEBUG as it is not working yet

        if (p == p1)
            return true;    // nullvector may point in any direction :)

        QPoint vp = p - p1,  // not (0, 0)
            vpp = p1 - p2;

            // the two vectors point in the same direction when vpp.y()/vpp.x() == vp.y()/vp.x()
            // i.e to avoid checking for zeros in divison: 
        return vpp.y() * vp.x() == vp.y() * vpp.x();
    }

    void add(QPoint p)
    {
        if (rect.isNull())
            rect = QRect(p.x(), p.y(), 1, 1);      // single point
        else if (!rect.contains(p))
        {
            if (rect.x() > p.x()) rect.setX(p.x());
            if (rect.y() > p.y()) rect.setY(p.y());
            if (rect.right() < p.x()) rect.setRight(p.x());
            if (rect.bottom() < p.y()) rect.setBottom(p.y());
        }
        int n = points.size() - 1;       
                                        
                                        
        if(n < 0  )
            points.push_back(p);
        else                      // we need at least one point already in the array
        {   // if a point just extends the line in the same direction (IsExtension())
            // as the previous point was from the one before it
            // then do not add a new point, just modify the coordintes
            // (n 0: 1 point, >0: at least 2 points are already in the array
            if( n > 0 && IsExtension(p, points[n], points[n-1]) )  // then the two vector points in the same direction
                points[n] = p;                            // so continuation
            else
                points.push_back(p);
        }
    }

    void add(int x, int y)
    {
        QPoint p(x, y);
        add(p);
    }

    bool intersects(const QRect& arect) const
    {
        return rect.intersects(arect);
    }

    void Translate(QPoint dr)
    {
        for (int i = 0; i < points.size(); ++i)
            points[i] = points[i] + dr;
        rect.translate(dr);
    }

};

inline QDataStream& operator<<(QDataStream& ofs, const DrawnItem& di)
{
    ofs << (qint32)di.type << (qint32)di.penKind << (qint32)di.penWidth;
    ofs << (qint32)di.points.size();
    for (auto pt : di.points)
        ofs << (qint32)pt.x() << (qint32)pt.y();
    return ofs;
}

inline QDataStream& operator>>(QDataStream& ifs, DrawnItem& di)
{
    qint32 n;
    ifs >> n; di.type = static_cast<HistEvent>(n);
    ifs >> n; di.penKind = (MyPenKind)n;
    ifs >> n; di.penWidth = n;

    qint32 x,y;
    di.clear();

    ifs >> n; 
    while (n--)
    {
        ifs >> x >> y;
        di.add(x, y);
    }
    return ifs;
}
//-------------------------------------------------------------------------
// a list of drawn elements
struct DrawnItemList
{
    QVector<DrawnItem> dw;
    int lastDrawnIndex = -1;        // start adding items after this

    DrawnItem *Add(DrawnItem& dri)
    {
        if (++lastDrawnIndex == dw.size())
            dw.push_back(dri);
        else
            dw[lastDrawnIndex] = dri;
        return &dw[lastDrawnIndex];
    }
    DrawnItem* Add(QVector<DrawnItem> dwother, QRect &Area)
    {
        DrawnItem* p;
        Area = QRect();

        for (auto dri : dwother)
        {
            p = Add(dri);          // return only the last one
            Area = Area.united(p->rect);
        }
        return p;
    }

    DrawnItem& operator[](int index)
    { 
        return dw[index];
    }

    void Clear() 
    { 
        dw.clear();
        lastDrawnIndex = -1;
    }
    int Size()
    {
        return dw.size();
    }
};

/*========================================================
 * One history element
 *-------------------------------------------------------*/

using IntVector = QVector<int>;

struct HistoryItem      // base class
{
    HistEvent type = heNone;
    bool isSaveable = false;            // can be saved into file (scriible and erase only?
    virtual bool Undo() { return true; }        // returns if process is complete, false: move to previous item
    virtual bool Redo() { return true; }        // returns if process is complete, false: move to next item
    virtual bool IsSaveable() const  { return isSaveable; }    // use when items may be deleetd
    virtual DrawnItem* GetDrawable(int index = 0) const { return nullptr; }
    virtual QRect Area() const { return QRect(); } // encompassing rectangle for all points
    virtual bool Hidden() const { return true; }  // must not draw it
};

struct HistoryClearVisible : HistoryItem
{
    HistoryClearVisible() : HistoryItem() { type =heVisibleCleared; }
};

// type heScribble, heEraser
struct HistoryDrawItem : public HistoryItem
{
    DrawnItemList* pDrawn = nullptr;    // pointer to base of array/list of drawn scribbles
    int drawnIndex = -1;                // index in 'History::_drawnItems' first drawnable
    QRect encompassingRect;             // same as for the drawable item
    HistoryDrawItem(DrawnItem &dri, DrawnItemList* pdri) : HistoryItem(), pDrawn(pdri) 
    { 
        type = dri.type; 
        isSaveable = true;
        encompassingRect = pDrawn->Add(dri)->rect;
        drawnIndex = pDrawn->lastDrawnIndex;
    }
    HistoryDrawItem(const HistoryDrawItem& other)
    {
        *this = other;
    }
    HistoryDrawItem(const HistoryDrawItem&& other)
    {
        *this = other;
    }

    bool Hidden() const override {return (*pDrawn)[drawnIndex].isDeleted; }

    HistoryDrawItem& operator=(const HistoryDrawItem& other)
    {
        type = other.type;
        isSaveable = true;
        pDrawn = other.pDrawn;
        drawnIndex = other.drawnIndex;
        encompassingRect = other.encompassingRect;
    }
    HistoryDrawItem& operator=(const HistoryDrawItem&& other)
    {
        type = other.type;
        isSaveable = true;
        pDrawn = other.pDrawn;
        drawnIndex = other.drawnIndex;
        encompassingRect = other.encompassingRect;
    }
    bool IsSaveable() const override { return (*pDrawn)[drawnIndex].isDeleted; }
    DrawnItem* GetDrawable(int index = 0) const override 
    { 
        return (index || (*pDrawn)[drawnIndex].isDeleted) ? nullptr : &(*pDrawn)[drawnIndex];
    }
    const QRect Area() { return encompassingRect; }
    // no new undo/redo needed
};

struct HistoryTopLeftChangedItem : public HistoryItem
{
    QPoint topLeft;                     // moved here

    //bool Undo() override;
    //bool Redo() override;
    HistoryTopLeftChangedItem(QPoint tl) : HistoryItem(), topLeft(tl) { type = heItemsDeleted; }
    HistoryTopLeftChangedItem(const HistoryTopLeftChangedItem& other)
    {
        *this = other;
    }
    HistoryTopLeftChangedItem& operator=(const HistoryTopLeftChangedItem& other)
    {
        type = heItemsDeleted;
        topLeft = topLeft;
        return *this;
    }
    HistoryTopLeftChangedItem(const HistoryTopLeftChangedItem&& other)
    {
        *this = other;
    }
    HistoryTopLeftChangedItem& operator=(const HistoryTopLeftChangedItem&& other)
    {
        type = heItemsDeleted;
        topLeft = topLeft;
        return *this;
    }
};

struct HistoryDeleteItems : public HistoryItem
{
    DrawnItemList* pDrawn = nullptr;        // pointer to base of array/list of drawn scribbles
    bool hidden = true;
    IntVector deletedList;              // indexes to element in '*pDrawn'
    bool Undo() override
    {
        for (auto i : deletedList)
            (*pDrawn)[i].isDeleted = false;
        hidden = false;
        return true;
    }
    bool Redo() override
    {
        for (auto i : deletedList)
            (*pDrawn)[i].isDeleted = true;
        hidden = true;
        return true;
    }
    bool Hidden() const override { return hidden; }
    HistoryDeleteItems(DrawnItemList* pdri, IntVector &selected) : HistoryItem(), pDrawn(pdri), deletedList(selected) 
    { 
        type = heItemsDeleted; 
        Redo();         // and mark them
    }
    HistoryDeleteItems(HistoryDeleteItems& other) : HistoryDeleteItems(other.pDrawn, other.deletedList) {}
    HistoryDeleteItems& operator=(const HistoryDeleteItems& other)
    {
        type = heItemsDeleted;
        pDrawn = other.pDrawn;
        deletedList = other.deletedList;
        hidden = other.hidden;
        return *this;
    }
    HistoryDeleteItems(HistoryDeleteItems&& other) : HistoryDeleteItems(other.pDrawn, other.deletedList) {}
    HistoryDeleteItems& operator=(const HistoryDeleteItems&& other)
    {
        type = heItemsDeleted;
        pDrawn = other.pDrawn;
        deletedList = other.deletedList;
        hidden = other.hidden;
        return *this;
    }
    DrawnItem* GetDrawable(int index = 0) const override
    {
        return (index <0 || index > deletedList.size()  || (*pDrawn)[deletedList[index] ].isDeleted) ? nullptr : &(*pDrawn)[deletedList[index]];
    }
};

struct HistoryPasteItem : HistoryItem
{
    DrawnItemList* pDrawn = nullptr;        // pointer to base of array/list of drawn scribbles
    int firstPastedIndex;
    int lastPastedIndex;
    QRect encompassingRect;

    bool Undo() override
    {
        pDrawn->lastDrawnIndex = firstPastedIndex;
        return true;
    }
    bool Redo() override
    {
        pDrawn->lastDrawnIndex = lastPastedIndex;
        return true;
    }
    HistoryPasteItem(DrawnItemList* pdri, QVector<DrawnItem>& pastedList) :
        HistoryItem(), pDrawn(pdri)
    {
        type = heItemsPasted;
        isSaveable = true;
        pDrawn->Add(pastedList, encompassingRect);
    }

    HistoryPasteItem(HistoryPasteItem& other)
    {
        *this = other;
    }
    HistoryPasteItem& operator=(const HistoryPasteItem& other)
    {
        type = heItemsPasted;
        isSaveable = true;
        pDrawn = other.pDrawn;
        firstPastedIndex = other.firstPastedIndex;
        lastPastedIndex = other.lastPastedIndex;
        encompassingRect = other.encompassingRect;
        return *this;
    }
    HistoryPasteItem(HistoryPasteItem&& other)
    {
        *this = other;
    }
    HistoryPasteItem& operator=(const HistoryPasteItem&& other)
    {
        type = heItemsPasted;
        isSaveable = true;
        pDrawn = other.pDrawn;
        firstPastedIndex = other.firstPastedIndex;
        lastPastedIndex = other.lastPastedIndex;
        encompassingRect = other.encompassingRect;
        return *this;
    }

    bool IsSaveable() const override
    {
        int n = 0;
        for (int i = firstPastedIndex; i <= lastPastedIndex; ++i)
            if ((*pDrawn)[i].isDeleted)
                ++n;
        return n < pDrawn->Size();
    }
    DrawnItem* GetDrawable(int index = 0) const override 
    { 
        if(index < 0)
            return nullptr; 
        index += firstPastedIndex;
        if (index > lastPastedIndex)
            return false;
        return (*pDrawn)[index].isDeleted ? nullptr : &(*pDrawn)[index];
    }

    QRect Area() const override { return encompassingRect; }
};

struct HistoryReColorItem : HistoryItem
{
    DrawnItemList* pDrawn = nullptr;        // pointer to base of array/list of drawn scribbles
    IntVector selectedList;             // indexes  to elements in '*pDrawn'
    QVector<MyPenKind> penKindList;     // colors for elements in selectedList
    MyPenKind pk;

    bool Undo() override
    {
        for (int i = 0; i < selectedList.size(); ++i)
            (*pDrawn)[selectedList[i]].penKind = penKindList[i];
        return true;
    }
    bool Redo() override
    {
        for (int i = 0; i < selectedList.size(); ++i)
        {
            penKindList[i] = (*pDrawn)[ selectedList[i] ].penKind;
            (*pDrawn)[selectedList[i]].penKind = pk;
        }
        return true;
    }
    HistoryReColorItem(DrawnItemList* pdri, MyPenKind pk, IntVector &selectedList) : HistoryItem(), pDrawn(pdri), pk(pk), selectedList(selectedList)
    { 
        type = heRecolor; 
        Redo();
    }

    HistoryReColorItem(HistoryReColorItem& other)
    { 
        *this = other;
    }

    HistoryReColorItem& operator=(const HistoryReColorItem& other)
    {
        type = heRecolor;
        pDrawn = other.pDrawn;
        selectedList = other.selectedList;
        penKindList = other.penKindList;
        pk = other.pk;
        return *this;
    }

    HistoryReColorItem(HistoryReColorItem&& other) : HistoryReColorItem(other.pDrawn, other.pk, other.selectedList)
    {
        *this = other;
    }

    HistoryReColorItem& operator=(const HistoryReColorItem&& other)
    {
        type = heRecolor;
        pDrawn = other.pDrawn;
        selectedList = other.selectedList;
        penKindList = other.penKindList;
        pk = other.pk;
        return *this;
    }
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
 *      _lastItem: index of the the last history item adedd
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
    HistoryItem     *_historyItem;       // for drawable and other elements

    DrawnItemList       _drawnItems;          // all items drawn on screen
    QVector<DrawnItem>  _copiedItems;    // copy items into this list for pasting anywhere even in newly opened documents

    QVector<HistoryItem*> _items;
    int _lastItem = -1;                 // in _items, index of last history item, -1: no such item

    IntVector _nSelectedItemsList;   // indices into '_drawnItems', set when we want to delete a bunch of items all together
    QRect _selectionRect;               // encompassing rectangle for selected items used for paste operation


    bool _modified = false;

    int _index = -1;            // start writing undo records from here


    bool _redoAble = false;      // set to true if no new drawing occured after an undo

    HistoryItem * _AddItem(HistoryItem* p)
    {
        if (++_lastItem == _items.size())
            _items.push_back(p);
        else
        {
            _items[_lastItem] = p;  // and delete above t his
            for (int j = _lastItem + 1; j < _items.size(); ++j)
            {
                delete _items[j];
                _items[j] = nullptr;
            }
        }
        _redoAble = false;
        return _items[_lastItem];
    }

    int _GetStartIndex()        // find first drawable item in '_items' after a clear screen 
    {
        if (_lastItem < 0)
            return -1;

        int i = _lastItem;
        for (; i >= 0; --i)
            if (_items[i]->type == heVisibleCleared)
                return i + 1;
            // there were no iems to show
        return 0;
    }

    bool _IsSaveable(HistoryItem &item)         // when n drawables in it are deleted
    {
        if (!item.isSaveable)
            return false;

        if (item.type == heScribble || item.type == heEraser)
        {
            HistoryDrawItem& hdi = dynamic_cast<HistoryDrawItem &>(item);
            return !_drawnItems[hdi.drawnIndex].isDeleted;
        }
        HistoryPasteItem& hpi = dynamic_cast<HistoryPasteItem&>(item);
        bool res = true;
        for(int i = hpi.firstPastedIndex; i <= hpi.lastPastedIndex; ++i)
            res &=  !_drawnItems[ i ].isDeleted;
        return res;
    }

    bool _IsSaveable(int i) 
    {
        return _IsSaveable(*_items[i]);
    }

public:
    History() {}
    ~History()
    {
        for (int i = 0; i < _items.size(); ++i)
            delete _items[i];
    }
    void clear()
    {
        _lastItem = -1;

        for (int i = 0; i < _items.size(); ++i)
            delete _items[i];
        _items.clear();

        _drawnItems.Clear();
        _redoAble = false;
        _modified = false;
    }
    int size() const { return _items.size(); }

    const qint32 MAGIC_ID = 0x53414d57; // "SAMW" - little endian !! MODIFY this and Save() for big endian processors!

    bool Save(QString name)
    {
        QFile f(name);
        f.open(QIODevice::WriteOnly);

        if (!f.isOpen())
            return false;   // can't write file

        QDataStream ofs(&f);
        ofs << MAGIC_ID;
           // only save after last clear screen op.
        for (int i = _GetStartIndex(); i <= _lastItem; ++i)
        {
            auto dt = _items[i];
            // do not save canvas movement or background image events
            if (ofs.status() != QDataStream::Ok)
                return false;

            if (dt->type == heItemsPasted)           // then save all visible pasted items
            {
                HistoryPasteItem& hpi = dynamic_cast<HistoryPasteItem&>(*dt);

                for (int i = hpi.firstPastedIndex; i <= hpi.lastPastedIndex; ++i)
                    if (!_drawnItems[i].isDeleted)
                        ofs << _drawnItems[i];
            }
            else
            {
                HistoryDrawItem& hpi = dynamic_cast<HistoryDrawItem&>(*dt);
                if (_IsSaveable(hpi))
                    ofs << _drawnItems[hpi.drawnIndex];
            }
        }
        _modified = false;
        return true;
    }

    int Load(QString name)  // returns _ites.size() when Ok, -items.size()-1 when read error
    {
        QFile f(name);
        f.open(QIODevice::ReadOnly);
        if (!f.isOpen())
            return -1;

        QDataStream ifs(&f);
        qint32 id;
        ifs >> id;
        if (id != MAGIC_ID)
            return 0;

        _items.clear();
        _drawnItems.Clear();
        _lastItem = -1;

        DrawnItem di;
        int i = 0;
        while (!ifs.atEnd())
        {
            ifs >> di;
            if (ifs.status() != QDataStream::Ok)
                return -_items.size() - 1;

            ++i;

            addDrawnItem(di);
        }
        _modified = false;
        return _lastItem = _items.size() - 1;
    }

    bool IsModified() const { return _modified; }
    bool CanUndo() const { return _lastItem >= 0; }
    bool CanRedo() const { return _redoAble; }

//--------------------- AddItems ------------------------------------------

    HistoryItem *addDrawnItem(DrawnItem &itm)           // may be after an undo, so
    {                                                  // delete all scribbles after the last visible one (items[lastItem].drawnIndex)
        HistoryDrawItem* p = new HistoryDrawItem(itm, &_drawnItems);
        return _AddItem(p);
    }

    HistoryItem *addDeleteItems()
    {
        if (!_nSelectedItemsList.size())
            return nullptr;          // do not add an empty list
        HistoryDeleteItems* p = new HistoryDeleteItems(&_drawnItems, _nSelectedItemsList);
        return _AddItem(p);
    }

    HistoryItem *addPastedItems( )
    {
        if (!_copiedItems.size())
            return nullptr;          // do not add an empty list

        HistoryPasteItem *p = new HistoryPasteItem(&_drawnItems, _copiedItems);
        return _AddItem(p);
    }
    
    HistoryItem* addClearScreen()
    {
        HistoryClearVisible* p= new HistoryClearVisible();
        return _AddItem(p);
    }

    void addOriginChanged(QPoint &origin)       // merge successive position changes
    {
        HistoryTopLeftChangedItem* p;
        if (_lastItem >= 0 && _items[_lastItem]->type == heTopLeftChanged)
        {
            p = dynamic_cast<HistoryTopLeftChangedItem*>(_items[_lastItem]);
            p->topLeft = origin;
        }
        else                                  // new event
        {
            p = new HistoryTopLeftChangedItem(origin);
            _AddItem(p);
        }
    }

    HistoryItem* addRecolor(MyPenKind pk)
    {
        if (!_nSelectedItemsList.size())
            return nullptr;          // do not add an empty list

        HistoryReColorItem* p = new HistoryReColorItem(&_drawnItems, pk, _nSelectedItemsList);
        return _AddItem(p);
    }

//-------------------------------------------------------------------------
    DrawnItem* DrawnItemAt(int index)
    {
        return &_drawnItems[index];
    }

    int SetFirstItemToDraw()
    {
        return _index = _GetStartIndex();    // sets _index to first item after last clear screen
    }

    QPoint BeginUndo()        // returns top left after undo 
    {
        QPoint tl(1,1);            // top left before last step ( (1,1) - not set)

        if (_lastItem >= 0)
        {
            _index = _GetStartIndex();

            _items[_lastItem]->Undo();

            if (_items[_lastItem]->type == heTopLeftChanged)
            {
                int i;
                bool otherTypeOfItem = false;   

                for (i = _lastItem - 1; i >= 0; --i)
                    if (_items[i]->type == heTopLeftChanged) // previous change position found
                    {
                        tl = dynamic_cast<HistoryTopLeftChangedItem*>(_items[i])->topLeft;
                        break;
                    }
                    else if (_items[i]->type == heVisibleCleared)
                    {
                        tl = QPoint(0, 0);
                        break;
                    }
                if(i < 0)       // no movement before this: move to (0, 0)
                    tl = QPoint(0, 0);
            }
            --_lastItem;

            _redoAble = true;
            _modified = true;
        }
        else
            _redoAble = false;

        return tl;
    }
    HistoryItem* GetOneStep()
    {
        if (_lastItem < 0 || _index > _lastItem)
            return nullptr;

        return  _items[_index++];
    }

    HistoryItem* Redo()   // returns item to redo
    {
        if (!_redoAble)
            return nullptr;
        if (_lastItem == _items.size() - 1)
        {
            _redoAble = false;
            return nullptr;
        }

        HistoryItem* phi = _items[++_lastItem];
        phi->Redo();

        _redoAble = _lastItem < _items.size() - 1;

        return phi;
    }

    int CollectItemsInside(QRect rect)
    {

        _nSelectedItemsList.clear();

        _selectionRect = QRect();     // minimum size of selection
            // just check scribbles after the last clear screen
        for(int i = _GetStartIndex(); i <= _lastItem; ++i)
        {
            const HistoryItem item = * _items[i];
            const DrawnItem* pdrni = item.GetDrawable();
            if (pdrni && (rect.contains(item.Area(), true)))    // union of rects in a pasted item
            {
                int j = 0;
                do
                {
                   _nSelectedItemsList.push_back(i+j);  // index in _drawnItemList more than one when items pasted
                } 
                while (pdrni = item.GetDrawable(++j));

                _selectionRect = _selectionRect.united(item.Area() );
            }
        }
        return _nSelectedItemsList.size();
    }

    void CopySelected()      // copies selected scribbles into array. origin will be relative to (0,0)
    {
        if (!_nSelectedItemsList.isEmpty())
        {
            DrawnItem ditem;
            _copiedItems.clear();
            for (auto i : _nSelectedItemsList)
            {
                ditem = _drawnItems[i];
                ditem.rect.translate( -_selectionRect.topLeft() ) ;
                for( QPoint &p : ditem.points)
                    p -= _selectionRect.topLeft();;
                _copiedItems.push_back(ditem);
            }
            _selectionRect.translate(-_selectionRect.topLeft());
        }
    }
    const QVector<DrawnItem>& CopiedItems() const { return _copiedItems;  }
    int CopiedCount() const { return _copiedItems.size();  }

    const QRect& EncompassingRect() const { return _selectionRect; }

    const QVector<int> &Selected() const { return _nSelectedItemsList;  }
};
