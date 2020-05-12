#include "history.h"
#include <QMessageBox>
#include <QMainWindow>

DrawnItem::DrawnItem(HistEvent he) noexcept : type(he) {}
DrawnItem::DrawnItem(const DrawnItem& di) { *this = di; }
DrawnItem::DrawnItem(const DrawnItem&& di) { *this = di; }
DrawnItem& DrawnItem::operator=(const DrawnItem& di)
{
	type = di.type;
	penKind = di.penKind;
	penWidth = di.penWidth;
	points = di.points;
	bndRect = di.bndRect;
	return *this;
}

DrawnItem& DrawnItem::operator=(const DrawnItem&& di)  noexcept
{
	type = di.type;
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

void DrawnItem::Rotate(MyRotation rot, QRect encRect)	// rotate around the center of encRect
{
	int erx = encRect.x(), ery = encRect.y(),
		erw = encRect.width(),
		erh = encRect.height();
					// item bndRectangle
	int rx = bndRect.x(), ry = bndRect.y(), rw = bndRect.width(), rh = bndRect.height();	
				   // modify encompassing rectangle and create a big and a little square
				   // for rotation with sides erb and erl respectively
		 
		// if rotR90  -> x' = erx + ery + erh - y,	y' = ery - erx + x
		// if rotL90  -> x' = erx - ery + y,		y' = erx + ery + erw - x
		// rot180     -> x' = 2*erx + erw - x,		y' = 2*ery + erh - y

	int x, y;
	int A = erx + ery,
		B = erx - ery;

	auto SwapWH = [&](QRect& r)
	{
		bndRect.setWidth(bndRect.height());
		bndRect.setHeight(rw);
	};

	auto RotR90 = [&](QPoint& p)
	{
		x = A + erh - p.y();
		y = -B + p.x();
		p.setX(x); p.setY(y);
	};

	auto RotRectR90 = [&](QRect& r)		  // get new rectangle for transform
	{
		QPoint p = QPoint(r.x(), r.y() + r.height()); // bottom left will be top left after rotation
		RotR90(p);
		r.setTopLeft(p);
		SwapWH(r);
	};
	auto RotL90 = [&](QPoint& p)
	{
		x = B + p.y();
		y = A + erw - p.x();
		p.setX(x); p.setY(y);
	};
	auto RotRectL90 = [&](QRect& r)
	{
		QPoint p = QPoint(r.x() + r.width(), r.y()); // top right will be top left after rotation
		RotL90(p);
		r.setTopLeft(p);
		SwapWH(r);
	};

	QPoint tl;	// top left of transformed item rectangle

	switch (rot)
	{
		case rotR90:
			for (QPoint& p : points)
				RotR90(p);
			RotRectR90(bndRect);
			SwapWH(encRect);
			break;
		case rotL90:
			for (QPoint& p : points)
				RotL90(p);
			RotRectL90(bndRect);
			SwapWH(encRect);
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
				p.setX(erw - p.x() + 2 * erx);
			bndRect.setX(erw + 2 * erx - rx - rw);		  // new item x: original top righ (rx + rw)
			break;
		case rotFlipV:
			for (QPoint& p : points)
				p.setY(erh - p.y() + 2 * ery);
			bndRect.setY(erh + 2 * ery - ry - rh);		  // new item x: original bottom left (ry + rh)
			break;
		default:
			break;
	}
}

inline QDataStream& operator<<(QDataStream& ofs, const DrawnItem& di)
{
	ofs << (qint32)di.type << (qint32)di.penKind << (qint32)di.penWidth;
	ofs << (qint32)di.points.size();
	for (auto pt : di.points)
		ofs << (qint32)pt.x() << (qint32)pt.y();
	return ofs;
}
		   // reads ONLY after the type is read in!
inline QDataStream& operator>>(QDataStream& ifs, DrawnItem& di)
{
	qint32 n;
//	ifs >> n; di.type = static_cast<HistEvent>(n);
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

inline QDataStream& operator<<(QDataStream& ofs, const BelowImage& bimg)
{
	ofs << (int)heScreenShot << bimg.topLeft.x() << bimg.topLeft.y();
	ofs << bimg.image;

	return ofs;
}

inline QDataStream& operator>>(QDataStream& ifs, BelowImage& bimg)
{	 // type already read in
	int x, y;
	ifs >> x >> y;
	bimg.topLeft = QPoint(x, y);
	ifs >> bimg.image;
	return ifs;
}

/*========================================================
 * One history element
 *-------------------------------------------------------*/

 //--------------------------------------------
HistoryClearCanvasItem::HistoryClearCanvasItem(History* pHist) : HistoryItem(pHist)
{
	type = heClearCanvas;
}

//--------------------------------------------
// type heScribble, heEraser
HistoryDrawnItem::HistoryDrawnItem(History* pHist, DrawnItem& dri) : HistoryItem(pHist), drawnItem(dri)
{
	type = dri.type;
	drawnItem = dri;
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
	HistoryItem(pHist), index(index), count(count)
{
	type = heItemsPastedBottom;
}

HistoryPasteItemBottom::HistoryPasteItemBottom(HistoryPasteItemBottom& other) :
	HistoryItem(pHist), index(other.index), count(other.count)
{
	type = heItemsPastedBottom;
}

HistoryPasteItemBottom& HistoryPasteItemBottom::operator=(const HistoryPasteItemBottom& other)
{
	type = heItemsPastedBottom;
	index = other.index;
	count = other.count;
	return *this;
}

int HistoryPasteItemBottom::Redo()		// only for bottom item: make items above this visible
{
	for (int i = 1; i <= count; ++i) //0th element is this
		(*pHist)[index + i]->SetVisibility(true);

	return count + 1;	// we are at bottom item -> change stack pointer to top item
}
//--------------------------------------------
HistoryPasteItemTop::HistoryPasteItemTop(History* pHist, int index, int count, QRect& rect) :
	HistoryItem(pHist), indexOfBottomItem(index), count(count), boundingRect(rect)
{
	type = heItemsPastedTop;
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

int  HistoryPasteItemTop::Undo() // only for top item
{
	SetVisibility(false);
		
	return count + 2;	// decrease actual pointer below bottom item
}

bool HistoryPasteItemTop::Hidden() const
{
	return (*pHist)[indexOfBottomItem]->Hidden();
}

void HistoryPasteItemTop::SetVisibility(bool visible)
{
	for (int i = 1; i <= count; ++i)
		(*pHist)[indexOfBottomItem + i]->SetVisibility(visible);
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
//--------------------------------------------

HistoryScreenShotItem::HistoryScreenShotItem(History* pHist, int which) : HistoryItem(pHist), which(which)
{
	type = heScreenShot;
}

HistoryScreenShotItem::HistoryScreenShotItem(const HistoryScreenShotItem& other) : HistoryItem(pHist)
{
	*this = other;
}

HistoryScreenShotItem& HistoryScreenShotItem::operator=(const HistoryScreenShotItem& other)
{
	type = heScreenShot;
	which = other.which;
	isVisible = other.isVisible;
	return *this;
}

int  HistoryScreenShotItem::Undo() // hide
{
	(*pHist->pImages)[which].isVisible = false;
	isVisible = false;
	return 1;
}

int  HistoryScreenShotItem::Redo() // show
{
	(*pHist->pImages)[which].isVisible = true;
	isVisible = true;
	return 0;
}

QRect HistoryScreenShotItem::Area() const
{
	return QRect((*pHist->pImages)[which].topLeft, (*pHist->pImages)[which].image.size());
}

bool HistoryScreenShotItem::Hidden() const
{
	return !isVisible;
}

void HistoryScreenShotItem::SetVisibility(bool visible)
{
	isVisible = visible;
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

//---------------------------------------------------------

HistoryRotationItem::HistoryRotationItem(History* pHist, MyRotation rotation, QRect rect, IntVector selList) :
	HistoryItem(pHist), rot(rotation), nSelectedItemList(selList), encRect(rect)
{
	switch (rot)
	{
		case rotR90:
			deg = -90;
			break;
		case rotL90:
			deg = 90;
			break;
		case rot180:
			deg = 180;
			break;
		case rotFlipH:
			flipH = true;
			break;
		case rotFlipV:
			flipV = true;
			break;
	}
	encRect = encRect;
	Redo();
}

HistoryRotationItem::HistoryRotationItem(const HistoryRotationItem& other) :
	HistoryItem(other.pHist), rot(other.rot), nSelectedItemList(other.nSelectedItemList)
{
	deg = other.deg;
	flipV = other.flipV;
	flipH = other.flipH;
	encRect = other.encRect;
}

HistoryRotationItem& HistoryRotationItem::operator=(const HistoryRotationItem& other)
{
	pHist = other.pHist;
	nSelectedItemList = other.nSelectedItemList;
	deg = other.deg;
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
			break;
	}
	for (int n : nSelectedItemList)
		(*pHist)[n]->Rotate(rotation, encRect);
	return 1;
}

int HistoryRotationItem::Redo()
{
	for (int n : nSelectedItemList)
		(*pHist)[n]->Rotate(rot, encRect);
	return 0;
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

	_nSelectedItemsList.clear();	// no valid selection after new item
	_selectionRect = QRect();

	if (++_actItem == _items.size())
	{
		_items.push_back(p);
		_endItem = _actItem + 1;
	}
	else                            // overwrite previous last undone item
	{
		delete _items[_actItem];   // was new'd
		_items[_actItem] = p;
		_endItem = _actItem + 1;   // no redo after this
		// DEBUG
												// and delete above this (just to see anywhere else if I have some problems)
		for (int j = _actItem + 1; j < _items.size(); ++j)
		{
			delete _items[j];
			_items[j] = nullptr;
		}
		// /DEBUG
	}
	_redoAble = false;
	_modified = true;

	return _items[_actItem];
}


/*========================================================
 * TASK:   find first drawable and visible history item 
 * PARAMS:
 * GLOBALS:	_items
 * RETURNS:	index of first displayable item or -1 if no items
 * REMARKS: - because the order of the items is independent
 *				of the position of the item we must always 
 *				check all items until either run out of items
 *				or found HistoryVisibleCleared item
 *-------------------------------------------------------*/
int History::_GetStartIndex()        // in '_items' after a clear screen 
{									 // returns: 0 no item find, start at first item, -1: no items, (count+1) to next history item
	if (_actItem < 0)
		return -1;

	int i = _actItem;
	for (; i >= 0; --i)
	{
		if (_items[i]->type == heClearCanvas)
			break;
	}
	if (i < 0)		// no clear canvas
		++i;

	if (_items[i]->type == heClearCanvas)
		++i;	// to first item after the clear paper

	while (i <= _actItem && _items[i]->Hidden())
		++i;

	if (i > _actItem)
		i = -1;

	return i;		// 
}

bool History::_IsSaveable(int i)
{
	return _items[i]->IsSaveable();
}

History::~History()
{
	for (int i = 0; i < _items.size(); ++i)
		delete _items[i];
}

void History::clear()
{
	for (int i = 0; i < _items.size(); ++i)
		delete _items[i];

	_items.clear();
	pImages->Clear();
	_actItem = -1;
	_endItem = 0;

	_redoAble = false;
	_modified = false;
}
int History::size() const { return _items.size(); }

HistoryItem* History::operator[](int index)
{
	if (index < 0 || index >= _endItem)
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
	int i = _GetStartIndex();   // index of first visible item after a clear screen
	if (i < 0)					// no elements or no visible elements
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
	// each 
	while (++i < _endItem)
	{
		if (ofs.status() != QDataStream::Ok)
		{
			f.remove();
			return false;
		}

		if (!_items[i]->Hidden())
		{
			int index = -1;
			if (_items[i]->type == heScreenShot)
			{
				ofs << (*pImages)[((HistoryScreenShotItem*)_items[i])->which];
				continue;
			}
			DrawnItem* pdrni;
			while ((pdrni = _items[i]->GetDrawable(++index)) != nullptr)
				ofs << *pdrni;
		}
	}
	_modified = false;
	if (QFile::exists(name + "~"))
		QFile::remove(QString(name + "~"));
	QFile::rename(name, QString(name + "~"));

	f.rename(name);
	return true;
}

int History::Load(QString name, QPoint& lastPosition)  // returns _ites.size() when Ok, -items.size()-1 when read error
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
	_actItem = -1;
	lastPosition = QPoint(0, 0);

	DrawnItem di;
	BelowImage bimg;
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
			if ((*pImages)[n].topLeft.y() > lastPosition.y())	// for END only the greatest y coordinate counts
			{
				lastPosition.setX((*pImages)[n].topLeft.x());	// x is always set for left of last item shown
				lastPosition.setY((*pImages)[n].topLeft.y());
			}
			continue;
		}

		di.type = (HistEvent)n;

		ifs >> di;
		if (ifs.status() != QDataStream::Ok)
			return -_items.size() - 1;

		++i;

		HistoryItem* phi = addDrawnItem(di);
		di = ((HistoryDrawnItem*)phi)->drawnItem;

		if (di.bndRect.y() > lastPosition.y())
		{
			lastPosition.setX(di.bndRect.x());
			lastPosition.setY(di.bndRect.y());
		}
	}
	_modified = false;
	_endItem = _items.size();
	_actItem = _endItem - 1;
	return  _items.size();
}

//--------------------- Add Items ------------------------------------------

HistoryItem* History::addClearCanvas()
{
	HistoryClearCanvasItem* p = new HistoryClearCanvasItem(this); // any drawable item after a clear screen is above this one
	return _AddItem(p);
}

HistoryItem* History::addDrawnItem(DrawnItem& itm)           // may be after an undo, so
{                                                  // delete all scribbles after the last visible one (items[lastItem].drawnIndex)
	HistoryDrawnItem* p = new HistoryDrawnItem(this, itm);
	return _AddItem(p);
}

HistoryItem* History::addDeleteItems()
{
	if (!_nSelectedItemsList.size())
		return nullptr;          // do not add an empty list
	HistoryDeleteItems* p = new HistoryDeleteItems(this, _nSelectedItemsList);
	return _AddItem(p);
}

HistoryItem* History::addPastedItems(QPoint topLeft)			   // tricky
{
	if (!_copiedItems.size())
		return nullptr;          // do not add an empty list
  // add bottom item
	int indexOfBottomItem = _actItem+1;
	HistoryPasteItemBottom* pb = new HistoryPasteItemBottom(this, indexOfBottomItem, _copiedItems.size());
	_AddItem(pb);		  
  // Add drawable items
	HistoryDrawnItem* pdri;
	for (DrawnItem di : _copiedItems)	// do not modify copied item list
	{
		di.Translate(topLeft, 0);
		pdri = new HistoryDrawnItem(this, di);
		_AddItem(pdri);
	}
  // finish by adding top item
	QRect rect = _copiedRect.translated(topLeft);
	HistoryPasteItemTop* p = new HistoryPasteItemTop(this, indexOfBottomItem, _copiedItems.size(), rect);
	return _AddItem(p);
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
		bJustVerticalSpace = bNothingAtLeftAndRight && _nIndexOfFirstBelow != 0x7FFFFFFF;

	if ((bNothingAtRight && !bNothingAtLeftAndRight) || (bNothingAtLeftAndRight && !bJustVerticalSpace))
		return nullptr;

	// Here _nIndexofFirstBelow is less than 0x7FFFFFFF
	HistoryRemoveSpaceitem* phrs = new HistoryRemoveSpaceitem(this, _nItemsRightOfList, _nIndexOfFirstBelow, _nItemsRightOfList.size() ? rect.width() : rect.height());
	return _AddItem(phrs);
}

void History::Translate(int from, QPoint p, int minY) // from 'first' item to _actItem if they are visible and  >= minY
{
	for (; from < _actItem; ++from)
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
	int i = _GetStartIndex();   // index of first item after a clear screen
	if (i < 0) // there was a clear screen and we are after it
		return;;

	QPoint dy = QPoint(0, heightInPixels);
	Translate(i, dy, y);
}

int History::SetFirstItemToDraw()
{
	return _index = _GetStartIndex();    // sets _index to first item after last clear screen
}

QRect  History::Undo()        // returns top left after undo 
{
	QRect rect;            // rectangle containing element ( (1,1) - not set)
	int delta = 0;			// change actItem by this amount
	if (_actItem >= 0)    // undoable
	{
		delta = _items[_actItem]->Undo();
		rect = _items[_actItem]->Area();     // area of undo

		_actItem -= delta;
		//  MUST NOT BE THIS
		if (_actItem < -1)
			throw("FATAL");

		_index = _GetStartIndex();            // we may have just redone a clear screen

		_redoAble = true;
		_modified = true;
	}
	else
		_redoAble = false;

	return rect;
}

HistoryItem* History::GetOneStep()
{
	if (_actItem < 0 || _index > _actItem)
		return nullptr;

	return  _items[_index++];
}

HistoryItem* History::Redo()   // returns item to redo
{
	if (!_redoAble)
		return nullptr;
	if (_actItem >= _endItem - 1)
	{
		_redoAble = false;
		return nullptr;
	}

	HistoryItem* phi = _items[++_actItem]; // in principle this should not go above '_endItem'

	if (phi)
	{
		_actItem += phi->Redo();	// for pasted items this is the number of items to jump over + 1
		phi = _items[_actItem];
	}

	_redoAble = _actItem < _endItem - 1;

	return phi;
}


/*========================================================
 * TASK:   collects visible items since the last clear
 *			screen into '_nSelectedItemsList'
 *			item indices for all items left of and
 *			right of the selected rectangle into
 *			lists '_nItemsLeftOf' and '_nItemsRightOf'
 *			respectively
 *			also sets index of first iem, which is
 *			completely below the selected region into
 *			'_nIndexOfFirstBelow' and '-selectionRect'
 * PARAMS: rect: (0,0) relative rectangle
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
	_nIndexOfFirstBelow = 0x7FFFFFFF;

	_selectionRect = QRect();     // minimum size of selection (0,0) relative!
		// just check scribbles after the last clear screen
	int i = _GetStartIndex();
	if (i < 0)	// nothing to collect
		return 0;

	for (; i <= _actItem; ++i)
	{
		const HistoryItem* item = _items[i];
		if (item->Translatable())
		{
			if (rect.contains(item->Area(), true))    // union of rects in a pasted item
			{                       // here must be an item for drawing or pasting (others are undrawable)
				_nSelectedItemsList.push_back(i);				 // index in _items 
				_selectionRect = _selectionRect.united(item->Area());
			}
			else if (rightOfRect(rect, item->Area()))
				_nItemsRightOfList.push_back(i);
			else if (leftOfRect(rect, item->Area()))
				_nItemsLeftOfList.push_back(i);
			if (i < _nIndexOfFirstBelow && item->Area().y() > rect.y() + rect.height())
				_nIndexOfFirstBelow = i;
		}
	}
	if (_nSelectedItemsList.isEmpty())		// save for removing empty space
		_selectionRect = rect;

	return _nSelectedItemsList.size();
}


/*========================================================
 * TASK:   copies selected scribbles into '_copiedItems'
 *
 * PARAMS:
 * GLOBALS:
 * RETURNS:
 * REMARKS: - origin of drawables in '_copiedItems'
 *				will be (0,0)
 *			- '_selectionRect''s top left will also be (0,0)
 *-------------------------------------------------------*/
void History::CopySelected()
{
	if (!_nSelectedItemsList.isEmpty())
	{
		_copiedItems.clear();
		_copiedImages.clear();

		for (int i : _nSelectedItemsList)  // indices of visible items selected
		{
			const HistoryItem* item = _items[i];
			if (item->type == heScreenShot)
			{
				BelowImage* pbmi = &(*pImages)[dynamic_cast<const HistoryScreenShotItem*>(item)->which];
				_copiedImages.push_back(*pbmi);
			}
			else
			{
				int index = 0; // index in source
				const DrawnItem* pdrni = item->GetDrawable();
				while (pdrni)
				{
					_copiedItems.push_back(*pdrni);
					_copiedItems[_copiedItems.size() - 1].Translate(-_selectionRect.topLeft(), -1);
					pdrni = item->GetDrawable(++index);
				}
			}
		}
		_copiedRect = _selectionRect.translated(-_selectionRect.topLeft());
	}
}
