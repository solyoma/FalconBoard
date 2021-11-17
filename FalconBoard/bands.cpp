#include <QApplication>
#include <QtGui>

#include "common.h"
#include "history.h"

// ********************************** BANDS *************


/*========================================================
 * TASK:	get existing band in which y lies or insert/append
 *			a new band if no such band exists
 * PARAMS:	y - >0, coordinate of point
 * GLOBALS:
 * RETURNS:
 * REMARKS: - we need band_index to check if this band exists
 *-------------------------------------------------------*/
int Bands::_AddBandFor(int y)
{
	int iband = _FindBandFor(y);		// check if this band exists if not iband = _bands.size()
	int nband = y / _bandHeight;

	if (iband == _bands.size() || _bands[iband].band_index != nband)	// all bands are either below this one
	{																	// or we found the first band above
		Band newBand;													// add new band
		newBand.band_index = nband;
		newBand.topItemY = y;
		_bands.insert(iband, newBand);
	}
	else if (_bands[iband].topItemY > y)	// existing band: just set top item (smallest y)
		_bands[iband].topItemY = y;
	
	return iband;
}


/*========================================================
 * TASK:	Gets index of existing band for a given y
 * PARAMS:
 * GLOBALS:
 * RETURNS: band index in _bands if there exists a band
 *				which has y inside, -1 otherwise
 * REMARKS: - bands are not contiguous
 *-------------------------------------------------------*/
int Bands::_GetBand(int y)
{
	int nband = y / _bandHeight;
	int iband = _FindBandFor(y);		// check if this band exists
	if (iband == _bands.size())
		return -1;
	return iband;
}

void Bands::SetParam(History* pHist, int bandHeight)
{
	if (_pHist == pHist && _bandHeight == bandHeight)
		return;

	_pHist = pHist;
	if (bandHeight < 0)
		bandHeight = QGuiApplication::primaryScreen()->geometry().height();

	_bandHeight = bandHeight;
	_bands.clear();
	HistoryItem* phi;
	int yix = -1;
	while ((phi = pHist->atYIndex(++yix)) != nullptr)
	{
		if (phi->IsScribble() && !phi->Hidden())
			Add(_pHist->IndexForYIndex(yix));
	}
}


/*========================================================
 * TASK:	add scribble or screenshot to as many bands
 *			as it spans
 * PARAMS:	index:in _items (zorder > DRAWABLE_ZORDER_BASE)
 *				or in 'pImages' whose Area() is used
 *				to see which bands it intersects
 * GLOBALS:
 * RETURNS:
 * REMARKS: -
 *-------------------------------------------------------*/
void Bands::Add(int index)
{
	HistoryItem* phi = (*_pHist)[index];
	int y = phi->Area().y(),			// top of new scribble
		h = y + phi->Area().height();	// bottom of new scrible +1
	int ib = -1;			// band index. -1: no item added yet
	while (y <= h || (h > ib *_bandHeight && h < y) )
	{
		if (ib < 0)			// only added once to count
			++_itemCount;
		ib = _AddBandFor(y);
		_Insert(_bands[ib], index);	// to items in band, z-ordered 
		++ib;
		y += _bandHeight;	// skip next band
	}
}


/*========================================================
 * TASK:	remove scribble or screenshot from band(s)
 *			if the band becomes empty removes the band too
 * PARAMS:
 * GLOBALS:
 * RETURNS:
 * REMARKS: -
 *-------------------------------------------------------*/
void Bands::Remove(int index)
{
	HistoryItem* phi = (*_pHist)[index];
	int y = phi->Area().y();
	if (_itemCount)			// only remove once from count
		--_itemCount;
	int ib = _GetBand(y);					// no item removed yet

	auto hasIndex = [&](int ib, int index) -> bool 
	{
		for (auto ix : _bands[ib].indices)
			if (ix.index == index)
				return true;
		return false;
	};

	while (hasIndex(ib,index))
	{
		_bands[ib].Remove(index);	// to items in band, z-ordered 
		if (_bands[ib].indices.isEmpty())	// no items left in band
			_bands.removeAt(ib);
		++ib;
	};
}

int Bands::ItemsStartingInBand(int bandIndex, ItemIndexVector& iv)
{
	for (auto ind : _bands[bandIndex].indices)
	{
		if (_ItemStartsInBand(bandIndex, (*_pHist)[ind.index]))
			iv.push_back(ind);
	}
	return iv.size();
}

/*========================================================
 * TASK:	gets all items that has any part
 *				inside that band into vector hv
 * PARAMS:
 * GLOBALS:
 * RETURNS:
 * REMARKS: -	does not clear hv !
 *-------------------------------------------------------*/
void Bands::_ItemsForBand(int bandIndex, QRect& rect, ItemIndexVector& hv, bool insidersOnly)
{
	Band& band = _bands[bandIndex];
	for (auto ind : band.indices)
	{
		HistoryItem* phi = (*_pHist)[ind.index];
		if ((insidersOnly && rect.contains(phi->Area())) || (!insidersOnly && rect.intersects(phi->Area())))
			hv.push_back(ind);
	}
}


/*========================================================
 * TASK:	collects scribble items inside band 'bandIndex'
 *			into three lists: items that are inside, left
 *			or right of 'rect'
 * PARAMS: bandIndex,
 *			rect,
 *			hvLeft, hvInside, hvRight: integer vectors for
				item indices
 * GLOBALS:
 * RETURNS:
 * REMARKS: - items with bounding rectangles whose corners
 *				are outside band ara not collected
 *-------------------------------------------------------*/
void Bands::_SelectItemsFromBand(int bandIndex, QRect& rect, ItemIndexVector& hvLeft, ItemIndexVector& hvInside, ItemIndexVector& hvRight, QRect& unionRect)
{
//	int topBand, bottomBand;    // indices of bands where this item starts and ends
	Band& band = _bands[bandIndex];
	auto inlineWithRect = [&](QRect& bndRect)	// returns bit #0: top id in line, bit#1: bottom is in line
	{
		return ((bndRect.y() >= rect.y() && bndRect.y() <= rect.bottom()) ? 1 : 0) +
			((bndRect.bottom() >= rect.y() && bndRect.bottom() <= rect.bottom()) ? 2 : 0);
	};

	for (auto ind : band.indices)
	{
		HistoryItem* phi = (*_pHist)[ind.index];
		QRect bndRect = phi->Area();
		int ilkind;

		if (rect.contains(bndRect))
		{
			hvInside.push_back(ind);
			unionRect = unionRect.united(bndRect);
		}
		else if ((ilkind = inlineWithRect(bndRect)))
		{
			//			if (!rect.intersects(bndRect))
			{
				if (ilkind & 1)	// top of bndrect in line with rect
				{
					if (bndRect.x() > rect.right())
						hvRight.push_back(ind);
					else if (bndRect.right() < rect.x())
						hvLeft.push_back(ind);
				}
				else
				{
					if (bndRect.right() > rect.right())
						hvRight.push_back(ind);
					else if (bndRect.x() < rect.x())
						hvLeft.push_back(ind);

				}
			}
		}
	}
}


/*========================================================
 * TASK:	populates an ItemIndexVector with indices of visible
 *			elements that are inside of or intersect 'rect'
 * PARAMS:	rect		 - area to include
 *			hv			 - fill elements into this vector
 *			insidersOnly - indices only items completely inside
 *							rect are return
 * GLOBALS:
 * RETURNS:
 * REMARKS: - does not change _bands
 *			- gets items from the bands that are inside this area
 *			- only items in a band that has their top coordinates
 *				inside that band are returned for a band
 *-------------------------------------------------------*/
void Bands::ItemsVisibleForArea(QRect& rect, ItemIndexVector& hv, bool insidersOnly)
{
	if (_bands.isEmpty() || !ItemCount())
		return;

	int y = rect.top();
	int ib = _FindBandFor(y);	// it may found the next band below y
	if (ib == _bands.size())
		return;
	y += rect.height();
	do
	{
		_ItemsForBand(ib, rect, hv, insidersOnly);
		if (y < (_bands[ib].band_index + 1) * _bandHeight)	// bottom of rect is in this band
			break;
		++ib;
	} while (ib != _bands.size());
}

void Bands::SelectItemsForArea(QRect& rect, ItemIndexVector& hvLeft, ItemIndexVector& hvInside, ItemIndexVector& hvRight, QRect& unionRect)
{
	if (_bands.isEmpty() || !ItemCount())
		return;

	int y = rect.top();
	int ib = _FindBandFor(y);	// it may found the next band below y
	if (ib == _bands.size())
		return;
	y += rect.height();
	do
	{
		_SelectItemsFromBand(ib, rect, hvLeft, hvInside, hvRight, unionRect);
		if (y < (_bands[ib].band_index + 1) * _bandHeight)	// bottom of rect is in this band
			break;
		++ib;
	} 	while (ib != _bands.size());
}

/*========================================================
 * TASK:	finds band that contains the y coordinate or
 *			the band after it or the band size
 * PARAMS:
 * GLOBALS:
 * RETURNS:
 * REMARKS: - caller must check if this is the correct band
 *			 for y
 *-------------------------------------------------------*/
int Bands::_FindBandFor(int y) const
{
	int	nband = y / _bandHeight;	// look for this band_ind

	QList<Band>::const_iterator iter;

	iter = std::lower_bound(_bands.begin(), _bands.end(), nband);
	if (iter != _bands.end())
		return (iter - _bands.begin());
	else
		return _bands.size();
}

void Bands::_Insert(Band& band, int index)
{
	int zi = (*_pHist)[index]->ZOrder();
	ItemIndex zind = { zi, index };
	auto iter = std::lower_bound(band.indices.begin(), band.indices.end(), zind);
	int i = iter - band.indices.begin();
	band.indices.insert(i, zind);
}


/*========================================================
 * TASK:	Checks if item starts in this band
 * PARAMS:	bi	 - valid band_index
 *			phi		- pointer to historyItem
 * GLOBALS:
 * RETURNS:
 * REMARKS: -
 *-------------------------------------------------------*/
int Bands::_ItemStartsInBand(int bi, HistoryItem* phi)
{
	QRect rhi = phi->Area();
	return  (rhi.top() >= _bands[bi].band_index * _bandHeight) &&
		(rhi.bottom() + 1 > rhi.top());
}

void Bands::Band::Remove(int ix) // z-order irrelevant
{
	for (int i = 0; i < indices.size(); ++i)
		if (indices.at(i).index == ix)
		{
			indices.remove(i);
			break;
		}
}
