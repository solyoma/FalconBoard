#pragma once

#include <QFile>
#include <QDataStream>
#include <QIODevice>

enum HistEvent {
    heNone,
    heScribble,        // series of points from start to finish of scribble
    heEraser,          // eraser used
    heVisibleCleared,  // visible image erased
    heCanvasMoved,     // canvas moved: new top left coordinates in points[0]
    heBackgroundLoaded,     // an image loaded into _background
    heBackgroundUnloaded,   // image unloaded from background
    heItemsDeleted     // store the list of items deleted in this event
                };
enum MyPenKind { penNone, penBlack, penRed, penGreen, penBlue, penEraser };

struct DrawnItem    // stores the freehand line strokes from pen down to pen up
{                   // or a screen erease event (when 'points' is empty)
    DrawnItem(HistEvent he = heScribble) noexcept : histEvent(he) {}
    DrawnItem(const DrawnItem& di)  { *this = di; }
    DrawnItem(const DrawnItem&& di) noexcept { *this = di; }
    DrawnItem& operator=(const DrawnItem& di)
    {
        histEvent = di.histEvent;
        penKind = di.penKind;
        penWidth = di.penWidth;
        points = di.points;
        tl = di.tl;
        br = di.br;
        return *this;
    }

    DrawnItem& operator=(const DrawnItem&& di)  noexcept
    {
        histEvent = di.histEvent;
        penKind = di.penKind;
        penWidth = di.penWidth;
        points = di.points;
        tl = di.tl;
        br = di.br;
        return *this;
    }

    HistEvent histEvent = heVisibleCleared;
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
        return false;
        if (p == p1)
            return true;    // nullvector may point in any direction :)

        QPointF vfp = p - p1,  // not (0.0, 0.0)
            vfpp = p1 - p2;

            // the two vectors point in the same direction?
        return (vfp.x() != 0 && vfpp.x() != 0) ? ((vfp.y() / vfp.x() - vfpp.y() / vfpp.x()) < 1e-3) :
                    ((vfp.y() != 0 && vfpp.y() != 0) ? (abs((vfp.x() / vfp.y() - vfpp.x() / vfpp.y())) < 1e-3) :
                        (vfp.x() == 0 && vfpp.x() == 0 && vfp.y() == 0 && vfpp.y() == 0));
    }

    void add(QPoint p)
    {
        if (tl.x() > p.x()) tl.setX(p.x());
        if (tl.y() > p.y()) tl.setY(p.y());
        if (br.x() < p.x()) br.setX(p.x());
        if (br.y() < p.y()) br.setY(p.y());
        int n = points.size() - 1;      // if a point just extends the line in the same direction 
                                        // as the previoud point was from the one before it
                                        // then do not add a new point, jast modify the coordintes
        if(n < 0  )
            points.push_back(p);
        else                      // we need at least one point already in the array
        {
            if( (n==0 && (IsExtension(p, points[n])) ) || (n > 0 && IsExtension(p, points[n], points[n-1])) )  // then the two vector points in the same direction
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

};

inline QDataStream& operator<<(QDataStream& ofs, const DrawnItem& di)
{
    ofs << (qint32)di.histEvent << (qint32)di.penKind << (qint32)di.penWidth;
    ofs << (qint32)di.points.size();
    for (auto pt : di.points)
        ofs << (qint32)pt.x() << (qint32)pt.y();
    return ofs;
}

inline QDataStream& operator>>(QDataStream& ifs, DrawnItem& di)
{
    qint32 n;
    ifs >> n; di.histEvent = static_cast<HistEvent>(n);
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
    HistEvent histEvent = heNone;
    int drawnIndex = -1;                // index in '_drawnItems'
    QRect deleteRect;                   // all scribbles to delete are inside this rectangle
    QVector<int> deletedList;           // indices into History::_dranIitems of deleted 
    QString sBackgroundImageName;       // heBackgroundLoaded
    bool isSaveable = false;            // can be saved into file (scriible and erase only?
    bool processed = false;             // e.g. a delete was already processed

    HistoryItem() {}
    HistoryItem(const HistoryItem & other) { *this = other;  }
    HistoryItem(const HistoryItem && other) { *this = other; }
    HistoryItem& HistoryItem::operator=(const HistoryItem& other) 
    { 
        histEvent   = other.histEvent;
        drawnIndex  = other.drawnIndex;
        deleteRect  = other.deleteRect;
        deletedList = other.deletedList;
        isSaveable  = other.isSaveable;
        sBackgroundImageName = other.sBackgroundImageName;
        return *this;
    }
    HistoryItem& HistoryItem::operator=(const HistoryItem&& other)
    {
        histEvent = other.histEvent;
        drawnIndex = other.drawnIndex;
        deleteRect = other.deleteRect;
        deletedList = other.deletedList;
        isSaveable = other.isSaveable;
        sBackgroundImageName = other.sBackgroundImageName;
        return *this;
    }


    void clear()
    {
        deletedList.clear();
        sBackgroundImageName.clear();
        histEvent = heNone;
        drawnIndex = -1;
    }
    void SetEvent(HistEvent he)
    {
        histEvent = he;
        isSaveable = (he == heScribble || he == heEraser);
        deletedList.clear();
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
    HistoryItem     _historyItem;       // used internally for non drawable elements

    QVector<HistoryItem> _items;

    QVector<DrawnItem> _drawnItems;     // all items drawn on screen
    QVector<int> _nSelectedItems;       // indices into '_drawnItems', set when we want to delete a bunch of items at once

    QRect _selectionRect;               // select items completely inside this rectangle

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
            if (_items[i].histEvent == heVisibleCleared)
                return i + 1;
            // there were no iems to show
        return 0;
    }

    inline bool _ValidDrawable(int index, bool* isClearEvent = nullptr)
    {
        const HistoryItem& item = _items[index];
        return ValidDrawable(item, isClearEvent);

    }

    DrawnItem* _DrawnItemFor(int index)
    {
        const HistoryItem& item = _items[index];
        DrawnItem clearItem;            // for clear screen items

        bool isClear;
        if (ValidDrawable(item, &isClear))
            return &_drawnItems[item.drawnIndex];

        return item.histEvent == heVisibleCleared ? &clearItem : nullptr;
    }

    bool _IsSaveable(const HistoryItem &item) const
    {
        return item.isSaveable ? (item.isSaveable && &_drawnItems[item.drawnIndex].isDeleted) : false;
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
            //di.histEvent = (HistEvent)(((int)di.histEvent) + 1);
            // /DEBUG            


            push_back(di);
        }
        _modified = false;
        return _lastItem = _items.size() - 1;
    }

    bool IsModified() const { return _modified; }
    bool CanUndo() const { return _lastItem >= 0; }
    bool CanRedo() const { return _redoAble; }

    void push_back(DrawnItem itm)           // and save it in history too
    {
        // sav drawn item
        _redoAble = false;
        if (++_lastDrawnIndex == _drawnItems.size())
            _drawnItems.push_back(itm);
        else
            _drawnItems[_lastDrawnIndex] = itm;

        _historyItem.SetEvent(itm.histEvent); // scribble or eraser
        _historyItem.drawnIndex = _lastDrawnIndex;
        _historyItem.processed = false;
        add(_historyItem);
    }

    void add(HistoryItem& hi)
    {
        if (++_lastItem == _items.size())
            _items.push_back(hi);
        else
            _items[_lastItem] = hi;
        _modified = true;
    }

    void ClearScreen()
    {
        _historyItem.SetEvent(heVisibleCleared);
        add(_historyItem);
    }

    bool ValidDrawable(const HistoryItem& item, bool* isClearEvent = nullptr)
    {
        bool res = _IsSaveable(item);
        if (isClearEvent)
            *isClearEvent = (item.histEvent == heVisibleCleared); // still may be not drawable

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

    void BeginUndo()        // set position back one section
    {
        if (_lastItem >= 0)
        {
            _index = _GetStartIndex();
                // if the last step was a deletion first redo that
            if (_items[_lastItem].histEvent == heItemsDeleted)
                UnDeleteItemsFor(_items[_lastItem]);
            --_lastItem;

            _redoAble = true;
            _modified = true;
        }
        else
            _redoAble = false;
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
        if (!_redoAble)
            _modified = false;

        return phi;
    }

    int CollectItemsInside(QRect rect)
    {
        _selectionRect = rect;

        _nSelectedItems.clear();
            // just check scribbles after the last clear screen
        for(int i = _GetStartIndex(); i < _lastItem; ++i)
        {
            const HistoryItem& item = _items[i];
            if (ValidDrawable(item) && (rect.contains(QRect(_drawnItems[item.drawnIndex].tl, _drawnItems[item.drawnIndex].br), true)))
                _nSelectedItems.push_back(item.drawnIndex); // index in _drawnItemList
        }
        return _nSelectedItems.size();
    }

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
            _historyItem.deleteRect = _selectionRect;
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
            _nSelectedItems.clear();
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
