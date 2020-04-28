#include "history.h"

DrawnItem::DrawnItem(HistEvent he) noexcept : type(he) {}
DrawnItem::DrawnItem(const DrawnItem& di) { *this = di; }
DrawnItem::DrawnItem(const DrawnItem&& di){ *this = di; }
DrawnItem& DrawnItem::operator=(const DrawnItem& di)
{
	type = di.type;
	penKind = di.penKind;
	penWidth = di.penWidth;
	points = di.points;
	rect = di.rect;
	return *this;
}

DrawnItem& DrawnItem::operator=(const DrawnItem&& di)  noexcept
{
	type = di.type;
	penKind = di.penKind;
	penWidth = di.penWidth;
	points = di.points;
	rect = di.rect;
	return *this;
}

void DrawnItem::clear()
{
	points.clear();
	rect = QRect();
}

bool DrawnItem::IsExtension(QPoint& p, QPoint& p1, QPoint& p2) // vectors p->p1 and p1->p are parallel?
{
	//return false;       // DEBUG as it is not working yet

	if (p == p1)
		return true;    // nullvector may point in any direction :)

	QPoint vp = p - p1,  // not (0, 0)
		vpp = p1 - p2;

	// the two vectors point in the same direction when vpp.y()/vpp.x() == vp.y()/vp.x()
	// i.e to avoid checking for zeros in divison: 
	return vpp.y() * vp.x() == vp.y() * vpp.x();
}

void DrawnItem::add(QPoint p)
{

	if (rect.isNull())
		rect = QRect(p.x() - 4*penWidth, p.y() - 4*penWidth, 8 * penWidth, 8 * penWidth);      // single point
	else if (!rect.contains(p))
	{
		if (rect.x() >= p.x() - penWidth) rect.setX(p.x() - 4*penWidth);
		if (rect.y() >= p.y() - penWidth) rect.setY(p.y() - 4*penWidth);
		if (rect.right() <= p.x() + penWidth) rect.setRight(p.x() + 6*penWidth + 1);
		if (rect.bottom() <= p.y() + penWidth) rect.setBottom(p.y() + 6*penWidth + 1);
	}
	// DEBUG
	// qDebug("point: (%d, %d), rect: (l: %d, t: %d, w: %d, h: %d)", p.x(), p.y(), rect.x(), rect.y(), rect.width(), rect.height());
	// /DEBUG            

	int n = points.size() - 1;


	if (n < 0)
		points.push_back(p);
	else                      // we need at least one point already in the array
	{   // if a point just extends the line in the same direction (IsExtension())
		// as the previous point was from the one before it
		// then do not add a new point, just modify the coordintes
		// (n 0: 1 point, >0: at least 2 points are already in the array
		if (n > 0 && IsExtension(p, points[n], points[n - 1]))  // then the two vector points in the same direction
			points[n] = p;                            // so continuation
		else
			points.push_back(p);
	}
}

void DrawnItem::add(int x, int y)
{
	QPoint p(x, y);
	add(p);
}

bool DrawnItem::intersects(const QRect& arect) const
{
	return rect.intersects(arect);
}

void DrawnItem::Translate(QPoint dr)
{
	for (int i = 0; i < points.size(); ++i)
		points[i] = points[i] + dr;
	rect.translate(dr);
}

inline QDataStream& operator<<(QDataStream& ofs, const DrawnItem& di)
{
	ofs << (qint32)di.type << (qint32)di.penKind << (qint32)di.penWidth;
	ofs << (qint32)di.points.size();
	for (auto pt : di.points)
		ofs << (qint32)pt.x() << (qint32)pt.y();
	return ofs;
}

inline QDataStream& operator>>(QDataStream& ifs, DrawnItem& di)
{
	qint32 n;
	ifs >> n; di.type = static_cast<HistEvent>(n);
	ifs >> n; di.penKind = (MyPenKind)n;
	ifs >> n; di.penWidth = n;

	qint32 x, y;
	di.clear();

	ifs >> n;
	while (n--)
	{
		ifs >> x >> y;
		di.add(x, y);
	}
	return ifs;
}



//-------------------------------------------------------------------------
// a list of drawn elements
DrawnItem* DrawnItemList::Add(DrawnItem& dri)
{
	if (++lastDrawnIndex == dw.size())
		dw.push_back(dri);
	else
		dw[lastDrawnIndex] = dri;
	return &dw[lastDrawnIndex];
}
DrawnItem* DrawnItemList::Add(QVector<DrawnItem> dwother, QRect& Area, QPoint* pTopLeft)
{
	DrawnItem* p;
	Area = QRect();

	for (auto dri : dwother)
	{
		QRect driRect = QRect();

		for (QPoint& pt : dri.points) // correct for paste position
		{
			if (pTopLeft)
				pt += *pTopLeft;
			if (!Area.contains(pt))
				driRect = driRect.united(QRect(pt, QSize(1, 1))).adjusted(-2*dri.penWidth, -2*dri.penWidth, 2*dri.penWidth, 2*dri.penWidth);
		}
		dri.rect = driRect;
		Area = Area.united(driRect);

		p = Add(dri);          // return only the last one
	}
	return p;
}

DrawnItem& DrawnItemList::operator[](int index)
{
	return dw[index];
}

void DrawnItemList::Clear()
{
	dw.clear();
	lastDrawnIndex = -1;
}
void DrawnItemList::Purge()    // elements with invalid type from list
{
	int i = 0;
	for (auto pd = dw.begin(); pd != dw.end(); ++pd)
		if (pd->type == heNone)
		{
			++i;
			pd = dw.erase(pd);
			if (pd == dw.end())
				break;
		}
	if (i < lastDrawnIndex)
		lastDrawnIndex = i;
}
int DrawnItemList::Size()
{
	return dw.size();
}

/*========================================================
 * One history element
 *-------------------------------------------------------*/

 //--------------------------------------------
HistoryClearCanvasItem::HistoryClearCanvasItem(int drawnIndex) : HistoryItem(), drawnIndex(drawnIndex) { type = heVisibleCleared; }

//--------------------------------------------
// type heScribble, heEraser
HistoryDrawItem::HistoryDrawItem(DrawnItem& dri, DrawnItemList* pdri) : HistoryItem(), pDrawn(pdri)
{
	type = dri.type;
	isSaveable = true;
	encompassingRect = pDrawn->Add(dri)->rect;
	drawnIndex = pDrawn->lastDrawnIndex;
}
HistoryDrawItem::HistoryDrawItem(const HistoryDrawItem& other)
{
	*this = other;
}
HistoryDrawItem::HistoryDrawItem(const HistoryDrawItem&& other)
{
	*this = other;
}

bool HistoryDrawItem::Hidden() const  { return (*pDrawn)[drawnIndex].isDeleted; }

HistoryDrawItem& HistoryDrawItem::operator=(const HistoryDrawItem& other)
{
	type = other.type;
	isSaveable = true;
	pDrawn = other.pDrawn;
	drawnIndex = other.drawnIndex;
	encompassingRect = other.encompassingRect;
	return *this;
}
HistoryDrawItem& HistoryDrawItem::operator=(const HistoryDrawItem&& other)
{
	type = other.type;
	isSaveable = true;
	pDrawn = other.pDrawn;
	drawnIndex = other.drawnIndex;
	encompassingRect = other.encompassingRect;
	return *this;
}
bool HistoryDrawItem::IsSaveable() const  { return (*pDrawn)[drawnIndex].isDeleted; }
DrawnItem* HistoryDrawItem::GetDrawable(int index) const 
{
	return (index || (*pDrawn)[drawnIndex].isDeleted) ? nullptr : &(*pDrawn)[drawnIndex];
}
QRect HistoryDrawItem::Area() const  { return encompassingRect; }

//--------------------------------------------
bool HistoryDeleteItems::Undo() 
{
	for (auto i : deletedList)
		(*pDrawn)[i].isDeleted = false;
	//        hidden = false;
	return true;
}
bool HistoryDeleteItems::Redo() 
{
	for (auto i : deletedList)
		(*pDrawn)[i].isDeleted = true;
	//        hidden = true;
	return true;
}
//    bool Hidden() const  { return hidden; }
HistoryDeleteItems::HistoryDeleteItems(DrawnItemList* pdri, IntVector& selected) : HistoryItem(), pDrawn(pdri), deletedList(selected)
{
	type = heItemsDeleted;
	Redo();         // and mark them
}
HistoryDeleteItems::HistoryDeleteItems(HistoryDeleteItems& other) : HistoryDeleteItems(other.pDrawn, other.deletedList) {}
HistoryDeleteItems& HistoryDeleteItems::operator=(const HistoryDeleteItems& other)
{
	type = heItemsDeleted;
	pDrawn = other.pDrawn;
	deletedList = other.deletedList;
	//        hidden = other.hidden;
	return *this;
}
HistoryDeleteItems::HistoryDeleteItems(HistoryDeleteItems&& other) : HistoryDeleteItems(other.pDrawn, other.deletedList) {}
HistoryDeleteItems& HistoryDeleteItems::operator=(const HistoryDeleteItems&& other)
{
	type = heItemsDeleted;
	pDrawn = other.pDrawn;
	deletedList = other.deletedList;
	//        hidden = other.hidden;
	return *this;
}

//--------------------------------------------
bool HistoryPasteItem::Undo() 
{
	pDrawn->lastDrawnIndex = firstPastedIndex;
	return true;
}
bool HistoryPasteItem::Redo() 
{
	pDrawn->lastDrawnIndex = lastPastedIndex;
	return true;
}
HistoryPasteItem::HistoryPasteItem(DrawnItemList* pdri, QVector<DrawnItem>& pastedList, QPoint topLeft) :
	HistoryItem(), pDrawn(pdri)
{
	type = heItemsPasted;
	isSaveable = true;
	firstPastedIndex = pDrawn->lastDrawnIndex + 1;
	lastPastedIndex = firstPastedIndex + pastedList.size() - 1;
	pDrawn->Add(pastedList, encompassingRect, &topLeft);
}

HistoryPasteItem::HistoryPasteItem(HistoryPasteItem& other)
{
	*this = other;
}
HistoryPasteItem& HistoryPasteItem::operator=(const HistoryPasteItem& other)
{
	type = heItemsPasted;
	isSaveable = true;
	pDrawn = other.pDrawn;
	firstPastedIndex = other.firstPastedIndex;
	lastPastedIndex = other.lastPastedIndex;
	encompassingRect = other.encompassingRect;
	return *this;
}
HistoryPasteItem::HistoryPasteItem(HistoryPasteItem&& other)
{
	*this = other;
}
HistoryPasteItem& HistoryPasteItem::operator=(const HistoryPasteItem&& other)
{
	type = heItemsPasted;
	isSaveable = true;
	pDrawn = other.pDrawn;
	firstPastedIndex = other.firstPastedIndex;
	lastPastedIndex = other.lastPastedIndex;
	encompassingRect = other.encompassingRect;
	return *this;
}

bool HistoryPasteItem::IsSaveable() const 
{
	int n = 0;
	for (int i = firstPastedIndex; i <= lastPastedIndex; ++i)
		if ((*pDrawn)[i].isDeleted)
			++n;
	return n < pDrawn->Size();
}
DrawnItem* HistoryPasteItem::GetDrawable(int index) const 
{
	if (index < 0)
		return nullptr;
	index += firstPastedIndex;
	if (index > lastPastedIndex)
		return false;
	return (*pDrawn)[index].isDeleted ? nullptr : &(*pDrawn)[index];
}

QRect HistoryPasteItem::Area() const  { return encompassingRect; }

//--------------------------------------------
bool HistoryReColorItem::Undo() 
{
	for (int i = 0; i < selectedList.size(); ++i)
		(*pDrawn)[selectedList[i]].penKind = penKindList[i];
	return true;
}
bool HistoryReColorItem::Redo() 
{
	for (int i = 0; i < selectedList.size(); ++i)
	{
		penKindList[i] = (*pDrawn)[selectedList[i]].penKind;
		(*pDrawn)[selectedList[i]].penKind = pk;
	}
	return true;
}
HistoryReColorItem::HistoryReColorItem(DrawnItemList* pdri, MyPenKind pk, IntVector& selectedList) : HistoryItem(), pDrawn(pdri), pk(pk), selectedList(selectedList)
{
	penKindList.resize(selectedList.size());
	for (int i = 0; i < selectedList.size(); ++i)
		encompassingRectangle = encompassingRectangle.united((*pDrawn)[selectedList[i]].rect);
	type = heRecolor;
	Redo();
}

HistoryReColorItem::HistoryReColorItem(HistoryReColorItem& other)
{
	*this = other;
}

HistoryReColorItem& HistoryReColorItem::operator=(const HistoryReColorItem& other)
{
	type = heRecolor;
	pDrawn = other.pDrawn;
	selectedList = other.selectedList;
	penKindList = other.penKindList;
	pk = other.pk;
	return *this;
}

HistoryReColorItem::HistoryReColorItem(HistoryReColorItem&& other) : HistoryReColorItem(other.pDrawn, other.pk, other.selectedList)
{
	*this = other;
}

HistoryReColorItem& HistoryReColorItem::operator=(const HistoryReColorItem&& other)
{
	type = heRecolor;
	pDrawn = other.pDrawn;
	selectedList = other.selectedList;
	penKindList = other.penKindList;
	pk = other.pk;
	return *this;
}
QRect HistoryReColorItem::Area() const  { return encompassingRectangle; }

/*========================================================
 * Class for storing history of editing
 *-------------------------------------------------------*/
HistoryItem* History::_AddItem(HistoryItem* p)
{
	if (!p)
		return nullptr;

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
	return _items[_actItem];
}

int History::_GetStartIndex()        // find first drawable item in '_items' after a clear screen 
{                           // returns: 0 no item find, start at first item, -1: no items, (count+1) to next history item
	if (_actItem < 0)
		return -1;

	int i = _actItem;
	for (; i >= 0; --i)
		if (_items[i]->type == heVisibleCleared)
			return i + 1;
	// there were no iems to show
	return 0;
}

bool History::_IsSaveable(HistoryItem& item)         // when n drawables in it are deleted
{
	if (!item.isSaveable)
		return false;

	if (item.type == heScribble || item.type == heEraser)
	{
		HistoryDrawItem& hdi = dynamic_cast<HistoryDrawItem&>(item);
		return !_drawnItems[hdi.drawnIndex].isDeleted;
	}
	HistoryPasteItem& hpi = dynamic_cast<HistoryPasteItem&>(item);
	bool res = true;
	for (int i = hpi.firstPastedIndex; i <= hpi.lastPastedIndex; ++i)
		res &= !_drawnItems[i].isDeleted;
	return res;
}

bool History::_IsSaveable(int i)
{
	return _IsSaveable(*_items[i]);
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
	_actItem = -1;
	_endItem = 0;

	_drawnItems.Clear();
	_redoAble = false;
	_modified = false;
}
int History::size() const { return _items.size(); }

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
	QFile f(name);
	f.open(QIODevice::WriteOnly);

	if (!f.isOpen())
		return false;   // can't write file

	QDataStream ofs(&f);
	ofs << MAGIC_ID;
	// only save after last clear screen op.
	int i = _GetStartIndex();   // index of first item after a clear screen
	if (i)       // otherwise no clear screen item: save all drawn items
	{
		HistoryClearCanvasItem* pcsi = dynamic_cast<HistoryClearCanvasItem*>(_items[i - 1]);   // back to the clear screen item
		i = pcsi->drawnIndex + 1; // next drawn item is the first one shown
	}
	--i;
	// each 
	while (++i <= _drawnItems.lastDrawnIndex)
	{
		if (ofs.status() != QDataStream::Ok)
			return false;

		if (_drawnItems[i].isDeleted == false)
			ofs << _drawnItems[i];
	}
	_modified = false;
	return true;
}

int History::Load(QString name)  // returns _ites.size() when Ok, -items.size()-1 when read error
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
	_drawnItems.Clear();
	_actItem = -1;

	DrawnItem di;
	int i = 0;
	while (!ifs.atEnd())
	{
		ifs >> di;
		if (ifs.status() != QDataStream::Ok)
			return -_items.size() - 1;

		++i;

		addDrawnItem(di);
	}
	_modified = false;
	_endItem = _items.size();
	return _actItem = _items.size() - 1;
}

//--------------------- Add Items ------------------------------------------

HistoryItem* History::addDrawnItem(DrawnItem& itm)           // may be after an undo, so
{                                                  // delete all scribbles after the last visible one (items[lastItem].drawnIndex)
	HistoryDrawItem* p = new HistoryDrawItem(itm, &_drawnItems);
	return _AddItem(p);
}

HistoryItem* History::addDeleteItems()
{
	if (!_nSelectedItemsList.size())
		return nullptr;          // do not add an empty list
	HistoryDeleteItems* p = new HistoryDeleteItems(&_drawnItems, _nSelectedItemsList);
	return _AddItem(p);
}

HistoryItem* History::addPastedItems(QPoint topLeft)
{
	if (!_copiedItems.size())
		return nullptr;          // do not add an empty list

	HistoryPasteItem* p = new HistoryPasteItem(&_drawnItems, _copiedItems, topLeft);
	return _AddItem(p);
}

HistoryItem* History::addClearCanvas()
{
	HistoryClearCanvasItem* p = new HistoryClearCanvasItem(_drawnItems.lastDrawnIndex); // any drawable item after a clear screen is above this one
	return _AddItem(p);
}

HistoryItem* History::addRecolor(MyPenKind pk)
{
	if (!_nSelectedItemsList.size())
		return nullptr;          // do not add an empty list

	HistoryReColorItem* p = new HistoryReColorItem(&_drawnItems, pk, _nSelectedItemsList);
	return _AddItem(p);
}

DrawnItem* History::DrawnItemAt(int index)
{
	return (index < 0 || index >= _drawnItems.Size()) ? nullptr : &_drawnItems[index];
}

int History::SetFirstItemToDraw()
{
	return _index = _GetStartIndex();    // sets _index to first item after last clear screen
}

QRect  History::Undo()        // returns top left after undo 
{
	QRect rect;            // rectangle containing element ( (1,1) - not set)

	if (_actItem >= 0)    // undoable
	{
		_items[_actItem]->Undo();
		rect = _items[_actItem]->Area();     // area of undo

		--_actItem;
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
	if (_actItem == _endItem - 1)
	{
		_redoAble = false;
		return nullptr;
	}

	HistoryItem* phi = _items[++_actItem]; // in principle this should not go above
	if (phi)
		phi->Redo();

	_redoAble = _actItem < _endItem - 1;

	return phi;
}

int History::CollectItemsInside(QRect rect) // stores inices in _drawn
{

	_nSelectedItemsList.clear();

	_selectionRect = QRect();     // minimum size of selection
		// just check scribbles after the last clear screen
	for (int i = _GetStartIndex(); i <= _actItem; ++i)
	{
		const HistoryItem* item = _items[i];
		const DrawnItem* pdrni = item->GetDrawable();
		if (pdrni && (rect.contains(item->Area(), true)))    // union of rects in a pasted item
		{                       // here must be an item for drawing or pasting (others are undrawable)
			int j = 0;
			int selIndex = -1;
			do
			{
				if (item->type == heScribble || item->type == heEraser)
					selIndex = dynamic_cast<const HistoryDrawItem*>(item)->drawnIndex;
				else   // pasted
					selIndex = dynamic_cast<const HistoryPasteItem*>(item)->firstPastedIndex + j;
				_nSelectedItemsList.push_back(selIndex);  // index in _drawnItemList more than one when items pasted
			} while (pdrni = item->GetDrawable(++j));

			_selectionRect = _selectionRect.united(item->Area());
		}
	}
	return _nSelectedItemsList.size();
}

void History::CopySelected()      // copies selected scribbles into array. origin will be relative to (0,0)
{
	if (!_nSelectedItemsList.isEmpty())
	{
		DrawnItem ditem;
		_copiedItems.clear();
		for (auto i : _nSelectedItemsList)
		{
			ditem = _drawnItems[i];
			ditem.rect.translate(-_selectionRect.topLeft());
			for (QPoint& p : ditem.points)
				p -= _selectionRect.topLeft();;
			_copiedItems.push_back(ditem);
		}
		_selectionRect.translate(-_selectionRect.topLeft());
	}
}
