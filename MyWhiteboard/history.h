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
    heItemsPasted       // draw events pasted, store undo/redo count
                };
enum MyPenKind { penNone, penBlack, penRed, penGreen, penBlue, penEraser };

struct DrawnItem    // stores the freehand line strokes from pen down to pen up
{                   // or a screen erease event (when 'points' is empty)
    DrawnItem(HistEvent he = heScribble) noexcept : _type(he) {}
    DrawnItem(const DrawnItem& di)  { *this = di; }
    DrawnItem(const DrawnItem&& di) noexcept { *this = di; }
    DrawnItem& operator=(const DrawnItem& di)
    {
        _type = di._type;
        penKind = di.penKind;
        penWidth = di.penWidth;
        points = di.points;
        tl = di.tl;
        br = di.br;
        return *this;
    }

    DrawnItem& operator=(const DrawnItem&& di)  noexcept
    {
        _type = di._type;
        penKind = di.penKind;
        penWidth = di.penWidth;
        points = di.points;
        tl = di.tl;
        br = di.br;
        return *this;
    }

    HistEvent _type = heVisibleCleared;
    bool isDeleted = false;

    MyPenKind penKind = penBlack;
    int penWidth;
    QVector<QPoint> points;         // coordinates are relative to logical origin (0,0) => canvas coord = points[i] - origin
    QPoint tl, br;                  // top left-bttom right coordinates of encapsulating rectangle
                                    // not saved on disk, recreated on read

    void clear() 
    { 
        points.clear(); 
        tl = QPoint(0x7FFFFFFF, 0x7FFFFFFF);
        br = QPoint(-1, -1);
    }

    static bool IsExtension(QPoint& p, QPoint& p1, QPoint& p2 = QPoint()) // vectors p->p1 and p1->p are parallel?
    {
        //return false;       // DEBUG as it is not working yet

        if (p == p1)
            return true;    // nullvector may point in any direction :)

        QPoint vp = p - p1,  // not (0, 0)
            vpp = p1 - p2;

            // the two vectors point in the same direction when vpp.y()/vppx == vp.y()/vp(x)
            // i.e to avoid checking for zeros in divison: 
        return vpp.y() * vp.x() == vp.y() * vpp.x();
    }

    void add(QPoint p)
    {
        if (tl.x() > p.x()) tl.setX(p.x());
        if (tl.y() > p.y()) tl.setY(p.y());
        if (br.x() < p.x()) br.setX(p.x());
        if (br.y() < p.y()) br.setY(p.y());
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

    bool intersects(const QRect& rect) const
    {
        return rect.intersects(QRect(tl, br - QPoint(1,1)));
    }

    void Translate(QPoint dr)
    {
        for (int i = 0; i < points.size(); ++i)
            points[i] = points[i] + dr;
        tl = tl + dr;
        br = br + dr;
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
    int drawnIndex = -1;                // index in 'History::_drawnItems'
    QVector<int> deletedList;           // indices into History::_dranIitems of deleted 
    QString sBackgroundImageName;       // heBackgroundLoaded
    bool isSaveable = false;            // can be saved into file (scriible and erase only?
    bool processed = false;             // e.g. a delete was already processed

    HistoryItem() {}
    HistoryItem(const HistoryItem & other) { *this = other;  }
    HistoryItem(const HistoryItem && other) { *this = other; }
    HistoryItem& HistoryItem::operator=(const HistoryItem& other) 
    { 
        _type   = other._type;
        drawnIndex  = other.drawnIndex;
        deletedList = other.deletedList;
        isSaveable  = other.isSaveable;
        sBackgroundImageName = other.sBackgroundImageName;
        return *this;
    }
    HistoryItem& HistoryItem::operator=(const HistoryItem&& other)
    {
        _type = other._type;
        drawnIndex = other.drawnIndex;
        deletedList = other.deletedList;
        isSaveable = other.isSaveable;
        sBackgroundImageName = other.sBackgroundImageName;
        return *this;
    }

    void clear()
    {
        deletedList.clear();
        sBackgroundImageName.clear();
        _type = heNone;
        drawnIndex = -1;
    }
    void SetEvent(HistEvent he)
    {
        _type = he;
        isSaveable = (he == heScribble || he == heEraser);
        deletedList.clear();
    }

    void SetTopLeft(QPoint tl)  // for heTopLeftChanged
    {
        deletedList.clear();
        deletedList.push_back(tl.x());
        deletedList.push_back(tl.y());
    }

    QPoint AsTopLeft()
    {
        return QPoint(deletedList[0], deletedList[1]);
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

    QVector<DrawnItem> _drawnItems;     // all items drawn on screen
    QVector<int> _nSelectedItems;       // indices into '_drawnItems', set when we want to delete a bunch of items all together
    QVector<DrawnItem> _copiedItems;    // copy items into this list for pasting anywhere even in newly opened documents

    QRect _selectionRect;               // encompassing rectangle for selected items used for paste operation

    bool _modified = false;

    int _index = -1;            // start writing undo records from here

    int _lastItem = -1;         // in _items, index of last history item, -1: no such item
    int _lastDrawnIndex = -1;   // in _drawnItems

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

    inline bool _ValidDrawable(int index)
    {
        const HistoryItem& item = _items[index];
        return ValidDrawable(item);

    }

    bool _IsSaveable(const HistoryItem &item) const
    {
        return item.isSaveable ? (item.isSaveable && !_drawnItems[item.drawnIndex].isDeleted) : false;
    }
    bool _IsSaveable(int i) const
    {
        return _IsSaveable(_items[i]);
    }
public:
    void clear()
    {
        _lastItem = -1;
        _lastDrawnIndex = -1;
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

            if (_IsSaveable(dt) )
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
        _lastDrawnIndex = -1;

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

    HistoryItem *addDrawnItem(DrawnItem itm)           // may be after an undo, so
    {                                                  // delete all scribbles after the last visible one (items[lastItem].drawnIndex)
        // save drawn item
        _redoAble = false;
        if (_lastItem >= 0 && (_items[_lastItem]._type == heScribble || _items[_lastItem]._type == heEraser)) // last item is drawable
        {
            _lastDrawnIndex = _items[_lastItem].drawnIndex;
        }
        if (++_lastDrawnIndex == _drawnItems.size())
            _drawnItems.push_back(itm);
        else
            _drawnItems[_lastDrawnIndex] = itm;

        _historyItem.SetEvent(itm._type); // scribble or eraser
        _historyItem.drawnIndex = _lastDrawnIndex;
        _historyItem.processed = false;     // if this contained deleted items, now it doesn't
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

        _nSelectedItems.clear();

        _selectionRect = QRect();     // minimum size of selection
        QRect r;
            // just check scribbles after the last clear screen
        for(int i = _GetStartIndex(); i <= _lastItem; ++i)
        {
            const HistoryItem& item = _items[i];
            r = QRect(_drawnItems[item.drawnIndex].tl, _drawnItems[item.drawnIndex].br);
            if (ValidDrawable(item) && (rect.contains(r, true)))
            {
                _nSelectedItems.push_back(item.drawnIndex); // index in _drawnItemList
                _selectionRect = _selectionRect.united(r);
            }
        }
        return _nSelectedItems.size();
    }

    void CopySelected()      // copies selected scribbles int array origin will be relative to (0,0)
    {
        if (!_nSelectedItems.isEmpty())
        {
            DrawnItem item;
            _copiedItems.clear();
            for (auto i : _nSelectedItems)
            {
                item = _drawnItems[i];
                item.tl -= _selectionRect.topLeft();
                item.br -= _selectionRect.topLeft();
                for( QPoint &p : item.points)
                    p -= _selectionRect.topLeft();;
                _copiedItems.push_back(item);
            }
            _selectionRect.translate(-_selectionRect.topLeft());
        }
    }
    const QVector<DrawnItem>& CopiedItems() const { return _copiedItems;  }
    int CopiedCount() const { return _copiedItems.size();  }

    const QRect& EncompassingRect() const { return _selectionRect; }

    const QVector<int> &Selected() const { return _nSelectedItems;  }

    void HideDeletedItems(HistoryItem *phi = nullptr) // phi points inside _items
    {
        if (phi && phi->processed)
            return;

        bool newData; 
        newData = phi == nullptr;

        if (newData)     // then new history item
        {
            _historyItem.SetEvent(heItemsDeleted);  // clears deletedList
            _historyItem.deletedList = _nSelectedItems;
            phi = &_historyItem;

            if (_nSelectedItems.isEmpty())          // and do not add it to list
                return;
        }
                                                   // else it was set into phi already
        for (int i : phi->deletedList)              // mark elements
            _drawnItems[i].isDeleted = true;

        if (newData)
        {
            _historyItem.processed = true;
            add(_historyItem);
            //_nSelectedItems.clear();
        }
        else
            phi->processed = true;
    }

    void UnDeleteItemsFor(HistoryItem &hi)
    {
        for (int i : hi.deletedList)
            _drawnItems[i].isDeleted = false;

        hi.processed = false;
    }
};
