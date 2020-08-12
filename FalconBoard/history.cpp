#include "history.h"
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

DrawnItem::DrawnItem(HistEvent he, int zorder) noexcept : type(he), zOrder(zorder) {}
DrawnItem::DrawnItem(const DrawnItem& di) { *this = di; }
DrawnItem::DrawnItem(const DrawnItem&& di) { *this = di; }
DrawnItem& DrawnItem::operator=(const DrawnItem& di)
{
	type = di.type;
	zOrder = di.zOrder;
	penKind = di.penKind;
	penWidth = di.penWidth;
	points = di.points;
	bndRect = di.bndRect;
	return *this;
}

DrawnItem& DrawnItem::operator=(const DrawnItem&& di)  noexcept
{
	type = di.type;
	zOrder = di.zOrder;
	penKind = di.penKind;
	penWidth = di.penWidth;
	points = di.points;
	bndRect = di.bndRect;
	return *this;
}

void DrawnItem::clear()
{
	points.clear();
	bndRect = QRect();
	type = heNone;
}

bool DrawnItem::IsExtension(const QPoint& p, const QPoint& p1, const QPoint& p2) // vectors p->p1 and p1->p are parallel?
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

void DrawnItem::add(QPoint p)
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

void DrawnItem::add(int x, int y)
{
	QPoint p(x, y);
	add(p);
}

void DrawnItem::Smooth()
{
	// smoothing points so that small variations in them vanish
	// ???
}

void DrawnItem::SetBoundingRectangle()
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

bool DrawnItem::intersects(const QRect& arect) const
{
	return bndRect.intersects(arect);
}

void DrawnItem::Translate(QPoint dr, int minY)
{
	if (bndRect.y() < minY || !isVisible)
		return;

	for (int i = 0; i < points.size(); ++i)
		points[i] = points[i] + dr;
	bndRect.translate(dr);
}

void DrawnItem::Rotate(MyRotation rotation, QRect encRect)	// rotate around the center of encRect
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

inline QDataStream& operator<<(QDataStream& ofs, const DrawnItem& di)
{
	ofs << (qint32)di.type << di.zOrder << (qint32)di.penKind << (qint32)di.penWidth;
	ofs << (qint32)di.points.size();
	for (auto pt : di.points)
		ofs << (qint32)pt.x() << (qint32)pt.y();
	return ofs;
}
		   // reads ONLY after the type is read in!
inline QDataStream& operator>>(QDataStream& ifs, DrawnItem& di)
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

QRect ScreenShotImage::Area(const QRect& canvasRect) const
{
	return QRect(topLeft, QSize(image.width(), image.height())).intersected(canvasRect);
}

void ScreenShotImage::Translate(QPoint p, int minY)
{
	if (topLeft.y() >= minY)
		topLeft += p;

}

void ScreenShotImage::Rotate(MyRotation rot, QRect encRect)
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

QRect ScreenShotImageList::Area(int index, const QRect& canvasRect) const
{
	return (*this)[index].Area(canvasRect);
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
			if (!Area(_index, _canvasRect).isNull())
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

void ScreenShotImageList::Rotate(int which, MyRotation rot, QRect encRect)
{
	if (which < 0 || which >= size() || (*this)[which].isVisible)
		return;
	(*this)[which].Rotate(rot, encRect);
}


void ScreenShotImageList::Clear()
{
	QList<ScreenShotImage>::clear();
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
HistoryDrawnItem::HistoryDrawnItem(History* pHist, DrawnItem& dri) : HistoryItem(pHist, dri.type), drawnItem(dri)
{
	type = dri.type;
	drawnItem.SetBoundingRectangle();
}

HistoryDrawnItem::HistoryDrawnItem(const HistoryDrawnItem& other) : HistoryItem(other.pHist)
{
	*this = other;
}
HistoryDrawnItem::HistoryDrawnItem(const HistoryDrawnItem&& other) : HistoryItem(other.pHist)
{
	*this = other;
}

void HistoryDrawnItem::SetVisibility(bool visible)
{
	drawnItem.isVisible = visible;
}

bool HistoryDrawnItem::Hidden() const
{
	return !drawnItem.isVisible;
}

HistoryDrawnItem& HistoryDrawnItem::operator=(const HistoryDrawnItem& other)
{
	pHist = other.pHist;
	type = other.type;
	drawnItem = other.drawnItem;
	return *this;
}
HistoryDrawnItem& HistoryDrawnItem::operator=(const HistoryDrawnItem&& other)
{
	pHist = other.pHist;
	type = other.type;
	drawnItem = other.drawnItem;
	return *this;
}
int HistoryDrawnItem::ZOrder() const
{
	return drawnItem.zOrder;
}

DrawnItem* HistoryDrawnItem::GetDrawable(int index) const
{
	return (index || Hidden()) ? nullptr : const_cast<DrawnItem*>(&drawnItem);
}

QRect HistoryDrawnItem::Area() const
{
	return drawnItem.bndRect;
}

void HistoryDrawnItem::Translate(QPoint p, int minY)
{
	drawnItem.Translate(p, minY);
}

void HistoryDrawnItem::Rotate(MyRotation rot, QRect encRect)
{
	drawnItem.Rotate(rot, encRect);
}

//--------------------------------------------
int  HistoryDeleteItems::Undo()	  // reveal items
{
	for (auto i : deletedList)
		(*pHist)[i]->SetVisibility(true);
	return 1;
}
int  HistoryDeleteItems::Redo()	// hide items
{
	for (auto i : deletedList)
		(*pHist)[i]->SetVisibility(false);
	return 0;
}

HistoryDeleteItems::HistoryDeleteItems(History* pHist, IntVector& selected) : HistoryItem(pHist), deletedList(selected)
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
HistoryDeleteItems::HistoryDeleteItems(HistoryDeleteItems&& other) : HistoryDeleteItems(other.pHist, other.deletedList) {}
HistoryDeleteItems& HistoryDeleteItems::operator=(const HistoryDeleteItems&& other)
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
int  HistoryRemoveSpaceitem::Redo()
{
	if (modifiedList.isEmpty())	 // vertical movement
	{
		int minY = (*pHist)[first]->Area().y();
		pHist->Translate(first, QPoint(0, -delta), minY);	 // -delta < 0 move up
	}
	else	// horizontal movement
	{
		QPoint dr(-delta, 0);								// move left
		for (int i : modifiedList)
			(*pHist)[i]->Translate(dr, -1);
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
int  HistoryRemoveSpaceitem::Undo()
{
	if (modifiedList.isEmpty())	 // vertical movement
	{
		int minY = (*pHist)[first]->Area().y();
		pHist->Translate(first, QPoint(0, delta), minY);	  //delte > 0 move down
	}
	else	// horizontal movement
	{
		QPoint dr(delta, 0);								 // delta >0 -> move right
		for (int i : modifiedList)
			(*pHist)[i]->Translate(dr, -1);
	}
	return 1;
}

HistoryRemoveSpaceitem::HistoryRemoveSpaceitem(History* pHist, IntVector& toModify, int first, int distance) :
	HistoryItem(pHist), modifiedList(toModify), first(first), delta(distance)
{
	type = heSpaceDeleted;
	Redo();
}

HistoryRemoveSpaceitem::HistoryRemoveSpaceitem(HistoryRemoveSpaceitem& other) : HistoryItem(other.pHist)
{
	*this = other;
}

HistoryRemoveSpaceitem& HistoryRemoveSpaceitem::operator=(const HistoryRemoveSpaceitem& other)
{
	modifiedList = other.modifiedList;
	first = other.first;
	delta = other.delta;
	return *this;
}
HistoryRemoveSpaceitem::HistoryRemoveSpaceitem(HistoryRemoveSpaceitem&& other) : HistoryItem(other.pHist)
{
	*this = other;
}

HistoryRemoveSpaceitem& HistoryRemoveSpaceitem::operator=(const HistoryRemoveSpaceitem&& other)
{
	modifiedList = other.modifiedList;
	first = other.first;
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
HistoryPasteItemTop::HistoryPasteItemTop(HistoryPasteItemTop&& other) : HistoryItem(other.pHist)
{
	*this = other;
}
HistoryPasteItemTop& HistoryPasteItemTop::operator=(const HistoryPasteItemTop&& other)
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

void HistoryPasteItemTop::Rotate(MyRotation rot, QRect encRect)
{
	for (int i = 1; i <= count; ++i)
		(*pHist)[indexOfBottomItem + i]->Rotate(rot, encRect);
}

DrawnItem* HistoryPasteItemTop::GetDrawable(int which) const
{
	if (which < 0 || which >= count)
		return nullptr;
	return (*pHist)[indexOfBottomItem + 1 + which]->GetDrawable(0);
}

QRect HistoryPasteItemTop::Area() const
{
	return boundingRect;
}
//---------------------------------------------------
HistoryReColorItem::HistoryReColorItem(History* pHist, IntVector& selectedList, MyPenKind pk) :
	HistoryItem(pHist), selectedList(selectedList), pk(pk)
{
	type = heRecolor;

	int n = 0;						  // set size for penKind array and get encompassing rectangle
	for (int i : selectedList)
	{
		boundingRectangle = boundingRectangle.united((*pHist)[i]->Area());
		n += (*pHist)[i]->Size();
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

HistoryReColorItem::HistoryReColorItem(HistoryReColorItem&& other) : HistoryReColorItem(other.pHist, other.selectedList, other.pk)
{
	*this = other;
}

HistoryReColorItem& HistoryReColorItem::operator=(const HistoryReColorItem&& other)
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
	for (int i : selectedList)
	{
		int index = 0;
		DrawnItem* pdri;
		while ((pdri = (*pHist)[i]->GetDrawable(index)))
			pdri->penKind = penKindList[index++];
	}
	return 1;
}
int  HistoryReColorItem::Redo()
{
	for (int i : selectedList)
	{
		int index = 0;
		DrawnItem* pdri;
		while ((pdri = (*pHist)[i]->GetDrawable(index)))
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
	HistoryItem(pHist), minY(top), heightInPixels(pixelChange)
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
	minY = other.minY; heightInPixels = other.heightInPixels; pHist = other.pHist;
	return *this;
}

int  HistoryInsertVertSpace::Undo()
{
	pHist->InserVertSpace(minY, -heightInPixels);
	return 1;
}

int  HistoryInsertVertSpace::Redo()
{
	pHist->InserVertSpace(minY, heightInPixels);
	return 0;
}
QRect HistoryInsertVertSpace::Area() const
{
	return QRect(0, minY, 100, 100);
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

void HistoryScreenShotItem::Rotate(MyRotation rot, QRect encRect)
{
	(*pHist->pImages)[which].Rotate(rot, encRect);
}

ScreenShotImage* HistoryScreenShotItem::GetScreenShotImage() const
{
	return &(*pHist->pImages)[which];
}

//---------------------------------------------------------

HistoryRotationItem::HistoryRotationItem(History* pHist, MyRotation rotation, QRect rect, IntVector selList) :
	HistoryItem(pHist), rot(rotation), nSelectedItemList(selList), encRect(rect)
{
	encRect = encRect;
	Redo();
}

HistoryRotationItem::HistoryRotationItem(const HistoryRotationItem& other) :
	HistoryItem(other.pHist), rot(other.rot), nSelectedItemList(other.nSelectedItemList)
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
	encRect = other.encRect;

	return *this;
}

int HistoryRotationItem::Undo()
{
	MyRotation rotation = rot;
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
		default:
			break;
	}
	for (int n : nSelectedItemList)
		(*pHist)[n]->Rotate(rotation, encRect);
	SwapWH(encRect);

	return 1;
}

int HistoryRotationItem::Redo()
{
	for (int n : nSelectedItemList)
		(*pHist)[n]->Rotate(rot, encRect);
	if(rot != rotFlipH && rot != rotFlipV)
		SwapWH(encRect);
	return 0;
}


void History::_SaveClippingRect()
{
	_savedClps.push(_clpRect);
}

void History::_RestoreClippingRect()
{
	_clpRect = _savedClps.pop();
}

//********************************** History class ****************************
History::History(const History& o)					
{
	_yindex = o._yindex;
	_items = o._items;
	_yxOrder = o._yxOrder;
}

History::History(const History&& o)
{
	_yindex = o._yindex;
	_items = o._items;
	_yxOrder = o._yxOrder;
}

History::~History()
{
	clear();
}

/*========================================================
 * TASK:	Fet index of top-left-most element at or below xy.y
 *			in _items, using '_yOrder' (binary search)
 * PARAMS:	xy  - limiting point 
 * GLOBALS:
 * RETURNS: index of first element (>0) in ordered list _yxOrder
 *			which is at the same height or below xy
 * REMARKS: - y inreses downwards
 *-------------------------------------------------------*/
int History::_YIndexForXY(QPoint xy)
{
	IntVector::iterator it = std::lower_bound(_yxOrder.begin(), _yxOrder.end(), 0,     // 0: in place of 'right' - not used
											  [&](const int left, const int right)	   // left: _yorder[xxx]
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
int History::_YIndexWhichInside()
{
	int i0 = _YIndexForXY(_clpRect.topLeft() );		// any number of items above this may be OK
	int ib = -1,	// before i0
		ia = -1;	// after i0			- if not -1 then element found 

	if (i0 >= _yxOrder.size())
		return -1;

	HistoryItem	*pb = _yitems(i0), 	// pointers before and after selected
				*pa = pb;			// starting from the same actual selected

	if (pa && !pa->Hidden() && pa->Area().intersects(_clpRect))
		ia = i0;			// set found element index for 'after' elements

	int inc = 1;									// should test all elements above the i-th one but only do it for max 100
	while (inc < 100 && (i0-inc >= 0 || ia < 0))
	{
		pb = i0 - inc >= 0 ? _yitems(i0 - inc) : nullptr;
		pa = ia < 0 && (i0 + inc  < _yxOrder.size()) ? _yitems(i0 + inc) : nullptr;
		if ( pb && !pb->Hidden() && pb->Area().intersects(_clpRect)) 
			ib = i0 - inc;
		else if (pa && !pa->Hidden() && pa->Area().intersects(_clpRect)) // only check those that come after if not found already in before 
			ia = i0 + inc;
		++inc;
	}

	if (ib >= 0)
		return ib;
	else if (ia >= 0)
		return ia;
	else
		return ib; // -1
}
int History::_YIndexForIndex(int index)      // index: in unordered array, returns yindex in _yxOrder
{
	QPoint pt = _items[index]->TopLeft();
	return _YIndexForXY(pt);
}

void History::ReplaceItem(int index, HistoryItem* pi)     // index: in '_items'
{
	int yindex = _YIndexForIndex(index);    // must exist
	delete _items[index];   // was new'd
	_items[index] = pi;
	_yxOrder.remove(yindex);
	int yi = _YIndexForXY(pi->TopLeft());
	_yxOrder.insert(yi, index);
}

void History::RemoveItemsStartingAt(int index)  // index: into _items
{
	for (int j = index; j < _items.size(); ++j)
	{
		int yindex = _YIndexForIndex(j);    // must exist
		delete _items[j];
		_items[j] = nullptr;
		_yxOrder.remove(yindex);
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
	int s = _items.size();					 // hysical index to put into _yxOrder 
	_items.push_back(pi);					 // always append

	if (pi->IsDrawable())	// Only drawable elements are put into _yxOrder
	{
		int yi = _YIndexForXY(pi->TopLeft());    // yi either to element with same x,y values or the one which is higher or at the right
		if (yi == _yxOrder.size() )  // all elements were at xy less than for this one
			_yxOrder.push_back(s);
		else          // the yi-th element has the larger y , or same and larger x coordinate
			_yxOrder.insert(yi, s);
	}
}

int History::YIndexOfTopmost(bool onlyY)
{
	int yi;
	if (onlyY)
		return _YIndexForXY(_clpRect.topLeft());
	else
		yi = _YIndexWhichInside();

	if (yi < 0)
		return -1;

	--yi;		// incremented before use
	int s = _yxOrder.size();
	HistoryItem* phi = nullptr;
	while (++yi < s)
	{
		phi = atYIndex(yi);
		if (!phi->Hidden())
			return yi;          // found
	}
	return -1;                  // all hidden
}
HistoryItem* History::TopmostItem()            // in ordered vector
{
	_yindex = _YIndexForXY(_clpRect.topLeft()) - 1;          // incremented in 'NextVisibleItem()'
	return NextVisibleItem();
}

HistoryItem* History::NextVisibleItem() // indexLimit is set up in Topmost()
{
	int s = _yxOrder.size();
	HistoryItem* phi = nullptr;
	while (++_yindex < s)
	{
		phi = atYIndex(_yindex);
		if (!phi->Hidden())
			return phi;
	}
	return nullptr;  // nothing
}

IntVector History::VisibleItemsBelow(int height)
{
	IntVector iv;
	if (height != 0x7fffffff)
		height += _clpRect.y();

	HistoryItem* pi = TopmostItem();
	while (pi && pi->TopLeft().y() < height)
	{
		iv.push_back(_yxOrder[_yindex]);    // absolute indices
		pi = NextVisibleItem();
	}

	return iv;
}

QPoint History::BottomRightVisible() const
{
	QPoint pt;

	if (!_yxOrder.isEmpty())
	{
		int ix = _yxOrder.size() - 1;
		HistoryItem* phi;
		while (ix >= 0 && (phi = _yitems(ix))->Hidden())
			--ix;
		if (ix >= 0)
			pt = phi->Area().topLeft();
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

	_redo.clear();	// no undo after new item addade

	_modified = true;

	return _items[ _items.size() -1 ];		// always the last element
}


/*========================================================
 * TASK:   find first drawable and visible history item 
 * PARAMS:
 * GLOBALS:	_items
 * RETURNS:	index of first displayable item or -1 if no items
 * REMARKS: - item is displayable if it is visible and 
 *			  the its area contains topLeft's top (y)
 *			  and the left of its area is
 *-------------------------------------------------------*/
int History::_YIndexForFirstVisible(bool onlyY)        // in '_items' after a clear screen 
{									 // returns: 0 no item find, start at first item, -1: no items, (count+1) to next history item
	if (!_items.size())
		return -1;
	return YIndexOfTopmost(onlyY);
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

	_lastZorder = DRAWABLE_ZORDER_BASE;

	_modified = false;
}

int History::size() const 
{ 
	return _items.size(); 
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
 * REMARKS: - drawn items follow each other in the order
 *              they were used.
 *          - if a clear screen was somewhere it contains
 *              the index of the last drawn item before it
 *              so only items after it are saved
 *-------------------------------------------------------*/
bool History::Save(QString name)
{
	// only save after last clear screen op.

	_SaveClippingRect();

	_clpRect.setTopLeft(QPoint(0, 0));

	int yi = _YIndexForFirstVisible( );   // index of first visible item
	if (yi < 0)					// no elements or no visible elements
	{
		QMessageBox::information(nullptr, sWindowTitle, QObject::tr("Nothing to save"));
		return false;;
	}

	QFile f(name + ".tmp");
	f.open(QIODevice::WriteOnly);

	if (!f.isOpen())
	{
		_RestoreClippingRect();
		return false;   // can't write file
	}

	QDataStream ofs(&f);
	ofs << MAGIC_ID;
	ofs << MAGIC_VERSION;

	for (int i =0; i < _items.size(); ++i )
	{
		HistoryItem* ph = _items[i];		// ordered by y then x coordinate
		if (ph->Hidden())		// hidden elements are not saved
			continue;

		if (ofs.status() != QDataStream::Ok)
		{
			f.remove();
			_RestoreClippingRect();
			return false;
		}

		int index = -1;
		if (ph->type == heScreenShot)
		{
			ofs << (*pImages)[((HistoryScreenShotItem*)ph)->which];
			continue;
		}
		DrawnItem* pdrni;
		while ((pdrni = ph->GetDrawable(++index)) != nullptr)
			ofs << *pdrni;
	}
	_modified = false;
	if (QFile::exists(name + "~"))
		QFile::remove(QString(name + "~"));
	QFile::rename(name, QString(name + "~"));

	f.rename(name);

	_RestoreClippingRect();

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
	bool bZOrderSaved = false;
	ifs >> id;
	if (id != MAGIC_ID)
		return 0;
	ifs >> version;
	if ((version >> 24) != 'V')
		return 0;

	clear();

	_inLoad = true;

	DrawnItem di;
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

		HistoryItem* phi = addDrawnItem(di);

	}
	_modified = false;
	_inLoad = false;
	return  _items.size();
}

//--------------------- Add Items ------------------------------------------

HistoryItem* History::addClearRoll()
{
	_SaveClippingRect();
	_clpRect = QRect(0, 0, 0x7fffffff, 0x7fffffff);
	HistoryItem* pi = addClearVisibleScreen();
	_RestoreClippingRect();
	return pi;
}

HistoryItem* History::addClearVisibleScreen()
{
	IntVector tobeDeleted = VisibleItemsBelow();

	HistoryDeleteItems* p = new HistoryDeleteItems(this, tobeDeleted);
	return _AddItem(p);
}

HistoryItem* History::addClearDown()
{
	_SaveClippingRect();
	_clpRect.setHeight(0x7fffffff);
	HistoryItem *pi = addClearVisibleScreen();
	_RestoreClippingRect();
	return pi;
}

HistoryItem* History::addDrawnItem(DrawnItem& itm)			// may be after an undo, so
{				                                            // delete all scribbles after the last visible one (items[lastItem].drawnIndex)
	if (_inLoad)
	{
		if (itm.zOrder > _lastZorder)
			_lastZorder = itm.zOrder;
	}
	else
		itm.zOrder = _lastZorder++;		// each scribbles are above any image (until 10 million images)
	HistoryDrawnItem * p = new HistoryDrawnItem(this, itm);
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
	IntVector* pList = pSprite ? &pSprite->nSelectedItemsList : &_nSelectedItemsList;

	if (!pList->size())
		return nullptr;          // do not add an empty list
	HistoryDeleteItems* p = new HistoryDeleteItems(this, *pList);
	return _AddItem(p);
}

HistoryItem* History::addPastedItems(QPoint topLeft, Sprite *pSprite)			   // tricky
{
	DrawnItemVector       *pCopiedItems = pSprite ? &pSprite->items : &_copiedItems;      
	ScreenShotImageList	  *pCopiedImages = pSprite ? &pSprite->images : &_copiedImages;
	QRect				  *pCopiedRect = pSprite ? &pSprite->rect : &_copiedRect;

	if (!pCopiedItems->size() && !pCopiedImages->size())
		return nullptr;          // do not add an empty list
  // ------------add bottom item

	HistoryDeleteItems* phd = nullptr;
	if (pSprite)	// then on top of _items there is a HistoryDeleteItems
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
		pb->moved = 1;
		_AddItem(phd);
	}

	//----------- add screenshots
	for (ScreenShotImage si : *pCopiedImages)
	{
		pImages->push_back(si);
		int n = pImages->size() - 1;
		(*pImages)[n].Translate(topLeft, -1);
		HistoryScreenShotItem* p = new HistoryScreenShotItem(this, n);
		_AddItem(p);
	}
  // ------------ add drawable items
	HistoryDrawnItem* pdri;
	for (DrawnItem di : *pCopiedItems)	// do not modify copied item list
	{
		di.Translate(topLeft, 0);
		pdri = new HistoryDrawnItem(this, di);
		_AddItem(pdri);
	}
  // ------------Add top item
	QRect rect = pCopiedRect->translated(topLeft);
	HistoryPasteItemTop* pt = new HistoryPasteItemTop(this, indexOfBottomItem, pCopiedItems->size() + pCopiedImages->size(), rect);
	if (pSprite)
		pt->moved = 1;		// mark it moved

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
	{
		if (_lastImage < (*pImages)[index].zOrder)
			_lastImage = (*pImages)[index].zOrder + 1;
	}
	else
		(*pImages)[index].zOrder = _lastImage++;

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
		bJustVerticalSpace = bNothingAtLeftAndRight && _nIndexOfFirstScreenShot != 0x7FFFFFFF;

	if ((bNothingAtRight && !bNothingAtLeftAndRight) || (bNothingAtLeftAndRight && !bJustVerticalSpace))
		return nullptr;

	// Here _nIndexofFirstScreenShot is less than 0x7FFFFFFF
	HistoryRemoveSpaceitem* phrs = new HistoryRemoveSpaceitem(this, _nItemsRightOfList, _nIndexOfFirstScreenShot, _nItemsRightOfList.size() ? rect.width() : rect.height());
	return _AddItem(phrs);
}


//********************************************************************* History ***********************************

void History::Translate(int from, QPoint p, int minY) // from 'first' item to _actItem if they are visible and  top is >= minY
{
	for (; from < _items.size(); ++from)
		_items[from]->Translate(p, minY);
	_modified = true;
}

void History::Rotate(HistoryItem* forItem, MyRotation withRotation)
{
	if (forItem)
		forItem->Rotate(withRotation, _selectionRect);
}

void History::InserVertSpace(int y, int heightInPixels)
{
	
	int i = _YIndexForFirstVisible();
if (!i)
{
	return;;
}

	QPoint dy = QPoint(0, heightInPixels);
	Translate(i, dy, y);
}

//void History::ClearSprite()
//{
//	_spriteImages.clear();
//	_spriteItems.clear();
//	_spriteRect = QRect();
//}

int History::SetFirstItemToDraw()
{
	return _yindex = _YIndexForFirstVisible();    // sets _index to first item after last clear screen
}

QRect  History::Undo()      // returns top left after undo 
{
	int actItem = _items.size();
	if (!actItem)
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

		// only drawable elements are in _yxOrder!
		if (phi->IsDrawable())
		{
			int yi = _YIndexForIndex(actItem);
			_yxOrder.remove(yi);
		}

		_items.pop_back();	// we need _items for removing the yindex
		--actItem;
	}

//	_SaveClippingRect();
// ????	_clpRect.setTop()
	_yindex = _YIndexForFirstVisible();

// ????	_RestoreClippingRect();
	_modified = true;

	return rect;
}

HistoryItem* History::GetOneStep()
{
	if (_yindex >= _yxOrder.size())
		return nullptr;

	HistoryItem* pi;
	while ((pi = atYIndex(_yindex++)) != nullptr && (pi->Hidden()))
		;

	return pi;
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
 * TASK:   collects indices from _yxOrder for drawable 
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
 * RETURNS:	size of selecetd item list + lists filled
 * REMARKS: - even pasted items require a single index
 *			- only selects items whose visible elements
 *			  are completely inside 'rect' (pasted items)
 *			- '_selectionrect' is set even, when no items
 *				are in '_nSelectedItemsList'. In thet case
 *				it is set to equal rect
 *-------------------------------------------------------*/
int History::CollectItemsInside(QRect rect) // only 
{

	auto rightOfRect = [](QRect rect, QRect other) -> bool
	{
		return  (rect.y() < other.y() && rect.y() + rect.height() > other.y() + other.height()) &&
			(rect.x() + rect.width() < other.x());
	};
	auto leftOfRect = [](QRect rect, QRect other) -> bool
	{
		return (rect.y() < other.y() && rect.y() + rect.height() > other.y() + other.height()) &&
			(rect.x() > other.x() + other.width());

	};

	_nSelectedItemsList.clear();
	_nItemsRightOfList.clear();
	_nItemsLeftOfList.clear();
	_nIndexOfFirstScreenShot = 0x7FFFFFFF;

	_selectionRect = QRect();     // minimum size of selection (0,0) relative!
	
	// first add images to list

	_SaveClippingRect();
	_clpRect = rect;
	int yi = _YIndexForFirstVisible(true);		 // in _yxOrder, only using y coordinate (no check for intersection here
	_RestoreClippingRect();
	if (yi < 0)	// nothing to collect
		return 0;

	for (; yi < _yxOrder.size(); ++yi)
	{
		const HistoryItem* pitem = _yitems(yi);
		if (pitem->Translatable())
		{
			if (rect.contains(pitem->Area(), true))    // union of rects in a pasted item
			{                       // here must be an item for drawing or pasting (others are undrawable)
				_nSelectedItemsList.push_back( _yxOrder[yi] );				 // index in _items !
				_selectionRect = _selectionRect.united(pitem->Area());
			}
			else if (rightOfRect(rect, pitem->Area()))
				_nItemsRightOfList.push_back(yi);
			else if (leftOfRect(rect, pitem->Area()))
				_nItemsLeftOfList.push_back(yi);
			if (yi < _nIndexOfFirstScreenShot && pitem->Area().y() > rect.y() + rect.height())
				_nIndexOfFirstScreenShot = yi;
		}
	}
	if (_nSelectedItemsList.isEmpty())		// save for removing empty space
		_selectionRect = rect;

	return _nSelectedItemsList.size();
}


/*========================================================
 * TASK:   copies selected scribbles into '_copiedItems'
 *
 * PARAMS:	sprite: possible null pointer to sprite
 * GLOBALS:
 * RETURNS:
 * REMARKS: - if sprite is null copies items into 
 *				'_copiedItems' else into the sprite
 *			- origin of drawables in '_copiedItems'
 *				will be (0,0)
 *			- '_selectionRect''s top left will also be (0,0)
 *-------------------------------------------------------*/
void History::CopySelected(Sprite *sprite)
{
	DrawnItemVector* pCopiedItems = sprite ? &sprite->items : &_copiedItems;
	ScreenShotImageList *pCopiedImages = sprite ? &sprite->images : &_copiedImages;
	if (sprite)
		sprite->nSelectedItemsList = _nSelectedItemsList;

	if (!_nSelectedItemsList.isEmpty())
	{
		pCopiedItems ->clear();
		pCopiedImages->clear();

		for (int i : _nSelectedItemsList)  // absolute indices of visible items selected
		{
			const HistoryItem* item = _items[i];
			if (item->type == heScreenShot)
			{
				ScreenShotImage* pbmi = &(*pImages)[dynamic_cast<const HistoryScreenShotItem*>(item)->which];
				pCopiedImages->push_back(*pbmi);
				(*pCopiedImages)[pCopiedImages->size() - 1].Translate( -_selectionRect.topLeft(), -1);
			}
			else
			{
				int index = 0; // index in source
				const DrawnItem* pdrni = item->GetDrawable();
				while (pdrni)
				{
					pCopiedItems->push_back(*pdrni);
					(*pCopiedItems)[pCopiedItems->size() - 1].Translate( -_selectionRect.topLeft(), -1);
					pdrni = item->GetDrawable(++index);
				}
			}
		}
		_copiedRect = _selectionRect.translated( - _selectionRect.topLeft());

		if (sprite)
			sprite->rect = _copiedRect;	// (0,0, width, height)
	}
}

Sprite::Sprite(History* ph) :pHist(ph) 
{ 
	ph->CopySelected(this); 
}