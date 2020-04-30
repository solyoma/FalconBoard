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
	type = heNone;
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
	di.points.clear();
	di.rect = QRect(0, 0, 0, 0);

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

 //--------------------------------------------
HistoryClearCanvasItem::HistoryClearCanvasItem(History* pHist) : HistoryItem(pHist) 
{ 
	type = heVisibleCleared; 
}

//--------------------------------------------
// type heScribble, heEraser
HistoryDrawnItem::HistoryDrawnItem(History* pHist, DrawnItem& dri) : HistoryItem(pHist), drawnItem(dri)
{
	type = dri.type;
	drawnItem = dri;
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
	drawnItem.isDeleted = !visible;
}

bool HistoryDrawnItem::Hidden() const
{ 
	return drawnItem.isDeleted; 
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
	return (index || Hidden() ) ? nullptr : const_cast<DrawnItem*>(&drawnItem);
}

QRect HistoryDrawnItem::Area() const  
{ 
	return drawnItem.rect; 
}

//--------------------------------------------
bool HistoryDeleteItems::Undo() 
{
	for (auto i : deletedList)
		(*pHist)[i]->SetVisibility(true);
	//        hidden = false;
	return true;
}
bool HistoryDeleteItems::Redo() 
{
	for (auto i : deletedList)
		(*pHist)[i]->SetVisibility(false);
	//        hidden = true;
	return true;
}
//    bool Hidden() const  { return hidden; }
HistoryDeleteItems::HistoryDeleteItems(History* pHist, IntVector& selected) : HistoryItem(pHist), deletedList(selected)
{
	type = heItemsDeleted;
	Redo();         // and mark them
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

HistoryPasteItem::HistoryPasteItem(History* pHist, DrawnItemVector& toPasteList, QRect& rect, QPoint topLeft) : 
		HistoryItem(pHist), pastedList(toPasteList), encompassingRect(rect)
{
	type = heItemsPasted;
	for (DrawnItem& di : pastedList)
		di.Translate(topLeft);
	encompassingRect.translate(topLeft);
}

HistoryPasteItem::HistoryPasteItem(HistoryPasteItem& other) : HistoryItem(other.pHist)
{
	*this = other;
}
HistoryPasteItem& HistoryPasteItem::operator=(const HistoryPasteItem& other)
{
	type = heItemsPasted;
	pastedList = other.pastedList;
	encompassingRect = other.encompassingRect;
	return *this;
}
HistoryPasteItem::HistoryPasteItem(HistoryPasteItem&& other) : HistoryItem(other.pHist)
{
	*this = other;
}
HistoryPasteItem& HistoryPasteItem::operator=(const HistoryPasteItem&& other)
{
	type = heItemsPasted;
	pastedList = other.pastedList;
	encompassingRect = other.encompassingRect;
	return *this;
}

bool HistoryPasteItem::Undo() 
{
	for (DrawnItem& di : pastedList)
		di.isDeleted = true;
	return true;
}
bool HistoryPasteItem::Redo() 
{
	for (DrawnItem& di : pastedList)
		di.isDeleted = false;
	return true;
}

bool HistoryPasteItem::Hidden() const
{
	return !pastedList.size() || pastedList[0].isDeleted;;
}

void HistoryPasteItem::SetVisibility(bool visible)
{
	for (DrawnItem& item : pastedList)
		item.isDeleted = visible;
}

DrawnItem* HistoryPasteItem::GetDrawable(int index) const 
{
	if (index < 0 || index >= pastedList.size())
		return nullptr;
	return pastedList[index].isDeleted ? nullptr : const_cast<DrawnItem *>(&pastedList[index]);
}

QRect HistoryPasteItem::Area() const  
{ 
	return encompassingRect; 
}

HistoryReColorItem::HistoryReColorItem(History* pHist, IntVector& selectedList, MyPenKind pk) : 
	HistoryItem(pHist), selectedList(selectedList), pk(pk)
{
	type = heRecolor;

	int n = 0;						  // set size for penKind array and get encompassing rectangle
	for (int i : selectedList)
	{
		encompassingRectangle = encompassingRectangle.united((*pHist)[i]->Area() );
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
//--------------------------------------------
bool HistoryReColorItem::Undo() 
{
	for (int i : selectedList)
	{
		int index = 0;
		DrawnItem* pdri;
		while( (pdri = (*pHist)[i]->GetDrawable(index)) )
			pdri->penKind = penKindList[index++];
	}
	return true;
}
bool HistoryReColorItem::Redo() 
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
	return true;
}
QRect HistoryReColorItem::Area() const  { return encompassingRectangle; }


/*========================================================
 * TASK:	add a new item to the history list
 * PARAMS:	p - item to add. Corresponding scribbles already
 *				added to pHist
 * GLOBALS:
 * RETURNS:
 * REMARKS: - all items 
 *-------------------------------------------------------*/
HistoryItem * History::_AddItem(HistoryItem * p)
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

int History::_GetStartIndex()        // find first drawable item in '_items' after a clear screen 
{									 // returns: 0 no item find, start at first item, -1: no items, (count+1) to next history item
	if (_actItem < 0)
		return -1;

	int i = _actItem;
	for (; i >= 0; --i)
		if (_items[i]->type == heVisibleCleared)
			return i + 1;
	// there were no items to show
	return 0;
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
	_actItem = -1;
	_endItem = 0;

	_redoAble = false;
	_modified = false;
}
int History::size() const { return _items.size(); }

HistoryItem* History::operator[](int index)
{
	if(index < 0 || index >= _endItem)
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
	QFile f(name+".tmp");
	f.open(QIODevice::WriteOnly);

	if (!f.isOpen())
		return false;   // can't write file

	QDataStream ofs(&f);
	ofs << MAGIC_ID;
	// only save after last clear screen op.
	int i = _GetStartIndex();   // index of first item after a clear screen
	if (i >= 0) // there was a clear screen and we are after it
		--i;
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
			int index=-1;
			DrawnItem* pdrni;
			while( (pdrni = _items[i]->GetDrawable(++index) ) != nullptr )
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
	int i = 0;
	while (!ifs.atEnd())
	{
		ifs >> di;
		if (ifs.status() != QDataStream::Ok)
			return -_items.size() - 1;

		++i;

		HistoryItem *phi = addDrawnItem(di);
		di = ((HistoryDrawnItem*)phi)->drawnItem;

		if (di.rect.x() > lastPosition.x())
			lastPosition.setX(di.rect.x());
		if (di.rect.y() > lastPosition.y())
			lastPosition.setY(di.rect.y());
	}
	_modified = false;
	_endItem = _items.size();
	return _actItem = _items.size() - 1;
}

//--------------------- Add Items ------------------------------------------

HistoryItem* History::addClearCanvasItem()
{
	HistoryClearCanvasItem* p = new HistoryClearCanvasItem(this);

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

HistoryItem* History::addPastedItems(QPoint topLeft)
{
	if (!_copiedItems.size())
		return nullptr;          // do not add an empty list

	HistoryPasteItem* p = new HistoryPasteItem(this, _copiedItems, _copiedRect, topLeft);
	return _AddItem(p);
}

HistoryItem* History::addClearCanvas()
{
	HistoryClearCanvasItem* p = new HistoryClearCanvasItem(this); // any drawable item after a clear screen is above this one
	return _AddItem(p);
}

HistoryItem* History::addRecolor(MyPenKind pk)
{
	if (!_nSelectedItemsList.size())
		return nullptr;          // do not add an empty list

	HistoryReColorItem* p = new HistoryReColorItem(this, _nSelectedItemsList, pk);
	return _AddItem(p);
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


/*========================================================
 * TASK:   collects visible items since the last clear 
 *			screen into '_nSelectedItemsList'
 * PARAMS: rect: (0,0) relative rectangle
 * GLOBALS:
 * RETURNS:
 * REMARKS: - even pasted items require a single index
 *			- only selects items whose visible elements
 *			  are completely inside 'rect' (pasted items)
 *-------------------------------------------------------*/
int History::CollectItemsInside(QRect rect) // only 
{

	_nSelectedItemsList.clear();

	_selectionRect = QRect();     // minimum size of selection (0,0) relative!
		// just check scribbles after the last clear screen
	for (int i = _GetStartIndex(); i <= _actItem; ++i)
	{
		const HistoryItem* item = _items[i];
		const DrawnItem* pdrni = item->GetDrawable(0);
		if (pdrni && (rect.contains(item->Area(), true)))    // union of rects in a pasted item
		{                       // here must be an item for drawing or pasting (others are undrawable)
			_nSelectedItemsList.push_back(i);				 // index in _items 
			_selectionRect = _selectionRect.united(item->Area());
		}
	}
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

		for (int i :_nSelectedItemsList)  // indices of visible items selected
		{
			const HistoryItem* item = _items[i];
			int index = 0; // index in source
			const DrawnItem* pdrni = item->GetDrawable();
			while (pdrni)
			{
				_copiedItems.push_back(*pdrni);
				_copiedItems[ _copiedItems.size()-1 ].Translate(-_selectionRect.topLeft());
				pdrni = item->GetDrawable(++index);
			}
		}
		_copiedRect = _selectionRect.translated(-_selectionRect.topLeft());
	}
}
