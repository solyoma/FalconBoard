#include "history.h"
#include <QApplication>
#include <QMessageBox>
#include <QMainWindow>
#include <algorithm>


static void SwapWH(QRect& r)
{
	int w = r.width();
	r.setWidth(r.height());
	r.setHeight(w);

}

//-------------------------------------------------

ScribbleItem::ScribbleItem(HistEvent he, int zorder) noexcept : type(he), zOrder(zorder) {}
ScribbleItem::ScribbleItem(const ScribbleItem& di) { *this = di; }
ScribbleItem::ScribbleItem(const ScribbleItem&& di) noexcept { *this = di; }
ScribbleItem& ScribbleItem::operator=(const ScribbleItem& di)
{
	type = di.type;
	zOrder = di.zOrder;
	penKind = di.penKind;
	penWidth = di.penWidth;
	points = di.points;
	bndRect = di.bndRect;
	return *this;
}

ScribbleItem& ScribbleItem::operator=(const ScribbleItem&& di)  noexcept
{
	type = di.type;
	zOrder = di.zOrder;
	penKind = di.penKind;
	penWidth = di.penWidth;
	points = di.points;
	bndRect = di.bndRect;
	return *this;
}

void ScribbleItem::clear()
{
	points.clear();
	bndRect = QRect();
	type = heNone;
}

bool ScribbleItem::IsExtension(const QPoint& p, const QPoint& p1, const QPoint& p2) // vectors p->p1 and p1->p are parallel?
{
	//return false;       // DEBUG as it is not working yet

	if (p == p1)
		return true;    // nullvector may point in any direction :)

	QPoint vp = p - p1,  // not (0, 0)
		vpp = p1 - p2;

	// the two vectors point in the same direction when vpp.y()/vpp.x() == vp.y()/vp.x()
	// i.e to avoid checking for zeros in divison: 
	return (vpp.y() * vp.x() == vp.y() * vpp.x()) && (vp.x() * vpp.x() > 0);
}

void ScribbleItem::add(QPoint p)
{
	int n = points.size() - 1;
	                    // we need at least one point already in the array
	// if a point just extends the line in the same direction (IsExtension())
	// as the previous point was from the one before it
	// then do not add a new point, just modify the coordinates of the middle point
	// (n 0: 1 point, >0: at least 2 points are already in the array
	if (n > 0 && IsExtension(p, points[n], points[n - 1]))  // then the two vector points in the same direction
		points[n] = p;                            // so continuation
	else
		points.push_back(p);
}

void ScribbleItem::add(int x, int y)
{
	QPoint p(x, y);
	add(p);
}

void ScribbleItem::Smooth()
{
	// smoothing points so that small variations in them vanish
	// ???
}

void ScribbleItem::SetBoundingRectangle()
{
	bndRect = QRect();
	if (points.isEmpty())
		return;
								 // add first point
	QPoint& p = points[0];
	int w = penWidth / 2;
	QRect pointRect = QRect(p.x() - w, p.y() - w, penWidth, penWidth);      // single point
	bndRect = pointRect;

	for (auto& p : points)
		bndRect = bndRect.united(QRect(p.x() - w, p.y() - w, penWidth, penWidth));

	//else if (!bndRect.contains(p))
	//{
	//	if (bndRect.x() >= p.x() - penWidth)		bndRect.setX(p.x() - 2 * penWidth);
	//	if (bndRect.y() >= p.y() - penWidth)		bndRect.setY(p.y() - 2 * penWidth);
	//	if (bndRect.right() <= p.x() + penWidth)	bndRect.setRight(p.x() + 4 * penWidth + 1);
	//	if (bndRect.bottom() <= p.y() + penWidth)	bndRect.setBottom(p.y() + 4 * penWidth + 1);
	//}
	// DEBUG
	// qDebug("point: (%d, %d), rect: (l: %d, t: %d, w: %d, h: %d)", p.x(), p.y(), rect.x(), rect.y(), rect.width(), rect.height());
	// /DEBUG            


}

bool ScribbleItem::intersects(const QRect& arect) const
{
	return bndRect.intersects(arect);
}

void ScribbleItem::Translate(QPoint dr, int minY)
{
	if (bndRect.y() < minY || !isVisible)
		return;

	for (int i = 0; i < points.size(); ++i)
		points[i] = points[i] + dr;
	bndRect.translate(dr);
}

void ScribbleItem::Rotate(MyRotation rotation, QRect encRect, float alpha)	// rotate around the center of encRect
{
	int erx = encRect.x(), ery = encRect.y(),
		erw = encRect.width(),
		erh = encRect.height();
					// item bndRectangle
	int rx = bndRect.x(), ry = bndRect.y(), rw = bndRect.width(), rh = bndRect.height();	
				   // modify encompassing rectangle and create a big and a little square
				   // for rotation with sides erb and erl respectively
		 
		// if h >= w d+ = erh else d+ = erw
		// if h >= w d- = erw else d- = erh
		// if rotR90  -> x' = erx + ery + (d+) - y,	y' = ery - erx + x
		// if rotL90  -> x' = erx - ery + y,		y' = erx + ery + (d-) - x
		// rot180     -> x' = 2*erx + erw - x,		y' = 2*ery + erh - y

	int x, y, d;
	int A = erx + ery,
		B = erx - ery;

	auto RotR90 = [&](QPoint& p)
	{
		d = (erh >= erw) ? (rot == rotNone ? erh : erw) : (rot == rotNone ? erw : erh);
		x = A + d - p.y();
		y = -B + p.x();
		p.setX(x); p.setY(y);
	};

	auto RotRectR90 = [&](QRect& r)		  // get new rectangle for transform
	{
		QPoint p = QPoint(r.x(), r.y() + r.height()); // bottom left will be top left after rotation
		RotR90(p);
		QSize size = QSize(r.height(), r.width());
		r = QRect(p, size);		// swap height and with
	};
	auto RotL90 = [&](QPoint& p)
	{
		d = (erh >= erw) ? (rot == rotNone ? erw : erh) : (rot == rotNone ? erh : erw);
		x = B + p.y();
		y = A + erw - p.x();
		p.setX(x); p.setY(y);
	};
	auto RotRectL90 = [&](QRect& r)
	{
		QPoint p = QPoint(r.x() + r.width(), r.y()); // top right will be top left after rotation
		RotL90(p);
		QSize size = QSize(r.height(), r.width());
		r = QRect(p, size);		// swap height and with
	};

	QPoint tl;	// top left of transformed item rectangle

	switch (rotation)
	{
		case rotR90:
			for (QPoint& p : points)
				RotR90(p);
			RotRectR90(bndRect);
			break;
		case rotL90:
			for (QPoint& p : points)
				RotL90(p);
			RotRectL90(bndRect);
			break;
		case rot180:
			for (QPoint& p : points)
			{
				x = 2*erx + erw - p.x();
				y = 2*ery + erh - p.y();
				p.setX(x), p.setY(y);
			}
			break;
		case rotFlipH:
			for (QPoint& p : points)
				p.setX(erw + 2 * erx - p.x());
			bndRect = QRect(erw + 2 * erx - rx - rw, ry, rw, rh);		  // new item x: original top righ (rx + rw)
			break;
		case rotFlipV:
			for (QPoint& p : points)
				p.setY(erh - p.y() + 2 * ery);
			bndRect = QRect(rx, erh + 2 * ery - ry - rh, rw, rh);		  // new item x: original bottom left (ry + rh)
			break;
		default:
			break;
	}

	if (rot == rotNone)		// save rotation for undo
		rot = rotation;
	else
		rot = rotNone;		// undone: no rotation set yet

}

inline QDataStream& operator<<(QDataStream& ofs, const ScribbleItem& di)
{
	ofs << (qint32)di.type << di.zOrder << (qint32)di.penKind << (qint32)di.penWidth;
	ofs << (qint32)di.points.size();
	for (auto pt : di.points)
		ofs << (qint32)pt.x() << (qint32)pt.y();
	return ofs;
}
		   // reads ONLY after the type is read in!
inline QDataStream& operator>>(QDataStream& ifs, ScribbleItem& di)
{
	qint32 n;
	ifs >> n; di.zOrder = n;
	ifs >> n; di.penKind = (MyPenKind)n;
	ifs >> n; di.penWidth = n;

	qint32 x, y;
	di.points.clear();
	di.bndRect = QRect(0, 0, 0, 0);

	ifs >> n;
	while (n--)
	{
		ifs >> x >> y;
		di.add(x, y);
	}

	di.SetBoundingRectangle();

	return ifs;
}

inline QDataStream& operator<<(QDataStream& ofs, const ScreenShotImage& bimg)
{
	ofs << (int)heScreenShot << bimg.zOrder << bimg.topLeft.x() << bimg.topLeft.y();
	ofs << bimg.image;

	return ofs;
}

inline QDataStream& operator>>(QDataStream& ifs, ScreenShotImage& bimg)
{	 // type already read in
	int x, y;
	ifs >> bimg.zOrder >> x >> y;
	bimg.topLeft = QPoint(x, y);
	ifs >> bimg.image;
	return ifs;
}

//------------------------------------------------

QRect ScreenShotImage::AreaOnCanvas(const QRect& canvasRect) const
{
	return Area().intersected(canvasRect);
}

void ScreenShotImage::Translate(QPoint p, int minY)
{
	if (topLeft.y() >= minY)
		topLeft += p;

}

void ScreenShotImage::Rotate(MyRotation rot, QRect encRect, float alpha)
{
	QTransform transform;
	switch (rot)
	{
		case rotR90: transform.rotate(270); image = image.transformed(transform, Qt::SmoothTransformation); break;
		case rotL90: transform.rotate(90);  image = image.transformed(transform, Qt::SmoothTransformation); break;
		case rot180: transform.rotate(180); image = image.transformed(transform, Qt::SmoothTransformation); break;
		case rotFlipH: image = image.mirrored(true, false); break;
		case rotFlipV: image = image.mirrored(false, true); break;
		default: break;
	}
}

// ---------------------------------------------

void ScreenShotImageList::Add(QImage& image, QPoint pt, int zorder)
{
	ScreenShotImage img;
	img.image = image;
	img.topLeft = pt;
	(*this).push_back(img);
}

QRect ScreenShotImageList::AreaOnCanvas(int index, const QRect& canvasRect) const
{
	return (*this)[index].AreaOnCanvas(canvasRect);
}

ScreenShotImage* ScreenShotImageList::ScreenShotAt(int index)
{
	if(index >= size())
		return nullptr;
	return &(*this)[index];
}

ScreenShotImage* ScreenShotImageList::FirstVisible(const QRect& canvasRect)
{
	_index = -1;
	_canvasRect = canvasRect;
	return NextVisible();
}

ScreenShotImage* ScreenShotImageList::NextVisible()
{
	if (_canvasRect.isNull())
		return nullptr;

	while (++_index < size())
	{
		if ((*this)[_index].isVisible)
		{
			if (!AreaOnCanvas(_index, _canvasRect).isNull())
				return &(*this)[_index];
		}
	}
	return nullptr;
}

void ScreenShotImageList::Translate(int which, QPoint p, int minY)
{
	if (which < 0 || which >= size() || (*this)[which].isVisible)
		return;
	(*this)[which].Translate(p, minY);
}

void ScreenShotImageList::Rotate(int which, MyRotation rot, QRect encRect, float alpha)
{
	if (which < 0 || which >= size() || (*this)[which].isVisible)
		return;
	(*this)[which].Rotate(rot, encRect, alpha);
}


void ScreenShotImageList::Clear()
{
	QList<ScreenShotImage>::clear();
}

/*========================================================
 * TASK:	search for topmost image in list 'pImages'
 * PARAMS:	p - paper relative point (logical coord.)
 * GLOBALS:
 * RETURNS:	index of found or -1
 * REMARKS: -
 *-------------------------------------------------------*/
int ScreenShotImageList::ImageIndexFor(QPoint& p) const
{
	int z = -1;
	int found = -1;
	for (int i = 0; i < size(); ++i)
	{
		ScreenShotImage ssi = operator[](i);
		if (ssi.Area().contains(p))
		{
			if (ssi.zOrder > z)
			{
				z = ssi.zOrder;
				found = i;
			}
		}
	}
	return found;
}

/*========================================================
 * TASK:	search for topmost image in list 'pImages'
 * PARAMS:	p - paper relative point (logical coord.)
 * GLOBALS:
 * RETURNS:	index of found or -1
 * REMARKS: -
 *-------------------------------------------------------*/
int ScreenShotImageList::ImageIndexFor(QPoint& p) const
{
	int z = -1;
	int found = -1;
	for (int i = 0; i < size(); ++i)
	{
		ScreenShotImage ssi = operator[](i);
		if (ssi.isVisible && ssi.Area().contains(p))
		{
			if (ssi.zOrder > z)
			{
				z = ssi.zOrder;
				found = i;
			}
		}
	}
	return found;
}

/*========================================================
 * One history element
 *-------------------------------------------------------*/

bool HistoryItem::operator<(const HistoryItem& other)
{
	if (!IsSaveable() || Hidden())
		return false;
	if (!other.IsSaveable() || other.Hidden())
		return true;

	if (TopLeft().y() < other.TopLeft().y())
		return true;
	else if (TopLeft().y() == other.TopLeft().y() )
	{
		if (TopLeft().x() < other.TopLeft().x())
			return true;
	}
	return false;
}

//--------------------------------------------
// type heScribble, heEraser
HistoryScribbleItem::HistoryScribbleItem(History* pHist, ScribbleItem& dri) : HistoryItem(pHist, dri.type), scribbleItem(dri)
{
	type = dri.type;
	scribbleItem.SetBoundingRectangle();
}

HistoryScribbleItem::HistoryScribbleItem(const HistoryScribbleItem& other) : HistoryItem(other.pHist)
{
	*this = other;
}
HistoryScribbleItem::HistoryScribbleItem(const HistoryScribbleItem&& other) noexcept: HistoryItem(other.pHist)
{
	*this = other;
}

void HistoryScribbleItem::SetVisibility(bool visible)
{
	scribbleItem.isVisible = visible;
}

bool HistoryScribbleItem::Hidden() const
{
	return !scribbleItem.isVisible;
}

HistoryScribbleItem& HistoryScribbleItem::operator=(const HistoryScribbleItem& other)
{
	pHist = other.pHist;
	type = other.type;
	scribbleItem = other.scribbleItem;
	return *this;
}
HistoryScribbleItem& HistoryScribbleItem::operator=(const HistoryScribbleItem&& other) noexcept
{
	pHist = other.pHist;
	type = other.type;
	scribbleItem = other.scribbleItem;
	return *this;
}
int HistoryScribbleItem::ZOrder() const
{
	return scribbleItem.zOrder;
}

ScribbleItem* HistoryScribbleItem::GetVisibleScribble(int index) const
{
	return (index || Hidden()) ? nullptr : const_cast<ScribbleItem*>(&scribbleItem);
}

ScribbleItem* HistoryScribbleItem::GetScribble(int index) const
{
	return (index) ? nullptr : const_cast<ScribbleItem*>(&scribbleItem);
}

QRect HistoryScribbleItem::Area() const
{
	return scribbleItem.bndRect;
}

void HistoryScribbleItem::Translate(QPoint p, int minY)
{
	scribbleItem.Translate(p, minY);
}

void HistoryScribbleItem::Rotate(MyRotation rot, QRect encRect, float alpha)
{
	scribbleItem.Rotate(rot, encRect);
}

//--------------------------------------------
int  HistoryDeleteItems::Undo()	  // reveal items
{
	for (auto i : deletedList)
		pHist->SetVisibility(i.index, true);
	return 1;
}
int  HistoryDeleteItems::Redo()	// hide items
{
	for (auto i : deletedList)
		pHist->SetVisibility(i.index, false);
	return 0;
}

HistoryDeleteItems::HistoryDeleteItems(History* pHist, ItemIndexVector& selected) : HistoryItem(pHist), deletedList(selected)
{
	type = heItemsDeleted;
	Redo();         // hide them
}
HistoryDeleteItems::HistoryDeleteItems(HistoryDeleteItems& other) : HistoryDeleteItems(other.pHist, other.deletedList) {}
HistoryDeleteItems& HistoryDeleteItems::operator=(const HistoryDeleteItems& other)
{
	type = heItemsDeleted;
	pHist = other.pHist;
	deletedList = other.deletedList;
	//        hidden = other.hidden;
	return *this;
}
HistoryDeleteItems::HistoryDeleteItems(HistoryDeleteItems&& other)  noexcept : HistoryDeleteItems(other.pHist, other.deletedList) {}
HistoryDeleteItems& HistoryDeleteItems::operator=(const HistoryDeleteItems&& other) noexcept
{
	type = heItemsDeleted;
	pHist = other.pHist;
	deletedList = other.deletedList;
	//        hidden = other.hidden;
	return *this;
}

//--------------------------------------------

/*========================================================
 * TASK:
 * PARAMS:
 * GLOBALS:
 * RETURNS:
 * REMARKS: -
 *-------------------------------------------------------*/
int  HistoryRemoveSpaceItem::Redo()
{
	if (modifiedList.isEmpty())	 // vertical movement
	{
		pHist->TranslateAllItemsBelow(QPoint(0, -delta), y);	 // -delta < 0 move up
	}
	else	// horizontal movement
	{
		QPoint dr(-delta, 0);								// move left
		for (auto i : modifiedList)
			(*pHist)[i.index]->Translate(dr, -1);
	}
	return 0;
}

/*========================================================
 * TASK:
 * PARAMS:
 * GLOBALS:
 * RETURNS:
 * REMARKS: -
 *-------------------------------------------------------*/
int  HistoryRemoveSpaceItem::Undo()
{
	if (modifiedList.isEmpty())	 // vertical movement
	{
		pHist->TranslateAllItemsBelow(QPoint(0, delta), y-delta);	  //delte > 0 move down
	}
	else	// horizontal movement
	{
		QPoint dr(delta, 0);								 // delta >0 -> move right
		for (ItemIndex i : modifiedList)
			(*pHist)[i.index]->Translate(dr, -1);
	}
	return 1;
}

HistoryRemoveSpaceItem::HistoryRemoveSpaceItem(History* pHist, ItemIndexVector& toModify, int first, int distance) :
	HistoryItem(pHist), modifiedList(toModify), y(y), delta(distance)
{
	type = heSpaceDeleted;
	Redo();
}

HistoryRemoveSpaceItem::HistoryRemoveSpaceItem(HistoryRemoveSpaceItem& other) : HistoryItem(other.pHist)
{
	*this = other;
}

HistoryRemoveSpaceItem& HistoryRemoveSpaceItem::operator=(const HistoryRemoveSpaceItem& other)
{
	modifiedList = other.modifiedList;
	y = other.y;
	delta = other.delta;
	return *this;
}
HistoryRemoveSpaceItem::HistoryRemoveSpaceItem(HistoryRemoveSpaceItem&& other)  noexcept : HistoryItem(other.pHist)
{
	*this = other;
}

HistoryRemoveSpaceItem& HistoryRemoveSpaceItem::operator=(const HistoryRemoveSpaceItem&& other)  noexcept
{
	modifiedList = other.modifiedList;
	y = other.y;
	delta = other.delta;
	return *this;
}

//--------------------------------------------

HistoryPasteItemBottom::HistoryPasteItemBottom(History* pHist, int index, int count) :
	HistoryItem(pHist, heItemsPastedBottom), index(index), count(count)
{
}

HistoryPasteItemBottom::HistoryPasteItemBottom(HistoryPasteItemBottom& other) :
	HistoryItem(other.pHist, heItemsPastedBottom), index(other.index), count(other.count)
{
}

HistoryPasteItemBottom& HistoryPasteItemBottom::operator=(const HistoryPasteItemBottom& other)
{
	type = heItemsPastedBottom;
	index = other.index;
	count = other.count;
	return *this;
}

//--------------------------------------------
HistoryPasteItemTop::HistoryPasteItemTop(History* pHist, int index, int count, QRect& rect) :
	HistoryItem(pHist, heItemsPastedTop), indexOfBottomItem(index), count(count), boundingRect(rect)
{
}

HistoryPasteItemTop::HistoryPasteItemTop(HistoryPasteItemTop& other) : 
	HistoryItem(other.pHist)
{
	*this = other;
}
HistoryPasteItemTop& HistoryPasteItemTop::operator=(const HistoryPasteItemTop& other)
{
	type = heItemsPastedTop;
	indexOfBottomItem = other.indexOfBottomItem;
	count = other.count;
	boundingRect = other.boundingRect;
	return *this;
}
HistoryPasteItemTop::HistoryPasteItemTop(HistoryPasteItemTop&& other)  noexcept: HistoryItem(other.pHist)
{
	*this = other;
}
HistoryPasteItemTop& HistoryPasteItemTop::operator=(const HistoryPasteItemTop&& other) noexcept
{
	type = heItemsPastedTop;
	indexOfBottomItem = other.indexOfBottomItem;
	count = other.count;
	boundingRect = other.boundingRect;
	return *this;
}

int  HistoryPasteItemTop::Undo() // elements are in _items
{
	SetVisibility(false);
	if (moved)	// then the first item above the bottom item is a history delete item
		(*pHist)[indexOfBottomItem + 1]->Undo();
	return count + moved + 2;	// decrease actual pointer below bottom item
}

int HistoryPasteItemTop::Redo()		// elements copied back to '_items' already
{
	if (moved)	// then the first item above the bottom item is a history delete item
		(*pHist)[indexOfBottomItem + 1]->Redo();
	SetVisibility(true);

	return count + moved + 1;	// we are at bottom item -> change stack pointer to top item
}

bool HistoryPasteItemTop::Hidden() const
{
	return (*pHist)[indexOfBottomItem]->Hidden();
}

void HistoryPasteItemTop::SetVisibility(bool visible)
{
	for (int i = 1; i <= count; ++i)
		(*pHist)[indexOfBottomItem + moved + i]->SetVisibility(visible);
}

void HistoryPasteItemTop::Translate(QPoint p, int minY)
{
	for (int i = 1; i <= count; ++i)
		(*pHist)[indexOfBottomItem + i]->Translate(p, minY);

	if (boundingRect.y() >= minY)
		boundingRect.translate(p);
}

void HistoryPasteItemTop::Rotate(MyRotation rot, QRect encRect, float alpha)
{
	for (int i = 1; i <= count; ++i)
		(*pHist)[indexOfBottomItem + i]->Rotate(rot, encRect);
}

ScribbleItem* HistoryPasteItemTop::GetScribble(int which) const
{
	if (which < 0 || which >= count)
		return nullptr;
	return (*pHist)[indexOfBottomItem + 1 + which]->GetScribble(0);
}

ScribbleItem* HistoryPasteItemTop::GetVisibleScribble(int which) const
{
	if (which < 0 || which >= count)
		return nullptr;
	return (*pHist)[indexOfBottomItem + 1 + which]->GetVisibleScribble(0);
}

QRect HistoryPasteItemTop::Area() const
{
	return boundingRect;
}
//---------------------------------------------------
HistoryReColorItem::HistoryReColorItem(History* pHist, ItemIndexVector& selectedList, MyPenKind pk) :
	HistoryItem(pHist), selectedList(selectedList), pk(pk)
{
	type = heRecolor;

	int n = 0;						  // set size for penKind array and get encompassing rectangle
	for (auto i : selectedList)
	{
		boundingRectangle = boundingRectangle.united((*pHist)[i.index]->Area());
		n += (*pHist)[i.index]->Size();
	}
	penKindList.resize(n);
	Redo();		// get original colors and set new color tp pk
}

HistoryReColorItem::HistoryReColorItem(HistoryReColorItem& other) : HistoryItem(other.pHist)
{
	*this = other;
}

HistoryReColorItem& HistoryReColorItem::operator=(const HistoryReColorItem& other)
{
	type = heRecolor;
	pHist = other.pHist;
	selectedList = other.selectedList;
	penKindList = other.penKindList;
	pk = other.pk;
	return *this;
}

HistoryReColorItem::HistoryReColorItem(HistoryReColorItem&& other)  noexcept: HistoryReColorItem(other.pHist, other.selectedList, other.pk)
{
	*this = other;
}

HistoryReColorItem& HistoryReColorItem::operator=(const HistoryReColorItem&& other) noexcept
{
	type = heRecolor;
	pHist = other.pHist;
	selectedList = other.selectedList;
	penKindList = other.penKindList;
	pk = other.pk;
	return *this;
}
int  HistoryReColorItem::Undo()
{
	for (auto i : selectedList)
	{
		int index = 0;
		ScribbleItem* pdri;
		while ((pdri = (*pHist)[i.index]->GetVisibleScribble(index)))
			pdri->penKind = penKindList[index++];
	}
	return 1;
}
int  HistoryReColorItem::Redo()
{
	for (auto i : selectedList)
	{
		int index = 0;
		ScribbleItem* pdri;
		while ((pdri = (*pHist)[i.index]->GetVisibleScribble(index)))
		{
			penKindList[index++] = pdri->penKind;
			pdri->penKind = pk;
		}
	}
	return 0;
}
QRect HistoryReColorItem::Area() const { return boundingRectangle; }

//---------------------------------------------------

HistoryInsertVertSpace::HistoryInsertVertSpace(History* pHist, int top, int pixelChange) :
	HistoryItem(pHist), y(top), heightInPixels(pixelChange)
{
	type = heVertSpace;
	Redo();
}

HistoryInsertVertSpace::HistoryInsertVertSpace(const HistoryInsertVertSpace& other) : HistoryItem(other.pHist)
{
	*this = other;
}
HistoryInsertVertSpace& HistoryInsertVertSpace::operator=(const HistoryInsertVertSpace& other)
{
	type = heVertSpace;
	y = other.y; heightInPixels = other.heightInPixels; pHist = other.pHist;
	return *this;
}

int  HistoryInsertVertSpace::Undo()
{
	pHist->InserVertSpace(y+heightInPixels, -heightInPixels);
	return 1;
}

int  HistoryInsertVertSpace::Redo()
{
	pHist->InserVertSpace(y, heightInPixels);
	return 0;
}
QRect HistoryInsertVertSpace::Area() const
{
	return QRect(0, y, 100, 100);
}

//--------------------------------------------
																					//  which: index in 
HistoryScreenShotItem::HistoryScreenShotItem(History* pHist, int which) : HistoryItem(pHist, heScreenShot), which(which)
{
	type = heScreenShot;
}

HistoryScreenShotItem::HistoryScreenShotItem(const HistoryScreenShotItem& other) : HistoryItem(other.pHist)
{
	*this = other;
}

HistoryScreenShotItem& HistoryScreenShotItem::operator=(const HistoryScreenShotItem& other)
{
	type = heScreenShot;
	which = other.which;
	return *this;
}

HistoryScreenShotItem::~HistoryScreenShotItem()
{
}

int  HistoryScreenShotItem::Undo() // hide
{
	(*pHist->pImages)[which].isVisible = false;
	return 1;
}

int  HistoryScreenShotItem::Redo() // show
{
	(*pHist->pImages)[which].isVisible = true;
	return 0;
}

int HistoryScreenShotItem::ZOrder() const
{
	return  (*pHist->pImages)[which].zOrder;
}

QPoint HistoryScreenShotItem::TopLeft() const
{
	return (*pHist->pImages)[which].topLeft;
}

QRect HistoryScreenShotItem::Area() const
{
	return QRect((*pHist->pImages)[which].topLeft, (*pHist->pImages)[which].image.size());
}

bool HistoryScreenShotItem::Hidden() const
{
	return !(*pHist->pImages)[which].isVisible;
}

bool HistoryScreenShotItem::Translatable() const
{
	return !Hidden();
}

void HistoryScreenShotItem::SetVisibility(bool visible)
{
	(*pHist->pImages)[which].isVisible = visible;
}

void HistoryScreenShotItem::Translate(QPoint p, int minY)
{
	(*pHist->pImages)[which].Translate(p, minY);
}

void HistoryScreenShotItem::Rotate(MyRotation rot, QRect encRect, float alpha)
{
	(*pHist->pImages)[which].Rotate(rot, encRect);
}

ScreenShotImage* HistoryScreenShotItem::GetScreenShotImage() const
{
	return &(*pHist->pImages)[which];
}

//---------------------------------------------------------

HistoryRotationItem::HistoryRotationItem(History* pHist, MyRotation rotation, QRect rect, ItemIndexVector selList, float alpha) :
	HistoryItem(pHist), rot(rotation), rAlpha(alpha), nSelectedItemList(selList), encRect(rect)
{
	encRect = encRect;
	Redo();
}

HistoryRotationItem::HistoryRotationItem(const HistoryRotationItem& other) :
	HistoryItem(other.pHist), rot(other.rot), rAlpha(other.rAlpha), nSelectedItemList(other.nSelectedItemList)
{
	flipV = other.flipV;
	flipH = other.flipH;
	encRect = other.encRect;
}

HistoryRotationItem& HistoryRotationItem::operator=(const HistoryRotationItem& other)
{
	pHist = other.pHist;
	nSelectedItemList = other.nSelectedItemList;
	flipV = other.flipV;
	flipH = other.flipH;
	rAlpha = other.rAlpha;
	encRect = other.encRect;

	return *this;
}

int HistoryRotationItem::Undo()
{
	MyRotation rotation = rot;
	float alpha = rAlpha;
	switch (rot)
	{
		case rotR90:
			rotation = rotL90;
			break;
		case rotL90:
			rotation = rotR90;
			break;
		case rot180:
		case rotFlipH:
		case rotFlipV:
			break;
		case rotAlpha:
			alpha = - rAlpha;
			break;
		default:
			break;
	}
	for (auto n : nSelectedItemList)
		(*pHist)[n.index]->Rotate(rotation, encRect, alpha);
	SwapWH(encRect);

	return 1;
}

int HistoryRotationItem::Redo()
{
	for (auto n : nSelectedItemList)
		(*pHist)[n.index]->Rotate(rot, encRect, rAlpha);
	if(rot != rotFlipH && rot != rotFlipV)
		SwapWH(encRect);
	return 0;
}


//********************************** History class ****************************
void History::_SaveClippingRect()
{
	_savedClps.push(_clpRect);
}

void History::_RestoreClippingRect()
{
	_clpRect = _savedClps.pop();
}

History::History(const History& o)					
{
	_items = o._items;
	_bands = o._bands;
	_yxOrder = o._yxOrder;
}

History::History(const History&& o) noexcept
{
	_items = o._items;
	_bands = o._bands;
	_yxOrder = o._yxOrder;
}

History::~History()
{
	clear();
}

/*========================================================
 * TASK:	Get index of top-left-most element at or below xy.y
 *			in _items, using '_yOrder' (binary search)
 * PARAMS:	xy  - limiting point 
 * GLOBALS:
 * RETURNS: index of first element (>0) in ordered list _yxOrder
 *			which is at the same height or below xy
 * REMARKS: - y inreses downwards
 *-------------------------------------------------------*/
int History::_YIndexForXY(QPoint xy)
{				// get first element, which is above or at xy.y
	IntVector::iterator it = std::lower_bound(_yxOrder.begin(), _yxOrder.end(), 0,     // 0: in place of 'right' - not used
											  [&](const int left, const int right)	   // left: _yorder[xxx], returns true if above or left
											  {
												  QPoint pt = ((const HistoryItem*)_items[left])->TopLeft();
												  if (pt.y() < xy.y())
													  return true;					   // pt.y is below xy.y
												  else if ((pt.y() == xy.y()))		   // or when they are the same 
													  return pt.x() < xy.x();		   // pt is either left (true) or right (false) of xy
												  else
													  return false;					   // pt.y is above xy.y
											  });
	return (it - _yxOrder.begin());
}

/*========================================================
 * TASK:	Get index of first element whose area
 *			intersects the clipping rectangle
 * PARAMS:	none
 * GLOBALS:	_yxOrde, _items
 * RETURNS: index of element or -1
 * REMARKS: 
 *-------------------------------------------------------*/
//int History::_YIndexWhichInside()
//{
//	int i0 = _YIndexForXY(_clpRect.topLeft() );		// any number of items above this may be OK
//	int ib = -1,	// before i0
//		ia = -1;	// after i0			- if not -1 then element found 
//
//	if (i0 >= _yxOrder.size())
//		return -1;
//
//	HistoryItem	*pb = _yitems(i0), 	// pointers before and after selected
//				*pa = pb;			// starting from the same actual selected
//
//	if (pa && !pa->Hidden() && pa->Area().intersects(_clpRect))
//		ia = i0;			// set found element index for 'after' elements
//
//	int inc = 1;									// should test all elements above the i-th one but only do it for max 100
//	while (inc < 100 && (i0-inc >= 0 || ia < 0))
//	{
//		pb = i0 - inc >= 0 ? _yitems(i0 - inc) : nullptr;
//		pa = ia < 0 && (i0 + inc  < _yxOrder.size()) ? _yitems(i0 + inc) : nullptr;
//		if ( pb && !pb->Hidden() && pb->Area().intersects(_clpRect)) 
//			ib = i0 - inc;
//		else if (pa && !pa->Hidden() && pa->Area().intersects(_clpRect)) // only check those that come after if not found already in before 
//			ia = i0 + inc;
//		++inc;
//	}
//
//	if (ib >= 0)
//		return ib;
//	else if (ia >= 0)
//		return ia;
//	else
//		return ib; // -1
//}
int History::_YIndexForIndex(int index)      // index: in unordered array, returns yindex in _yxOrder
{
	QPoint pt = _items[index]->TopLeft();
	return _YIndexForXY(pt);
}

int History::IndexForYIndex(int yix)
{
	if (yix < 0 || yix >= _yxOrder.size())
		return -1;
	return _yxOrder[yix];
}

void History::ReplaceItem(int index, HistoryItem* pi)     // index: in '_items'
{
	int yindex = _YIndexForIndex(index);    // must exist
	delete _items[index];   // was new'd
	_items[index] = pi;
	_yxOrder.remove(yindex);
	int yi = _YIndexForXY(pi->TopLeft());
	_yxOrder.insert(yi, index);
	// no need to change _bands
}

void History::RemoveItemsStartingAt(int index)  // index: into _items
{
	for (int j = index; j < _items.size(); ++j)
	{
		int yindex = _YIndexForIndex(j);    // must exist
		delete _items[j];
		_items[j] = nullptr;
		_yxOrder.remove(yindex);
		_bands.Remove(j);
	}
}

/*========================================================
 * TASK: pushes new element into _items and insert corresponding
 *		index (if applicable) into _yxOrder
 * PARAMS:		pointer to existing item
 * GLOBALS:
 * RETURNS:
 * REMARKS: - does not modify the _redo vector like _AddItem does
 *-------------------------------------------------------*/
void History::_push_back(HistoryItem* pi)
{
	int s = _items.size();					 // physical index to put into _yxOrder 
	_items.push_back(pi);					 // always append

	if (pi->IsScribble())	// Only scribble elements are put into _yxOrder
	{
		_bands.Add(s);	// screenshots in _items has a zorder less than DRAWABLE_ZORDER_BASE

		int yi = _YIndexForXY(pi->TopLeft());    // yi either to element with same x,y values or the one which is higher or at the right
		if (yi == _yxOrder.size() )  // all elements at xy were less than for this one
			_yxOrder.push_back(s);
		else          // the yi-th element has the larger y , or same and larger x coordinate
			_yxOrder.insert(yi, s);
	}
}

QPoint History::BottomRightVisible(QSize screenSize) const
{
	QPoint pt;

	if (!_yxOrder.isEmpty())
	{
		int ix = _yxOrder.size() - 1;
		HistoryItem* phi;
		while (ix >= 0 && (phi = _yitems(ix))->Hidden())
			--ix;
		if (ix >= 0)
		{
			pt = phi->Area().topLeft();
			pt.setY( pt.y() - screenSize.height()/2 );
			pt.setX( pt.x() - screenSize.width());
			if (pt.x() < 0)
				pt.setX(0);
			if (pt.y() < 0)
				pt.setY(0);
		}
	}
	return pt;
}


/*========================================================
 * TASK:	add a new item to the history list
 * PARAMS:	p - item to add. Corresponding scribbles already
 *				added to pHist
 * GLOBALS:
 * RETURNS:
 * REMARKS: - all items
 *-------------------------------------------------------*/
HistoryItem* History::_AddItem(HistoryItem* p)
{
	if (!p)
		return nullptr;

	// _nSelectedItemsList.clear();	// no valid selection after new item
	// _selectionRect = QRect();

	_push_back(p);

	_redo.clear();	// no undo after new item added

	_modified = true;

	return _items[ _items.size() -1 ];		// always the last element
}

bool History::_IsSaveable(int i)
{
	return _items[i]->IsSaveable();
}

void History::clear()		// does not clear lists of copied items and screen snippets
{
	for (auto i : _items)
		delete i;
	for (auto i : _redo)
		delete i;

	_items.clear();
	_redo.clear();
	_yxOrder.clear();
	pImages->Clear();
	_bands.Clear();

	_lastZorder = DRAWABLE_ZORDER_BASE;
	_readCount = 0;

	_modified = false;
}

int History::size() const 
{ 
	return _items.size(); 
}

int History::CountOfVisible() const
{
	int cnt = 0; 
	for (auto a : _yxOrder)
		if (!_items[a]->Hidden())
			++cnt;
	return cnt;
}

HistoryItem* History::operator[](int index)	// absolute index
{
	if (index < 0 || index >= _items.size())
		return nullptr;
	return _items[index];
}

/*========================================================
 * TASK:    save actual visible drawn items
 * PARAMS:  file name
 * GLOBALS:
 * RETURNS:
 * REMARKS: - ordering by band and then by z-order
 *-------------------------------------------------------*/
bool History::Save(QString name)
{
	// only save visible items

	if (_bands.ItemCount() == 0)					// no elements or no visible elements
	{
		QMessageBox::information(nullptr, sWindowTitle, QObject::tr("Nothing to save"));
		return false;;
	}
	
	QFile f(name + ".tmp");
	f.open(QIODevice::WriteOnly);

	if (!f.isOpen())
		return false;   // can't write file

	QDataStream ofs(&f);
	ofs << MAGIC_ID;
	ofs << MAGIC_VERSION;	// file version
	ItemIndexVector iv;
	for (int i =0; i < _bands.Size(); ++i )
	{
		if (_bands.ItemsStartingInBand(i, iv))
		{
			for (auto ind : iv)
			{
				HistoryItem* phi = _items[ind.index];		// ordered by y then x coordinate
				if (phi->Hidden())					// hidden elements are not saved
					continue;

				if (ofs.status() != QDataStream::Ok)
				{
					f.remove();
					return false;
				}

				int index = -1;
				if (phi->IsImage())
				{
					ofs << (*pImages)[((HistoryScreenShotItem*)phi)->which];
					continue;
				}
				else
				{
					ScribbleItem* pscrbl;
					while ((pscrbl = phi->GetVisibleScribble(++index)) != nullptr)
						ofs << *pscrbl;
				}
			}
		}
		iv.clear();
	}
	_modified = false;
	if (QFile::exists(name + "~"))
		QFile::remove(QString(name + "~"));
	QFile::rename(name, QString(name + "~"));

	f.rename(name);

	return true;
}

int History::Load(QString name)  // returns _ites.size() when Ok, -items.size()-1 when read error
{
	QFile f(name);
	f.open(QIODevice::ReadOnly);
	if (!f.isOpen())
		return -1;

	QDataStream ifs(&f);
	qint32 id, version;
	ifs >> id;
	if (id != MAGIC_ID)
		return 0;
	ifs >> version;
	if ((version >> 24) != 'V')
		return 0;

	clear();

	_inLoad = true;

	ScribbleItem di;
	ScreenShotImage bimg;
	int i = 0, n;
	while (!ifs.atEnd())
	{
		ifs >> n;	// HistEvent				
		if ((HistEvent(n) == heScreenShot))
		{
			ifs >> bimg;
			pImages->push_back(bimg);

			int n = pImages->size() - 1;
			HistoryScreenShotItem* phss = new HistoryScreenShotItem(this, n);
			_AddItem(phss);
			continue;
		}

		di.type = (HistEvent)n;

		ifs >> di;
		if (ifs.status() != QDataStream::Ok)
		{
			_inLoad = false;
			return -_items.size() - 1;
		}

		++i;

		HistoryItem* phi = addScribbleItem(di);

	}
	_modified = false;
	_inLoad = false;
	return  _readCount = _items.size();
}

//--------------------- Add Items ------------------------------------------

HistoryItem* History::addClearRoll()
{
	_SaveClippingRect();
	_clpRect = QRect(0, 0, 0x70000000, 0x70000000);
	HistoryItem* pi = addClearVisibleScreen();
	_RestoreClippingRect();
	return pi;
}

HistoryItem* History::addClearVisibleScreen()
{
	ItemIndexVector toBeDeleted;
	_bands.ItemsVisibleForArea(_clpRect, toBeDeleted);

	HistoryDeleteItems* p = new HistoryDeleteItems(this, toBeDeleted);
	return _AddItem(p);
}

HistoryItem* History::addClearDown()
{
	_SaveClippingRect();
	_clpRect.setHeight(0x70000000);
	HistoryItem *pi = addClearVisibleScreen();
	_RestoreClippingRect();
	return pi;
}

HistoryItem* History::addScribbleItem(ScribbleItem& itm)			// may be after an undo, so
{				                                            // delete all scribbles after the last visible one (items[lastItem].scribbleIndex)
	if (_inLoad)
	{
		if (itm.zOrder > _lastZorder)
			_lastZorder = itm.zOrder;
	}
	else
		itm.zOrder = _lastZorder++;		// each scribbles are above any image (until 10 million images)
	HistoryScribbleItem * p = new HistoryScribbleItem(this, itm);
	return _AddItem(p);
}


/*========================================================
 * TASK:	hides visible items on list
 * PARAMS:	pSprite: NULL   - use _nSelectedItemList
 *					 exists - use its selection list
 * RETURNS: pointer to delete item on _items
 * REMARKS: 
 *-------------------------------------------------------*/
HistoryItem* History::addDeleteItems(Sprite* pSprite)
{
	ItemIndexVector* pList = pSprite ? &pSprite->nSelectedItemsList : &_nSelectedItemsList;

	if (!pList->size())
		return nullptr;          // do not add an empty list
	HistoryDeleteItems* p = new HistoryDeleteItems(this, *pList);
	return _AddItem(p);
}

HistoryItem* History::addPastedItems(QPoint topLeft, Sprite *pSprite)			   // tricky
{
	ScribbleItemVector    *pCopiedItems = pSprite ? &pSprite->items : &_copiedItems;      
	ScreenShotImageList	  *pCopiedImages = pSprite ? &pSprite->images : &_copiedImages;
	QRect				  *pCopiedRect = pSprite ? &pSprite->rect : &_copiedRect;

	if (!pCopiedItems->size() && !pCopiedImages->size())
		return nullptr;          // do not add an empty list
  // ------------add bottom item

	HistoryDeleteItems* phd = nullptr;
	if (pSprite && pSprite->itemsDeleted)	// then on top of _items there is a HistoryDeleteItems
	{
		phd = (HistoryDeleteItems * ) _items[_items.size() - 1];	// pointer to HistoryDeleteItems
		_items.pop_back();
	}
	// -------------- add the bottom item --------
	int indexOfBottomItem = _items.size();
	HistoryPasteItemBottom* pb = new HistoryPasteItemBottom(this, indexOfBottomItem, pCopiedItems->size() + pCopiedImages->size());
	_AddItem(pb);
	if (pSprite)
	{
		pb->moved = pSprite->itemsDeleted ? 1 : 0;
		_AddItem(phd);
	}

	//----------- add screenshots
	for (ScreenShotImage si : *pCopiedImages)
	{
		si.itemIndex = _items.size();	// new historyItem will be here in _items
		int n = pImages->size();		// and here in pImages
		pImages->push_back(si);
		(*pImages)[n].Translate(topLeft, -1);
		HistoryScreenShotItem* p = new HistoryScreenShotItem(this, n);
		_AddItem(p);
	}
  // ------------ add scribble items
	HistoryScribbleItem* pdri;
	for (ScribbleItem di : *pCopiedItems)	// do not modify copied item list
	{
		di.Translate(topLeft, 0);
		pdri = new HistoryScribbleItem(this, di);
		_AddItem(pdri);
	}
  // ------------Add top item
	QRect rect = pCopiedRect->translated(topLeft);
	HistoryPasteItemTop* pt = new HistoryPasteItemTop(this, indexOfBottomItem, pCopiedItems->size() + pCopiedImages->size(), rect);
	if (pSprite)
		pt->moved = pSprite->itemsDeleted ? 1 : 0;		// mark it moved

	return _AddItem(pt);
}

HistoryItem* History::addRecolor(MyPenKind pk)
{
	if (!_nSelectedItemsList.size())
		return nullptr;          // do not add an empty list

	HistoryReColorItem* p = new HistoryReColorItem(this, _nSelectedItemsList, pk);
	return _AddItem(p);
}

HistoryItem* History::addInsertVertSpace(int y, int heightInPixels)
{
	HistoryInsertVertSpace* phi = new HistoryInsertVertSpace(this, y, heightInPixels);
	return _AddItem(phi);
}

HistoryItem* History::addScreenShot(int index)
{
	HistoryScreenShotItem* phss = new HistoryScreenShotItem(this, index);

	if (!_inLoad)
		(*pImages)[index].zOrder = _nextImageZorder = index;
	else
		(*pImages)[index].zOrder = _nextImageZorder++;

	(*pImages)[index].itemIndex = _items.size();	// _AddItem increases this

	return _AddItem(phss);
}

HistoryItem* History::addRotationItem(MyRotation rot)
{
	if (!_nSelectedItemsList.size())
		return nullptr;          // do not add an empty list
	HistoryRotationItem* phss = new HistoryRotationItem(this, rot, _selectionRect, _nSelectedItemsList);
	return _AddItem(phss);
}

HistoryItem* History::addRemoveSpaceItem(QRect& rect)
{
	bool bNothingAtRight = _nItemsRightOfList.isEmpty(),
		bNothingAtLeftAndRight = bNothingAtRight && _nItemsLeftOfList.isEmpty(),
		bJustVerticalSpace = bNothingAtLeftAndRight && _nIndexOfFirstScreenShot != 0x70000000;

	if ((bNothingAtRight && !bNothingAtLeftAndRight) || (bNothingAtLeftAndRight && !bJustVerticalSpace))
		return nullptr;

	// Here _nIndexofFirstScreenShot is less than 0x70000000
	HistoryRemoveSpaceItem* phrs = new HistoryRemoveSpaceItem(this, _nItemsRightOfList, _nIndexOfFirstScreenShot, _nItemsRightOfList.size() ? rect.width() : rect.height());
	return _AddItem(phrs);
}


//********************************************************************* History ***********************************

void History::TranslateAllItemsBelow(QPoint dy, int belowY) // from 'first' item to _actItem if they are visible and  top is >= minY
{
	auto it = std::lower_bound(_yxOrder.begin(), _yxOrder.end(), belowY, [&](int left, int right) { return _items[left]->Area().top() < belowY; });
	int from = it - _yxOrder.begin();
	for (; from < _yxOrder.size(); ++from)
		_yitems(from)->Translate(dy, belowY);
	_modified = true;
}

void History::Rotate(HistoryItem* forItem, MyRotation withRotation)
{
	if (forItem)
		forItem->Rotate(withRotation, _selectionRect);
}

void History::InserVertSpace(int y, int heightInPixels)
{
	QPoint dy = QPoint(0, heightInPixels);
	TranslateAllItemsBelow(dy, y);
}

QRect History::Undo()      // returns top left after undo
{
	int actItem = _items.size();
	if (!actItem || actItem == _readCount)		// no more undo
		return QRect();

	// ------------- first Undo top item
	HistoryItem* phi = _items[--actItem];
	int count	= phi->Undo();		// it will affect this many elements (copy / paste )
	QRect rect	=  phi->Area();     // area of undo

	// -------------then move item(s) to _redo list
	while (count--)
	{
		phi = _items[ actItem ];	// here so index is never negative 
		_redo.push_back(phi);

		// only scribble elements are in _yxOrder!
		if (phi->IsScribble())
		{
			int yi = _YIndexForIndex(actItem);
			_yxOrder.remove(yi);
			_bands.Remove(actItem);
		}

		_items.pop_back();	// we need _items for removing the yindex
		--actItem;
	}
	_modified = true;

	return rect;
}

int History::GetScribblesInside(QRect rect, HistoryItemVector &hv)
{
	ItemIndexVector iv;
	_bands.ItemsVisibleForArea(rect, iv);
	for (auto i : iv)
		hv.push_back(_items[i.index]);
	return hv.size();
}


/*========================================================
 * TASK:	Redo changes to history
 * PARAMS:	none
 * GLOBALS:	_items, _redo
 * RETURNS:	pointer to top element after redo
 * REMARKS: first element moved back from _redo list to 
 *			_items, then the element Redo() function is called
 *-------------------------------------------------------*/
HistoryItem* History::Redo()   // returns item to redone
{
	int actItem = _redo.size();
	if (!actItem)
		return nullptr;

	HistoryItem* phi = _redo[--actItem];

	int count = phi->RedoCount();

	while (count--)
	{
		phi = _redo[ actItem-- ];

		_push_back(phi);
		_redo.pop_back();
	}
	phi->Redo();
	
	_modified = true;

	return phi;
}


/*========================================================
 * TASK:	add index-th element of _items to selected list
 * PARAMS:	index: in _items or -1 If -1 uses last drawn item added
 * GLOBALS:
 * RETURNS:
 * REMARKS: -
 *-------------------------------------------------------*/
void History::AddToSelection(int index)
{
	if(index < 0)
		index = _items.size()-1;
	const HistoryItem* pitem = _items.at(index);
	_nSelectedItemsList.push_back( pitem->ItxFrom(index) );
	_selectionRect = _selectionRect.united(pitem->Area());

}

/*========================================================
 * TASK:   collects indices from _yxOrder for scribble 
 *			items that are inside a rectangular area
 *			into '_nSelectedItemsList', 
 *
 *			item indices for all items at left and
 *			right of the selected rectangle into
 *			lists '_nItemsLeftOf' and '_nItemsRightOf'
 *			respectively
 *
 *			also sets index of the first item, which is
 *			completely below the selected region into
 *			'_nIndexOfFirstScreenShot' and '-selectionRect'
 * PARAMS:	rect: (0,0) relative rectangle
 * GLOBALS:
 * RETURNS:	size of selected item list + lists filled
 * REMARKS: - even pasted items require a single index
 *			- only selects items whose visible elements
 *			  are completely inside 'rect' (pasted items)
 *			- '_selectionrect' is set even, when no items
 *				are in '_nSelectedItemsList'. In thet case
 *				it is set to equal rect
 *-------------------------------------------------------*/
int History::CollectItemsInside(QRect rect) // only 
{
	_nSelectedItemsList.clear();
	_nItemsRightOfList.clear();
	_nItemsLeftOfList.clear();
	_nIndexOfFirstScreenShot = 0x70000000;

	_selectionRect = QRect();     // minimum size of selection (0,0) relative!
	
	// first add images to list
	_bands.SelectItemsForArea(rect, _nItemsLeftOfList, _nSelectedItemsList, _nItemsRightOfList, _selectionRect);	
							// collect indices for visible, scribble elements completely inside 'rect'
	if (_nSelectedItemsList.isEmpty())		// save for removing empty space
		_selectionRect = rect;

	return _nSelectedItemsList.size();
}

int History::SelectTopmostImageFor(QPoint& p)
{
	_nSelectedItemsList.clear();
	_nItemsRightOfList.clear();
	_nItemsLeftOfList.clear();

	int i = ImageIndexFor(p);		// index in _ScreenShotImages
	if (i < 0)
		return i;

	ScreenShotImage& ssi = (*pImages)[i];
	ItemIndex ind = { ssi.zOrder, ssi.itemIndex };
	_nSelectedItemsList.push_back(ind);
	_selectionRect = ssi.Area();
	return i;
}


/*========================================================
 * TASK:   copies selected scribbles into '_copiedItems'
 *
 * PARAMS:	sprite: possible null pointer to sprite
 * GLOBALS:
 * RETURNS:
 * REMARKS: - if sprite is null copies items into 
 *				'_copiedItems' else into the sprite
 *			- origin of scribbles in '_copiedItems'
 *				will be (0,0)
 *			- '_selectionRect''s top left will also be (0,0)
 *-------------------------------------------------------*/
void History::CopySelected(Sprite *sprite)
{
	ScribbleItemVector* pCopiedItems = sprite ? &sprite->items : &_copiedItems;
	ScreenShotImageList *pCopiedImages = sprite ? &sprite->images : &_copiedImages;
	if (sprite)
		sprite->nSelectedItemsList = _nSelectedItemsList;

	if (!_nSelectedItemsList.isEmpty())
	{
		pCopiedItems ->clear();
		pCopiedImages->clear();

		for (auto ix : _nSelectedItemsList)  // absolute indices of visible items selected
		{
			const HistoryItem* item = _items[ix.index];
			if (ix.zorder < DRAWABLE_ZORDER_BASE)	// then image
			{
				ScreenShotImage* pbmi = &(*pImages)[dynamic_cast<const HistoryScreenShotItem*>(item)->which];
				pCopiedImages->push_back(*pbmi);
				(*pCopiedImages)[pCopiedImages->size() - 1].Translate( -_selectionRect.topLeft(), -1);
			}
			else
			{
				int index = 0; // index in source
				const ScribbleItem* pscrbl = item->GetVisibleScribble();
				while (pscrbl)
				{
					pCopiedItems->push_back(*pscrbl);
					(*pCopiedItems)[pCopiedItems->size() - 1].Translate( -_selectionRect.topLeft(), -1);
					pscrbl = item->GetVisibleScribble(++index);
				}
			}
		}
		_copiedRect = _selectionRect.translated( - _selectionRect.topLeft());

		std::sort(pCopiedItems->begin(), pCopiedItems->end(), [](ScribbleItem &pl, ScribbleItem &pr) {return pl.zOrder < pr.zOrder; });
		std::sort(pCopiedImages->begin(), pCopiedImages->end(), [](ScreenShotImage &pl, ScreenShotImage &pr) {return pl.zOrder < pr.zOrder; });

		if (sprite)
			sprite->rect = _copiedRect;	// (0,0, width, height)
	}
}

Sprite::Sprite(History* ph) :pHist(ph) 
{ 
	ph->CopySelected(this); 
}

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
	int nband = y / _bandHeight;
	Band newBand;
	int iband = _FindBandFor( y );		// check if this band exists
	if (iband == _bands.size() || _bands[iband].band_index != nband)	// all bands are either below this one
	{																	// or we found the first band above
		newBand.band_index = iband;
		_bands.insert(iband, newBand);
	}
	// else 	// found 
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
	int y = phi->Area().y(),
		h = y + phi->Area().height();
	int ib = -1;			// no item added yet
	while (y <= h)
	{
		if (ib < 0)			// only added once to count
			++_itemCount;
		ib = _AddBandFor(y);
		_Insert(_bands[ib], index);	// to items in band, z-ordered 
		y += _bandHeight;
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
	int y = phi->Area().y(),
		h = y + phi->Area().height();
	int ib = -1;		// no item removed yet
	while (y <= h)
	{
		if (ib < 0 && _itemCount)		// only remove once from count
			--_itemCount;
		ib = _GetBand(y);
		if (ib < 0)	// shouldn't !
			return;
		_bands[ib].Remove(index);	// to items in band, z-ordered 
		if (_bands[ib].indices.isEmpty())	// no items left in band
			_bands.removeAt(ib);
		y += _bandHeight;
	}
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
 *				inside that band into vectro hv
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
		if( (insidersOnly && rect.contains(phi->Area())) || (!insidersOnly && rect.intersects(phi->Area() ) ) )
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
	int topBand, bottomBand;    // indices of bands where this item starts and ends
	Band& band = _bands[bandIndex];
	auto inlineWithRect = [&](QRect& bndRect)	// returns bit #0: top id in line, bit#1: bottom is in line
	{
		return ((bndRect.y() >= rect.y() && bndRect.y() <= rect.bottom()) ? 1: 0) +
			((bndRect.bottom() >= rect.y() && bndRect.bottom() <= rect.bottom()) ? 2 : 0);
	};

	for (auto ind : band.indices)
	{
		HistoryItem* phi = (*_pHist)[ind.index];
		QRect& bndRect = phi->Area();
		int ilkind;
		
		if (rect.contains(bndRect))
		{
			hvInside.push_back(ind);
			unionRect = unionRect.united(bndRect);
		}
		else if ( (ilkind = inlineWithRect(bndRect)) )
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
void Bands::ItemsVisibleForArea(QRect& rect, ItemIndexVector &hv, bool insidersOnly)
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

void Bands::SelectItemsForArea(QRect& rect, ItemIndexVector& hvLeft, ItemIndexVector& hvInside, ItemIndexVector& hvRight, QRect &unionRect)
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
	}
	while (ib != _bands.size());
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
	for(int i=0; i < indices.size(); ++i)
		if (indices.at(i).index == ix)
		{
			indices.remove(i);
			break;
		}
}
