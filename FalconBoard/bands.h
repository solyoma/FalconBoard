#pragma once
#ifndef _BANDS_H
#define _BANDS_H

#include <algorithm>
#include <limits>

#include <QList>
#include <QRect>

class History;
struct HistoryItem;
struct ItemIndex;
using ItemIndexVector = QVector<ItemIndex>;  // ordered by 'zorder'

/*========================================================
* bands on screen
* all items are categorized into bands of height of
* _screenHeight, each band contains indices of each
* element which intersects that band, so one element may
* appear on more than one band(currently 2)
* a band may contain both images and scribbles
*
* Band for any y coord is y / _bandHeight
* ------------------------------------------------------ - */
struct Bands
{
    Bands() {}
    void SetParam(History* pHist, int bandHeight);
    void Add(int ix);    // existing and visible drawnable in pHist->_items[ix]
    void Remove(int ix);
    int  ItemsStartingInBand(int bandIndex, ItemIndexVector& iv);
    void ItemsVisibleForYRange(int miny, int maxy, ItemIndexVector& hv);
    void ItemsVisibleForArea(QRect& rect, ItemIndexVector& hv, bool onlyInsider = false);
    void SelectItemsForArea(QRect& rect, ItemIndexVector& hvLeft, ItemIndexVector& hvInside, ItemIndexVector& hvRight, QRect& unionRect);
    void Clear() { _bands.clear(); }
    int constexpr BandHeight() const { return _bandHeight; }
    bool IsEmpty() const { return _bands.isEmpty(); }
    int Size() const { return _bands.size(); }
    int ItemCount() const
    {
        //int cnt = 0;
        //for (int ib = 0; ib < _bands.size(); ++ib)
        //    cnt += _bands[ib].indices.size();
        //return cnt;
        return _itemCount;
    }
    int IndexForItemNumber(int num) // returns -1 if no such index
    {
        if (num < 0)
            return num;

        static int
            _ib = 0,
            _iprev = -2,        // so that _prev+1 != 0
            _ibase = 0;

        if (num != _iprev + 1)         // then must go through _bands
        {
            for (_ib = 0; _ib < _bands.size(); ++_ib)
            {
                int isize = _bands[_ib].indices.size();
                if (num < isize)    // found the band
                {
                    _iprev = num;
                    return _bands[_ib].indices[num].index;
                }
                num -= isize;
                _ibase += isize;
            }
            return -1;  // no such item
        }
        else                   // next item
        {
            if (++_iprev == _bands[_ib].indices.size())  // end of this band
            {
                _ibase += _bands[_ib].indices.size();
                if (++_ib == _bands.size())
                    return -1;
            }
            return _bands[_ib].indices[num - _ibase].index;
        }
        return -1;
    }
private:
    struct Band
    {

        int band_index;           // needed because _bands is not contiguous  
        int topItemY = std::numeric_limits<int>::max();   // y coordinate of topmost item in band, max if not set
        ItemIndexVector indices;  // z-ordered indices in _items for visible items
        bool operator<(const Band& other) const { return band_index < other.band_index; }
        bool operator<(int n) const { return band_index < n; };

        void Remove(int index);
    };

    History* _pHist = nullptr;
    int _bandHeight = -1;
    QPoint _visibleBands;   // x() top, y() bottom, any of them -1: not used
    QList<Band> _bands;     // Band-s ordered by ascending band index
    int _itemCount = 0;    // count of all individual items in all bands

    int _ItemStartsInBand(int band_index, HistoryItem* phi);
    int _AddBandFor(int y);  // if present returns existing, else inserts new band, or appends when above all bands
    int _GetBand(int y);     // if present returns existing, else return -1
    int _FindBandFor(int y) const;  // binary search: returns index of  the band which have y inside
    void _ItemsInRangeForBand(int bandIndex, int ymin, int ymax, ItemIndexVector& iv);
    void _ItemsInRectForBand(int bandIndex, QRect& rect, ItemIndexVector& iv, bool insidersOnly);
    void _SelectItemsFromBand(int bandIndex, QRect& rect, ItemIndexVector& hvLeft, ItemIndexVector& hvInside, ItemIndexVector& hvRight, QRect& unionRect);
    void _Insert(Band& band, int index); // insert into indices at correct position
};
#endif
