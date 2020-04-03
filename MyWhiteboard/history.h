#pragma once

#include <QFile>
#include <QDataStream>
#include <QIODevice>

enum HistEvent { heScribble,        // series of points from start to finish of scribble
                 heEraser,          // eraser used
                 heVisibleCleared,  // visible image erased
                 heCanvasMoved,     // canvas moved: new top left coordinates in points[0]
                 heImageLoaded      // an image loaded into _background
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
        return *this;
    }

    DrawnItem& operator=(const DrawnItem&& di)  noexcept
    {
        histEvent = di.histEvent;
        penKind = di.penKind;
        penWidth = di.penWidth;
        points = di.points;
        return *this;
    }

    HistEvent histEvent = heScribble;
    MyPenKind penKind = penBlack;
    int penWidth;
    QVector<QPoint> points;           // coordinates are relative to logical origin (0,0)


    void clear() { points.clear(); }
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
    QPoint pt;
    di.points.clear();
    ifs >> n; 
    while (n--)
    {
        ifs >> x >> y;
        pt.setX(x); pt.setY(y);
        di.points.push_back(pt);
    }
    return ifs;
}

class History  // stores all drawing sections and keeps track of undo and redo
{
    DrawnItem _clearItem;

    QVector<DrawnItem> _items;
    int _index = -1;        // start writing undo records from here
    int _lastItem = -1;   // index of last item drawn on screen -1: no such item
                         // next line will be added at index (lastItem+1)
                         // when smaller than items.size()-1 and no new section is added
                         // then redu is possible starting at (lastItem+1)
                         // when new sectiom added after an undo it cannot be re-done
                         // and further UNDOs can't recreate the lines lost this way

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
    void clear() { _lastItem = -1; _items.clear(); _IndicesOfClearScreen.clear(), _redoAble = false; }
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
        for (auto dt : _items)
        {                     // do not save canvas movement or background image events
            if (ofs.status() != QDataStream::Ok)
                return false;

            if (dt.histEvent == heScribble || dt.histEvent == heVisibleCleared)
                ofs << dt;
        }
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
        return _lastItem =_items.size()-1;
    }

    bool CanUndo() const { return _lastItem >= 0; }
    bool CanRedo() const { return _redoAble; }

    void push_back(DrawnItem itm)
    {
        _redoAble = false;
        if (++_lastItem == _items.size())
            _items.push_back(itm);
        else
            _items[_lastItem] = itm;
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
        return pdrni;
    }
};


