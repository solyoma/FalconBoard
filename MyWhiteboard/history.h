#pragma once

#include <QFile>
#include <QDataStream>
#include <QIODevice>

enum HistEvent { heScribble,        // series of points from start to finish of scribble
                 heEraser,          // eraser used
                 heVisibleCleared,  // visible image erased
                 heCanvasMoved,     // canvas moved: new top left coordinates in points[0]
                 heImageLoaded,     // an image loaded into _background
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

    HistEvent histEvent = heScribble;
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
 * Class for storing history of editing
 *  contains a list of items drawn on screen, 
 *      screen deletion, item deleteion, canvas movement
 * GLOBALS:
 * RETURNS:
 * REMARKS: -
 *-------------------------------------------------------*/
class History  // stores all drawing sections and keeps track of undo and redo
{
    DrawnItem _clearItem;

    QVector<DrawnItem> _items;
    QVector<int> _nSelectedItems;   // indices into _items, set when we want to delete a bunch of items at once

    int _index = -1;        // start writing undo records from here
    int _lastItem = -1;   // index of last item drawn on screen -1: no such item
                         // next line will be added at index (lastItem+1)
                         // when smaller than items.size()-1 and no new section is added
                         // then redu is possible starting at (lastItem+1)
                         // when new sectiom added after an undo it cannot be re-done
                         // and further UNDOs can't recreate the lines lost this way
    bool _modified = false;

    QVector<int> _IndicesOfClearScreen;  // stores index positions wher the screen was cleared
                                        // start drawing for undo from the last element of this
                                        // which is smaller than or equal to 'lastItem'

    bool _redoAble = false;      // set to true if no new drawing occured after an undo

    int _GetStartIndex()
    {
        if (_lastItem < 0)
            return -1;

        for (int i = _IndicesOfClearScreen.size() - 1; i >= 0; --i)
            if (_IndicesOfClearScreen[i] < _lastItem)
                return i;
        return 0;
    }
public:
    void clear() 
    { 
        _lastItem = -1; 
        _items.clear(); 
        _IndicesOfClearScreen.clear();
        _redoAble = false; 
        _modified = false; 
    }
    int size() const { return _items.size(); }

    const qint32 MAGIC_ID = 0x53414d57; // "SAMW" - little endian
    bool Save(QString name)
    {
        QFile f(name);
        f.open(QIODevice::WriteOnly);

        if (!f.isOpen())
            return false;   // can't write file

        QDataStream ofs(&f);
        ofs << MAGIC_ID;
        for (int i =0; i <= _lastItem; ++i) 
        {   
            auto dt = _items[i];
            // do not save canvas movement or background image events
            if (ofs.status() != QDataStream::Ok)
                return false;

            if (dt.histEvent == heScribble || dt.histEvent == heVisibleCleared)
                ofs << dt;
        }
        _modified = false;
        return true;
    }

    int Load(QString name)  // returns _ites.size() when Ok, -items.size()-1 when read error
    {
        QFile f(name);
        f.open(QIODevice::ReadOnly);
        if (!f.isOpen())    
            return - 1;

        QDataStream ifs(&f);
        qint32 id;
        ifs >> id;
        if (id != MAGIC_ID)
            return 0;

        _items.clear();
        _IndicesOfClearScreen.clear();
        DrawnItem di;
        int i = 0;
        while (!ifs.atEnd())
        {
            ifs >> di;
            if (ifs.status() != QDataStream::Ok)
                return -_items.size() - 1;

            if (di.histEvent == heVisibleCleared)
                _IndicesOfClearScreen.push_back(i);
            ++i;
            _items.push_back(di);
        }
        _modified = false;
        return _lastItem =_items.size()-1;
    }

    bool IsModified() const { return _modified; }
    bool CanUndo() const { 
        return _lastItem >= 0; 
    }
    bool CanRedo() const { return _redoAble; }

    void push_back(DrawnItem itm)
    {
        _redoAble = false;
        if (++_lastItem == _items.size())
            _items.push_back(itm);
        else
            _items[_lastItem] = itm;
        _modified = true;
    }

    void ClearScreen() { push_back(_clearItem); }

    void SetFirstItemToDraw()
    {
        _index = _GetStartIndex();    // sets _index to first item after last clear screen
    }

    void BeginUndo()        // set position back one section
    {
        if (_lastItem >= 0)
        {
            _index = _GetStartIndex();
            _redoAble = true;
            --_lastItem;
            _modified = true;
        }
        else
            _redoAble = false;
    }
    const DrawnItem* GetOneStep()
    {
        if (_lastItem < 0 || _index > _lastItem)
            return nullptr;
        return &_items[_index++];
    }

    const DrawnItem* Redo()   // returns item to redo
    {
        if (!_redoAble)
            return nullptr;
        if (_lastItem == _items.size() - 1)
        {
            _redoAble = false;
            return nullptr;
        }
        const DrawnItem* pdrni = &_items[++_lastItem];
        _redoAble = _lastItem < _items.size() - 1;
        if(!_redoAble)
            _modified = false;

        return pdrni;
    }

    int CollectItemsInside(QRect rect)
    {
        _nSelectedItems.clear();
        for (int i=0; i < _items.size(); ++i)
        {
            if (rect.contains(QRect(_items[i].tl, _items[i].br), true))
                _nSelectedItems.push_back(i);
        }
        return _nSelectedItems.size();
    }

    void DeleteItemsInList()
    {
        if (_nSelectedItems.isEmpty())
            return;
        QVector<DrawnItem>::iterator itB = _items.begin()+ _nSelectedItems[0],
             itE = _items.begin() + _nSelectedItems[_nSelectedItems.size()-1] + 1;
        _items.erase(itB, itE);
        if (_lastItem >= _items.size())
            _lastItem = _items.size() - 1;
        if (_lastItem > _nSelectedItems[0] && _lastItem < _items.size()) // inside
            _lastItem = _nSelectedItems[0];
        _nSelectedItems.clear();
    }
};


