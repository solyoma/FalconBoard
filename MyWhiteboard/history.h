#pragma once

#include <QFile>
#include <QDataStream>
#include <QIODevice>

enum HistEvent {
    heNone,
    heScribble,        // series of points from start to finish of scribble
    heEraser,          // eraser used
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
    DrawnItem(HistEvent he = heScribble) noexcept : _type(he) {}
    DrawnItem(const DrawnItem& di)  { *this = di; }
    DrawnItem(const DrawnItem&& di) noexcept { *this = di; }
    DrawnItem& operator=(const DrawnItem& di)
    {
        _type = di._type;
        penKind = di.penKind;
        penWidth = di.penWidth;
        points = di.points;
        rect = di.rect;
        return *this;
    }

    DrawnItem& operator=(const DrawnItem&& di)  noexcept
    {
        _type = di._type;
        penKind = di.penKind;
        penWidth = di.penWidth;
        points = di.points;
        rect = di.rect;
        return *this;
    }

    HistEvent _type = heNone;
    bool isDeleted = false;

    MyPenKind penKind = penBlack;
    int penWidth;
    QVector<QPoint> points;         // coordinates are relative to logical origin (0,0) => canvas coord = points[i] - origin
    QRect rect;                     // top left-bttom right coordinates of encapsulating rectangle
                                    // not saved on disk, recreated on read

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
    ofs << (qint32)di._type << (qint32)di.penKind << (qint32)di.penWidth;
    ofs << (qint32)di.points.size();
    for (auto pt : di.points)
        ofs << (qint32)pt.x() << (qint32)pt.y();
    return ofs;
}

inline QDataStream& operator>>(QDataStream& ifs, DrawnItem& di)
{
    qint32 n;
    ifs >> n; di._type = static_cast<HistEvent>(n);
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


/*========================================================
 * One history element
 *-------------------------------------------------------*/
struct HistoryItem
{
    HistEvent _type = heNone;
    bool isSaveable = false;            // can be saved into file (scriible and erase only?
    QRect itemRect;                     // simple dranables: same as their rect, pasted items: unions of those

    int drawnIndex = -1;                // index in 'History::_drawnItems' first drawnable,  OR new 'Left' for 'heTopLeftEvent'
    int lastDrawnIndex = -1;            // index in 'History::_drawnItems' last drawnable OR new 'Top' for 'heTopLeftEvent'
                                        // for single drawnItem both are the same

    QVector<int> deletedList;           // indices into History::_drawnItems
    bool hidden = false;                // e.g. a delete was already hidden

    QString sBackgroundImageName;       // heBackgroundLoaded

    HistoryItem() {}
    HistoryItem(const HistoryItem & other) { *this = other;  }
    HistoryItem(const HistoryItem && other) { *this = other; }
    HistoryItem& HistoryItem::operator=(const HistoryItem& other) 
    { 
        _type   = other._type;
        drawnIndex  = other.drawnIndex;
        lastDrawnIndex  = other.lastDrawnIndex;
        deletedList = other.deletedList;
        isSaveable  = other.isSaveable;
        sBackgroundImageName = other.sBackgroundImageName;
        itemRect = other.itemRect;
        return *this;
    }
    HistoryItem& HistoryItem::operator=(const HistoryItem&& other)
    {
        _type = other._type;
        drawnIndex  = other.drawnIndex;
        lastDrawnIndex  = other.lastDrawnIndex;
        deletedList = other.deletedList;
        isSaveable = other.isSaveable;
        sBackgroundImageName = other.sBackgroundImageName;
        itemRect = other.itemRect;
        return *this;
    }

    void clear()
    {
        deletedList.clear();
        sBackgroundImageName.clear();
        _type = heNone;
        drawnIndex = -1;
        lastDrawnIndex = -1;
    }
    void SetEvent(HistEvent he)   // removes deleted indices
    {
        _type = he;
        isSaveable = (he == heScribble || he == heEraser || heItemsPasted);
        deletedList.clear();
    }

    void SetTopLeft(QPoint tl)  // heTopLeftChanged event
    {
        deletedList.clear();
        drawnIndex      = tl.x();
        lastDrawnIndex  = tl.y();
    }

    QPoint AsTopLeft()
    {
        return QPoint(drawnIndex, lastDrawnIndex);
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
    HistoryItem     _historyItem;       // for drawable and other elements

    QVector<HistoryItem> _items;
    int _lastItem = -1;                 // in _items, index of last history item, -1: no such item

    QVector<DrawnItem> _drawnItems;     // all items drawn on screen
    int _indexLastDrawnItem = -1;       // in _drawnItems

    QVector<int> _nSelectedItemsList;       // indices into '_drawnItems', set when we want to delete a bunch of items all together
    QRect _selectionRect;               // encompassing rectangle for selected items used for paste operation

    QVector<DrawnItem> _copiedItems;    // copy items into this list for pasting anywhere even in newly opened documents

    bool _modified = false;

    int _index = -1;            // start writing undo records from here


    bool _redoAble = false;      // set to true if no new drawing occured after an undo

    int _GetStartIndex()        // find first drawable item in '_items' after a clear screen 
    {
        if (_lastItem < 0)
            return -1;

        int i = _lastItem;
        for (; i >= 0; --i)
            if (_items[i]._type == heVisibleCleared)
                return i + 1;
            // there were no iems to show
        return 0;
    }


    int _AddToDrawnItems(DrawnItem& itm)
    {
        if (++_indexLastDrawnItem == _drawnItems.size())
            _drawnItems.push_back(itm);
        else
            _drawnItems[_indexLastDrawnItem] = itm;
        return _indexLastDrawnItem;
    }


    /*========================================================
     * TASK:    add new item to vector _drawnItem and sets 
     *          _historyItem
     * PARAMS:
     * GLOBALS:
     * RETURNS:
     * REMARKS: - called by addDrawnItem() and addPasteItem()
     *          - does not add '_historyItem' to list
     *          - although sets all of its parameters
     *          - type added is always heScribble or heEraser
     *              so modify it for 'heItemsPasted'
     *-------------------------------------------------------*/
    void _AddNewDrawnItem(DrawnItem& itm) 
    {                                          
        _redoAble = false;                      // get rid of undone items
        if (_lastItem >= 0)                     // 
        {                                       // so "remove" those items by modifying '_indexLastDrawnItem'
            if (_items[_lastItem]._type == heScribble || _items[_lastItem]._type == heEraser) // last item is drawable
                _indexLastDrawnItem = _items[_lastItem].drawnIndex;
            if (_items[_lastItem]._type == heItemsPasted)               // for pasted items
                _indexLastDrawnItem = _items[_lastItem].lastDrawnIndex; // end of pasted stored here
        }
        _AddToDrawnItems(itm);

        _historyItem.SetEvent(itm._type); // scribble or eraser
        _historyItem.drawnIndex = _indexLastDrawnItem;
        _historyItem.lastDrawnIndex = _indexLastDrawnItem;
        _historyItem.hidden = false;     // if this contained deleted items, now it doesn't
        _historyItem.itemRect = itm.rect;
    }

    inline bool _ValidDrawable(int index)
    {
        const HistoryItem& item = _items[index];
        return ValidDrawable(item);

    }

    bool _IsSaveable(const HistoryItem &item) const         // when n drawables in it are deleted
    {
        if (!item.isSaveable)
            return false;
        bool res = true;
        for(int i = item.drawnIndex; i <= item.lastDrawnIndex; ++i)
            res &=  !_drawnItems[item.drawnIndex].isDeleted;
        return res;
    }
    bool _IsSaveable(int i) const
    {
        return _IsSaveable(_items[i]);
    }
public:
    void clear()
    {
        _lastItem = -1;
        _indexLastDrawnItem = -1;
        _items.clear();
        _drawnItems.clear();
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

            if (dt._type == heItemsPasted)           // then save all visible pasted items
            {
                for (int i = dt.drawnIndex; i <= dt.lastDrawnIndex; ++i)
                    if (!_drawnItems[i].isDeleted)
                        ofs << _drawnItems[i];
            }
            else if (_IsSaveable(dt) )
                ofs << _drawnItems[dt.drawnIndex];
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
        _drawnItems.clear();
        _lastItem = -1;
        _indexLastDrawnItem = -1;

        DrawnItem di;
        int i = 0;
        while (!ifs.atEnd())
        {
            ifs >> di;
            if (ifs.status() != QDataStream::Ok)
                return -_items.size() - 1;

            ++i;
            // DEBUG
            //di._type = (HistEvent)(((int)di._type) + 1);
            // /DEBUG            


            addDrawnItem(di);
        }
        _modified = false;
        return _lastItem = _items.size() - 1;
    }

    bool IsModified() const { return _modified; }
    bool CanUndo() const { return _lastItem >= 0; }
    bool CanRedo() const { return _redoAble; }

    HistoryItem *addDrawnItem(DrawnItem &itm)           // may be after an undo, so
    {                                                  // delete all scribbles after the last visible one (items[lastItem].drawnIndex)
        // save drawn item
        _AddNewDrawnItem(itm);
        return add(_historyItem);
    }
            // when paste items first call NewPastedItem(), then
            // add each element pasted with AddToPastedItems()
            // and finish with 'add(void)'
    void NewPastedItem(DrawnItem& itm)
    {
        _AddNewDrawnItem(itm);
        _historyItem._type = heItemsPasted; // modify one set before
    }

    void AddToPastedItem(DrawnItem& itm)  // only call after newPastedItem() !
    {
        _historyItem.lastDrawnIndex = _AddToDrawnItems(itm);
        _historyItem.itemRect = _historyItem.itemRect.united(itm.rect);
    }

    HistoryItem* addPastedItem(DrawnItem& itm)  // only call after newPastedItem()  and items added
    {
        return add(_historyItem);
    }
    DrawnItem* DrawnItemFor(int index)             // items for one scribble
    {
        const HistoryItem& item = _items[index];
        bool isClear;
        if (ValidDrawable(item))
            return &_drawnItems[item.drawnIndex];

        return nullptr;     // only scribbles and erases are stored in _drawnItems
    }


    HistoryItem *add(HistoryItem& hi)
    {
        if (++_lastItem == _items.size())
            _items.push_back(hi);
        else
            _items[_lastItem] = hi;
        _modified = true;
        return &_items[_lastItem];
    }
    HistoryItem* add()
    {
        return add(_historyItem);
    }
    
    HistoryItem* addClearScreen()
    {
        _historyItem.SetEvent(heVisibleCleared);
        return add(_historyItem);
    }

    void addOriginChanged(QPoint &origin)       // merge successive position changes
    {
        if (_lastItem >= 0 && _items[_lastItem]._type == heTopLeftChanged)
        {
            _items[_lastItem].SetTopLeft(origin);
        }
        else                                  // new event
        {
            _historyItem.SetEvent(heTopLeftChanged);
            _historyItem.deletedList.clear();
            _historyItem.SetTopLeft(origin);
            add();
        }
    }

    bool ValidDrawable(const HistoryItem& item)
    {
        bool res = _IsSaveable(item);
        return res;
    }
    DrawnItem* DrawableFor(HistoryItem& item)
    {
        return &_drawnItems[item.drawnIndex];
    }

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
                // if the last step was a deletion first redo that
            if (_items[_lastItem]._type == heItemsDeleted)
                UnDeleteItemsFor(_items[_lastItem]);
                // if the last step ws positioning set the pervious position
            else if (_items[_lastItem]._type == heTopLeftChanged)
            {
                int i;
                bool otherTypeOfItem = false;   

                for (i = _lastItem - 1; i >= 0; --i)
                    if (_items[i]._type == heTopLeftChanged) // previous change position found
                    {
                        tl = QPoint(_items[i].AsTopLeft());
                        break;
                    }
                    else if (_items[i]._type == heVisibleCleared)
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

        return  &_items[_index++];
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

        HistoryItem* phi = &_items[++_lastItem];
        _redoAble = _lastItem < _items.size() - 1;
        //if (!_redoAble)
        //    _modified = false;

        return phi;
    }

    int CollectItemsInside(QRect rect)
    {

        _nSelectedItemsList.clear();

        _selectionRect = QRect();     // minimum size of selection
            // just check scribbles after the last clear screen
        for(int i = _GetStartIndex(); i <= _lastItem; ++i)
        {
            const HistoryItem& item = _items[i];
            if (ValidDrawable(item) && (rect.contains(item.itemRect, true)))    // union of rects in a pasted item
            {
                for (int i = item.drawnIndex; i <= item.lastDrawnIndex; ++i)
                {
                    _nSelectedItemsList.push_back(i); // index in _drawnItemList
                }
                _selectionRect = _selectionRect.united(item.itemRect);
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

    void HideDeletedItems(HistoryItem *phi = nullptr) // phi points inside _items
    {
        if (phi && phi->hidden)
            return;

        bool newData; 
        newData = phi == nullptr;

        if (newData)     // then new history item
        {
            _historyItem.SetEvent(heItemsDeleted);  // clears deletedList
            _historyItem.deletedList = _nSelectedItemsList;
            phi = &_historyItem;

            if (_nSelectedItemsList.isEmpty())          // and do not add it to list
                return;
        }
                                                   // else it was set into phi already
        for (int i : phi->deletedList)             // and just mark elements
            _drawnItems[i].isDeleted = true;

        if (newData)
        {
            _historyItem.hidden = true;
            add(_historyItem);
            //_nSelectedItemsList.clear();
        }
        else
            phi->hidden = true;
    }

    void UnDeleteItemsFor(HistoryItem &hi)
    {
        for (int i : hi.deletedList)
            _drawnItems[i].isDeleted = false;

        hi.hidden = false;
    }
};
