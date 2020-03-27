#pragma once

struct DrawnItem    // stores the scribbled lines from pen down to pen up
{                   // or a screen erease event (when points are empty)
    QColor penColor;
    int penWidth;
    QVector<QPoint> points;
    void clear() { points.clear(); }
};

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
        for (int i = _IndicesOfClearScreen.size() - 1; i >= 0; --i)
            if (_IndicesOfClearScreen[i] < _lastItem)
                return i;
        return 0;
    }
public:
    void clear() { _lastItem = -1; _items.clear(); _IndicesOfClearScreen.clear(), _redoAble = false; }
    int size() const { return _items.size(); }
    bool CanUndo() const { return _lastItem >= 0; }
    bool CanRedo() const { return _redoAble; }

    void push_back(DrawnItem& itm)
    {
        _redoAble = false;
        if (++_lastItem == _items.size())
            _items.push_back(itm);
        else
            _items[_lastItem] = itm;
    }

    void ClearScreen() { push_back(_clearItem); }

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
    const DrawnItem* UndoOneStep()
    {
        if (_index > _lastItem)
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


