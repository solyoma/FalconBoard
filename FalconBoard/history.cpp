#include <QApplication>
#include <QMessageBox>
#include <QMainWindow>
#include <algorithm>

#include "history.h"
//#include <cmath>

HistoryList historyList;       // many histories are possible

//*****************************************************************************************************
// Histoy 
// ****************************************************************************************************


/*========================================================
 * One history element
 *-------------------------------------------------------*/

HistoryItem::HistoryItem(History* pHist, HistEvent typ) : pHist(pHist), type(typ), pDrawables(pHist->Drawables()) {}


bool HistoryItem::operator<(const HistoryItem& other)
{
	if (!IsSaveable() || IsHidden())
		return false;
	if (!other.IsSaveable() || other.IsHidden())
		return true;

	if (TopLeft().y() < other.TopLeft().y())
		return true;
	else if (TopLeft().y() == other.TopLeft().y())
	{
		if (TopLeft().x() < other.TopLeft().x())
			return true;
	}
	return false;
}



//-------------------------------------------- 
// HistoryDrawableItem
//--------------------------------------------
// expects a complete subclass cast as DrawableItem and adds a copy of it
HistoryDrawableItem::HistoryDrawableItem(History* pHist, DrawableItem& dri) : HistoryItem(pHist, HistEvent::heDrawable)
{
	DrawableItem* _pDrawable;
	switch (dri.dtType)
	{
		case DrawableType::dtDot:		_pDrawable = new DrawableDot(dynamic_cast<DrawableDot&>(dri));				break;
		case DrawableType::dtCross:		_pDrawable = new DrawableCross(dynamic_cast<DrawableCross&>(dri));			break;
		case DrawableType::dtEllipse:	_pDrawable = new DrawableEllipse(dynamic_cast<DrawableEllipse&>(dri));		break;
		case DrawableType::dtLine:		_pDrawable = new DrawableLine(dynamic_cast<DrawableLine&>(dri));			break;
		case DrawableType::dtRectangle:	_pDrawable = new DrawableRectangle(dynamic_cast<DrawableRectangle&>(dri));	break;
		case DrawableType::dtScribble:	_pDrawable = new DrawableScribble(dynamic_cast<DrawableScribble&>(dri));	break;
		case DrawableType::dtScreenShot:_pDrawable = new DrawableScreenShot(dynamic_cast<DrawableScreenShot&>(dri));break;
		case DrawableType::dtText:		_pDrawable = new DrawableText(dynamic_cast<DrawableText&>(dri));			break;
	}
	indexOfDrawable = pHist->AddToDrawables(_pDrawable);
}

HistoryDrawableItem::HistoryDrawableItem(History* pHist, DrawableItem* pdri) : HistoryItem(pHist, HistEvent::heDrawable)
{
	indexOfDrawable = pHist->AddToDrawables(pdri);
}

HistoryDrawableItem::HistoryDrawableItem(const HistoryDrawableItem& other) : HistoryItem(other.pHist)
{
	*this = other;
}
HistoryDrawableItem::HistoryDrawableItem(HistoryDrawableItem&& other) noexcept : HistoryItem(other.pHist)
{
	*this = other;
}

HistoryDrawableItem::~HistoryDrawableItem()
{
}

void HistoryDrawableItem::SetVisibility(bool visible)
{
	pDrawables->SetVisibility(indexOfDrawable,visible);	// also adds to or removes from quad tree
}

bool HistoryDrawableItem::IsVisible() const
{
	return _Drawable()->isVisible;
}

HistoryDrawableItem& HistoryDrawableItem::operator=(const HistoryDrawableItem& other)
{
	pHist = other.pHist;
	type = other.type;
	indexOfDrawable = other.indexOfDrawable;
	return *this;
}
HistoryDrawableItem& HistoryDrawableItem::operator=(HistoryDrawableItem&& other) noexcept
{
	pHist = other.pHist;
	type = other.type;
	indexOfDrawable = other.indexOfDrawable;
	return *this;
}
int HistoryDrawableItem::ZOrder() const
{
	return _Drawable()->zOrder;
}

int HistoryDrawableItem::Undo()
{
	pHist->Drawables()->Undo();	// also removes from Quad tree
	return 1;
}

int HistoryDrawableItem::Redo()
{
	pHist->Drawables()->Redo(); // also adds to Quad tree
	return 1;
}

//-------------------------------------------- 
// HistoryDeleteItem
//--------------------------------------------
DrawableItem* HistoryDrawableItem::GetDrawable(bool onlyVisible, int* pIndex) const
{
	if (pIndex)
		*pIndex = indexOfDrawable;
	return onlyVisible && IsHidden() ? nullptr : const_cast<DrawableItem*>(_Drawable());
}

QRectF HistoryDrawableItem::Area() const
{
	return _Drawable()->Area();
}

void HistoryDrawableItem::Translate(QPointF p, int minY)
{
	pDrawables->TranslateDrawable(indexOfDrawable,p, minY);
}

void HistoryDrawableItem::Rotate(MyRotation rot, QRectF encRect, float alpha)
{
	pDrawables->RotateDrawable(indexOfDrawable,rot, encRect,alpha);
}

//--------------------------------------------
int  HistoryDeleteItems::Undo()	  // reveal items
{
	for (auto drix : deletedList)
		pDrawables->SetVisibility(drix, true);
	
	return 1;
}
int  HistoryDeleteItems::Redo()	// hide items
{
	for (auto drix : deletedList)
		pDrawables->SetVisibility(drix, false);
	return 0;
}

HistoryDeleteItems::HistoryDeleteItems(History* pHist, DrawableIndexVector& selected) : HistoryItem(pHist), deletedList(selected)
{
	type = HistEvent::heItemsDeleted;
	Redo();         // hide them
}
HistoryDeleteItems::HistoryDeleteItems(HistoryDeleteItems& other) : HistoryDeleteItems(other.pHist, other.deletedList) {}
HistoryDeleteItems& HistoryDeleteItems::operator=(const HistoryDeleteItems& other)
{
	type = HistEvent::heItemsDeleted;
	pHist = other.pHist;
	deletedList = other.deletedList;
	//        hidden = other.hidden;
	return *this;
}
HistoryDeleteItems::HistoryDeleteItems(HistoryDeleteItems&& other)  noexcept : HistoryDeleteItems(other.pHist, other.deletedList) {}
HistoryDeleteItems& HistoryDeleteItems::operator=(const HistoryDeleteItems&& other) noexcept
{
	type = HistEvent::heItemsDeleted;
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
		pDrawables->VertShiftItemsBelow(y, -delta);	 // -delta < 0 move up
	}
	else	// horizontal movement
	{
		QPointF dr(-delta, 0);								// move left
		for (auto index : modifiedList)
			(*pHist)[index]->Translate(dr, -1);
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
		pDrawables->VertShiftItemsBelow(y - delta, delta);	  //delta > 0 move down
	}
	else	// horizontal movement
	{
		QPointF dr(delta, 0);								 // delta >0 -> move right
		for (auto index : modifiedList)
			(*pHist)[index]->Translate(dr, -1);
	}
	return 1;
}

HistoryRemoveSpaceItem::HistoryRemoveSpaceItem(History* pHist, DrawableIndexVector& toModify, int distance, int y) :
	HistoryItem(pHist), modifiedList(toModify), delta(distance), y(y)
{
	type = HistEvent::heSpaceDeleted;
	Redo();
}

HistoryRemoveSpaceItem::HistoryRemoveSpaceItem(const HistoryRemoveSpaceItem& other) : HistoryItem(other.pHist)
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

HistoryRemoveSpaceItem& HistoryRemoveSpaceItem::operator=(HistoryRemoveSpaceItem&& other)  noexcept
{
	modifiedList = other.modifiedList;
	y = other.y;
	delta = other.delta;
	return *this;
}

//--------------------------------------------

HistoryPasteItemBottom::HistoryPasteItemBottom(History* pHist, int index, int count) :
	HistoryItem(pHist, HistEvent::heItemsPastedBottom), index(index), count(count)
{
}

HistoryPasteItemBottom::HistoryPasteItemBottom(HistoryPasteItemBottom& other) :
	HistoryItem(other.pHist, HistEvent::heItemsPastedBottom), index(other.index), count(other.count)
{
}

HistoryPasteItemBottom& HistoryPasteItemBottom::operator=(const HistoryPasteItemBottom& other)
{
	type = HistEvent::heItemsPastedBottom;
	index = other.index;
	count = other.count;
	return *this;
}

//--------------------------------------------
HistoryPasteItemTop::HistoryPasteItemTop(History* pHist, int index, int count, QRectF& rect) :
	HistoryItem(pHist, HistEvent::heItemsPastedTop), indexOfBottomItem(index), count(count), boundingRect(rect)
{
}

HistoryPasteItemTop::HistoryPasteItemTop(const HistoryPasteItemTop& other) :
	HistoryItem(other.pHist)
{
	*this = other;
}
HistoryPasteItemTop& HistoryPasteItemTop::operator=(const HistoryPasteItemTop& other)
{
	type = HistEvent::heItemsPastedTop;
	indexOfBottomItem = other.indexOfBottomItem;
	count = other.count;
	boundingRect = other.boundingRect;
	return *this;
}
HistoryPasteItemTop::HistoryPasteItemTop(HistoryPasteItemTop&& other)  noexcept : HistoryItem(other.pHist)
{
	*this = other;
}
HistoryPasteItemTop& HistoryPasteItemTop::operator=(HistoryPasteItemTop&& other) noexcept
{
	type = HistEvent::heItemsPastedTop;
	indexOfBottomItem = other.indexOfBottomItem;
	count = other.count;
	boundingRect = other.boundingRect;
	return *this;
}

int  HistoryPasteItemTop::Undo() // elements are in _items and will be moved to _redoList after this
{
	if (moved)	// then the first item above the bottom item is a history delete item
		(*pHist)[indexOfBottomItem + 1]->Undo();
	return count + moved + 2;	// decrease actual pointer below bottom item
}

int HistoryPasteItemTop::Redo()		// elements copied back to '_items' already, but not into _pDrawables
{
	if (moved)	// then the first item above the bottom item is a history delete item
		(*pHist)[indexOfBottomItem + 1]->Redo();

	return count + moved + 1;	// we are at bottom item -> change stack pointer to top item
}

bool HistoryPasteItemTop::IsVisible() const
{
	return (*pHist)[indexOfBottomItem]->IsVisible();
}

void HistoryPasteItemTop::SetVisibility(bool visible) // visible=false then Undo, true then redo
{
	for (int i = 1; i <= count; ++i)
	{
		HistoryDrawableItem* phd = (HistoryDrawableItem*)(*pHist)[indexOfBottomItem + moved + i];
		pDrawables->SetVisibility(phd->indexOfDrawable, visible);
	}
}

void HistoryPasteItemTop::Translate(QPointF p, int minY)
{
	for (int i = 1; i <= count; ++i)
		(*pHist)[indexOfBottomItem + i]->Translate(p, minY);

	if (boundingRect.y() >= minY)
		boundingRect.translate(p);
}

void HistoryPasteItemTop::Rotate(MyRotation rot, QRectF encRect, float alpha)
{
	for (int i = 1; i <= count; ++i)
		(*pHist)[indexOfBottomItem + i]->Rotate(rot, encRect);
}

DrawableItem* HistoryPasteItemTop::GetNthDrawable(int which) const
{
	if (which < 0 || which >= count)
		return nullptr;
	return (*pHist)[indexOfBottomItem + 1 + which]->GetDrawable();
}

DrawableItem* HistoryPasteItemTop::GetNthVisibleDrawable(int which) const
{
	if (which < 0 || which >= count)
		return nullptr;
	return (*pHist)[indexOfBottomItem + 1 + which]->GetDrawable(true);
}

QRectF HistoryPasteItemTop::Area() const
{
	return boundingRect;
}


//--------------------------------------------------- 
// HistoryReColorItem
//---------------------------------------------------
HistoryReColorItem::HistoryReColorItem(History* pHist, DrawableIndexVector& listOfSelected, FalconPenKind pk) :
	HistoryItem(pHist), selectedList(listOfSelected), pk(pk)
{
	type = HistEvent::heRecolor;
	int siz = selectedList.size() - 1;
	for (int ix = siz; ix >= 0; --ix)
	{
		DrawableItem* pdrwi = pHist->Drawable(selectedList[ix]);
		if (pdrwi->dtType != DrawableType::dtScreenShot)
			boundingRectangle = boundingRectangle.united(pdrwi->Area());
		else
			selectedList.remove(ix);
	}
	Redo();		// get original colors and set new color tp pk
}

HistoryReColorItem::HistoryReColorItem(HistoryReColorItem& other) : HistoryItem(other.pHist)
{
	*this = other;
}

HistoryReColorItem& HistoryReColorItem::operator=(const HistoryReColorItem& other)
{
	type = HistEvent::heRecolor;
	pHist = other.pHist;
	selectedList = other.selectedList;
	penKindList = other.penKindList;
	pk = other.pk;
	return *this;
}

HistoryReColorItem::HistoryReColorItem(HistoryReColorItem&& other)  noexcept : HistoryReColorItem(other.pHist, other.selectedList, other.pk)
{
	*this = other;
}

HistoryReColorItem& HistoryReColorItem::operator=(const HistoryReColorItem&& other) noexcept
{
	type = HistEvent::heRecolor;
	pHist = other.pHist;
	selectedList = other.selectedList;
	penKindList = other.penKindList;
	pk = other.pk;
	return *this;
}
int  HistoryReColorItem::Undo()
{
	int iact = 0;
	for (auto drix : selectedList)
	{
		DrawableItem* pdri = pHist->Drawable(drix);
		pdri->SetPenKind( penKindList[iact++]);
	}
	return 1;
}
int  HistoryReColorItem::Redo()
{
	for (auto drix : selectedList)	// selectedlist: indices of drawables in pHist
	{
		DrawableItem* pdri = pHist->Drawable(drix);
		penKindList.push_back(pdri->PenKind());
		pdri->SetPenKind( pk);
	}
	return 0;
}
QRectF HistoryReColorItem::Area() const { return boundingRectangle; }

//---------------------------------------------------

HistoryInsertVertSpace::HistoryInsertVertSpace(History* pHist, int top, int pixelChange) :
	HistoryItem(pHist), y(top), heightInPixels(pixelChange)
{
	type = HistEvent::heVertSpace;
	Redo();
}

HistoryInsertVertSpace::HistoryInsertVertSpace(const HistoryInsertVertSpace& other) : HistoryItem(other.pHist)
{
	*this = other;
}
HistoryInsertVertSpace& HistoryInsertVertSpace::operator=(const HistoryInsertVertSpace& other)
{
	type = HistEvent::heVertSpace;
	y = other.y; heightInPixels = other.heightInPixels; pHist = other.pHist;
	return *this;
}

int  HistoryInsertVertSpace::Undo()
{
	pHist->InserVertSpace(y + heightInPixels, -heightInPixels);
	return 1;
}

int  HistoryInsertVertSpace::Redo()
{
	pHist->InserVertSpace(y, heightInPixels);
	return 0;
}
QRectF HistoryInsertVertSpace::Area() const
{
	return QRectF(0, y, 100, 100);
}

//---------------------------------------------------------

HistoryRotationItem::HistoryRotationItem(History* pHist, MyRotation rotation, QRectF rect, DrawableIndexVector selList, float alpha) :
	HistoryItem(pHist), rot(rotation), rAlpha(alpha), driSelectedDrawables(selList), encRect(rect)
{
	encRect = encRect;
	Redo();
}

HistoryRotationItem::HistoryRotationItem(const HistoryRotationItem& other) :
	HistoryItem(other.pHist), rot(other.rot), rAlpha(other.rAlpha), driSelectedDrawables(other.driSelectedDrawables)
{
	flipV = other.flipV;
	flipH = other.flipH;
	encRect = other.encRect;
}

HistoryRotationItem& HistoryRotationItem::operator=(const HistoryRotationItem& other)
{
	pHist = other.pHist;
	driSelectedDrawables = other.driSelectedDrawables;
	flipV = other.flipV;
	flipH = other.flipH;
	rAlpha = other.rAlpha;
	encRect = other.encRect;

	return *this;
}

// helper
static void SwapWH(QRectF& r)
{
	int w = r.width();
	r.setWidth(r.height());
	r.setHeight(w);
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
			alpha = -rAlpha;
			break;
		default:
			break;
	}
	for (auto dri : driSelectedDrawables)
		(*pHist)[dri]->Rotate(rotation, encRect, alpha);
	SwapWH(encRect);

	return 1;
}

int HistoryRotationItem::Redo()
{
	for (auto n : driSelectedDrawables)
		(*pHist)[n]->Rotate(rot, encRect, rAlpha);
	if (rot != rotFlipH && rot != rotFlipV)
		SwapWH(encRect);
	return 0;
}


//****************** HistorySetTransparencyForAllScreenshotsItem ****************
HistorySetTransparencyForAllScreenshotsItems::HistorySetTransparencyForAllScreenshotsItems(History* pHist, QColor transparentColor, qreal fuzzyness) : fuzzyness(fuzzyness), transparentColor(transparentColor), HistoryItem(pHist)
{
	Redo();
}

int HistorySetTransparencyForAllScreenshotsItems::Redo()
{
	DrawableList* pdrbl = &pHist->_drawables;
	int siz = undoBase = pdrbl->Size(DrawableType::dtScreenShot); // for undo this will be the first position
	DrawableItem* psi, *psin;
	DrawableScreenShot* pds;
	//DrawableItemIndex int drix;
	int imgIndex = 0;
		// get indices for visible screenshots, needed for undo
	for (int i = 0; i < undoBase; ++i)
	{
		psi = (*pdrbl)[i];
		if (psi->dtType == DrawableType::dtScreenShot && psi->isVisible)
		{
			//drix = { dtScreenShot, i, psi->zOrder };
			affectedIndexList.push_back(i/*drix*/);

			pds = new DrawableScreenShot(*(DrawableScreenShot*)psi);	// copy of the old, so set new image
			QPixmap image = pds->Image();
			QBitmap bm = MyCreateMaskFromColor(image, transparentColor, fuzzyness);
			image.setMask(bm);
			pds->AddImage(image);
			psi->isVisible = false;			// hide original

			pdrbl->AddDrawable(pds);
		}
	}
	return 1;
}

int HistorySetTransparencyForAllScreenshotsItems::Undo()
{
	DrawableList* pdrbl = &pHist->_drawables;
	// first new image is at the first position saved

	pdrbl->Erase(undoBase);

	DrawableScreenShot* psi;
	for (int i = 0; i < affectedIndexList.size(); ++i)
	{
		psi = (DrawableScreenShot*)(*pdrbl)[affectedIndexList[i]];
		psi->isVisible = true;			// show original
	}
	return 1;
}


//****************** HistoryEraserStrokeItem ****************

HistoryEraserStrokeItem::HistoryEraserStrokeItem(History* pHist, DrawableItem& dri) : eraserPenWidth(dri.penWidth), eraserStroke(dri.ToPolygonF()), HistoryItem(pHist)
{
	pHist->GetDrawablesInside(dri.Area(), affectedIndexList);
	// not all drawables inside dri's area are really affected by 
	// the eraser strokes (or rectangle, or ellipse
	if (affectedIndexList.isEmpty())				// will not be used or saved in callers!
		return;
	Redo();
}

HistoryEraserStrokeItem::HistoryEraserStrokeItem(const HistoryEraserStrokeItem& o)  
	: eraserPenWidth(o.eraserPenWidth), eraserStroke(o.eraserStroke), affectedIndexList(o.affectedIndexList),subStrokesForAffected(o.subStrokesForAffected), HistoryItem(o)
{
	if (affectedIndexList.isEmpty())				// will not be used or saved in callers!
		return;
	Redo();
}

HistoryEraserStrokeItem& HistoryEraserStrokeItem::operator=(const HistoryEraserStrokeItem& o)
{
	(HistoryItem&)(*this) = (HistoryItem&)o;
	eraserPenWidth = o.eraserPenWidth; 
	eraserStroke = o.eraserStroke; 
	affectedIndexList = o.affectedIndexList; 
	subStrokesForAffected = o.subStrokesForAffected;
	return *this;
}

int HistoryEraserStrokeItem::Undo()
{
	for (int i = 0; i < affectedIndexList.size(); ++i)
	{
		DrawableItem* pdrwi = pHist->Drawable(affectedIndexList.at(i) );
		for (int j = 0; j < subStrokesForAffected[i]; ++j)
			pdrwi->RemoveLastEraserStroke();
	}
	subStrokesForAffected.clear();
	return 1;
}

int HistoryEraserStrokeItem::Redo()
{
	int nStrokes;
	for (auto ix : affectedIndexList)
	{
		DrawableItem* pdrwi = pHist->Drawable(ix);
		nStrokes = pdrwi->AddEraserStroke(eraserPenWidth, eraserStroke); // it may add more than one stroke to given DrawableItem
		subStrokesForAffected << nStrokes;
	}
	return 1;
}

//****************** HistoryPenWidthChangeItem ****************

HistoryPenWidthChangeItem::HistoryPenWidthChangeItem(History* pHist, qreal changeWidthBy) :
	dw(changeWidthBy), HistoryItem(pHist, HistEvent::hePenWidthChange)
{
	affectedIndexList = pHist->Selected();
	if (affectedIndexList.isEmpty())				// will not be used or saved in callers!
		return;
	Redo();
}

HistoryPenWidthChangeItem::HistoryPenWidthChangeItem(const HistoryPenWidthChangeItem &o) :
	affectedIndexList(o.affectedIndexList), dw(o.dw), HistoryItem(o)
{
	if (affectedIndexList.isEmpty())				// will not be used or saved in callers!
		return;
	Redo();
}

HistoryPenWidthChangeItem& HistoryPenWidthChangeItem::operator=(const HistoryPenWidthChangeItem& o)
{
	(HistoryItem&)(*this) = (HistoryItem&)o;
	affectedIndexList = o.affectedIndexList; 
	dw = o.dw;
	return *this;
}
int HistoryPenWidthChangeItem::Undo()
{
	for (auto ix : affectedIndexList)
	{
		DrawableItem* pdrwi = pHist->Drawable(ix);
		if (pdrwi && pdrwi->penWidth < 999)
		{
			if ((pdrwi->penWidth -= dw) < 0)
				pdrwi->penWidth = 0;
		}
	}
	return 1;
}
int HistoryPenWidthChangeItem::Redo()
{
	for (auto ix : affectedIndexList)
	{
		DrawableItem* pdrwi = pHist->Drawable(ix);
		if (pdrwi && pdrwi->penWidth < 999)
		{
			if ((pdrwi->penWidth += dw) < 0)
				pdrwi->penWidth = 0;
		}
	}
	return 1;
}



//********************************** History class ****************************
History::History(HistoryList* parent) noexcept: _parent(parent) 
{ 
	_quadTreeDelegate.SetUp();
	_drawables.SetQuadTreeDelegate(&_quadTreeDelegate).SetZorderStore(&_zorderStore);	// allocate objects and set them 
	_drawables.SetZorderStore(&_zorderStore);
}

History::History(const History& o)
{
	_parent = o._parent;
	_items = o._items;
	_redoList = o._redoList;
	_drawables = o._drawables;
	_quadTreeDelegate = o._quadTreeDelegate;

	_zorderStore = o._zorderStore;
}

History::History(History&& o) noexcept
{
	_parent = o._parent;
	_items = o._items;
	_redoList = o._redoList;
	_drawables = o._drawables;
	_quadTreeDelegate = o._quadTreeDelegate;
	_zorderStore = o._zorderStore;

	o._items.empty();
	o._parent	= nullptr;
}

History::~History()
{
	Clear(true);
}

void History::_SaveClippingRect()
{
	_savedClps.push(_clpRect);
}

void History::_RestoreClippingRect()
{
	_clpRect = _savedClps.pop();
}

QSizeF History::UsedArea()
{
	QRectF rect;
	for (auto item : _items)
	{
		if (!item->IsHidden())
			rect = rect.united(item->Area());
	}
	return rect.size();
}

int History::CountOnPage(int px, int py, QSize pageSize, bool &getAreaSize)	// px, py = 0, 1, ...
{
	static QSizeF usedSize;
	if (getAreaSize)
	{
		usedSize = UsedArea();
		getAreaSize = false;
	}
	if (px * pageSize.width() > usedSize.width() || py * pageSize.height() > usedSize.height())
		return -1;
	QuadArea area(px * pageSize.width(), py * pageSize.height(), pageSize.width(), pageSize.height());
	return _drawables.Count(area);
}

/*=============================================================
 * TASK:	get drawable items that are right of rect
 * PARAMS : 'rect' - of which points of items must be at the right
 * GLOBALS : 
 * RETURNS :
 * REMARKS : 
 * ------------------------------------------------------------*/
int History::RightMostInBand(QRectF rect)
{
	IntVector iv;

	// only get areas till right of Quad are
	iv = _drawables.ListOfItemIndicesInRect(rect);

	if (iv.empty())
		return 0;
	int x = -1;
	for (auto ix : iv)
	{
		HistoryItem* phi = _items[ix];
		if (x < phi->Area().right())
			x = phi->Area().right();
	}
	return x;
}


QPointF History::BottomRightLimit(QSize &screenSize)
{
	return _drawables.BottomRightLimit(screenSize);
}


/*========================================================
 * TASK:	add a new item to the history list
 * PARAMS:	p - item to add. Corresponding drawables already
 *				added to '_drawables'
 * GLOBALS:
 * RETURNS:
 * REMARKS: - all items
 *-------------------------------------------------------*/
HistoryItem* History::_AddItem(HistoryItem* p)
{
	if (!p)
		return nullptr;

	_items.push_back(p);
		  // no undo after new item added
	for(auto a: _redoList)
		if (a->type == HistEvent::heDrawable)
		{
			_drawables.ClearRedo();			// any  Undo()-t drawbles must be removed
			break;
		}
	_redoList.clear();	

	_modified = true;

	int n = _items.size() - 1;
	p = _items[n];

	return p;		// always the last element
}

bool History::_IsSaveable(int i)
{
	return _items[i]->IsSaveable();
}

void History::Clear(bool andDeleteQuadTree)		// does not clear lists of copied items and screen snippets
{
	for (auto i : _items)
		delete i;
	for (auto i : _redoList)
		delete i;

	_items.clear();
	_redoList.clear();

	_drawables.Clear(andDeleteQuadTree);			// images and others + clear _pQTree and set it to nullptr
	_readCount = 0;
	_loadedName.clear();

	_modified = false;
}

int History::Size() const
{
	return _items.size();
}

int History::CountOfVisible()
{
	return _drawables.Count();
}

int History::CountButScreenShots()
{
	return _drawables.Count() - _drawables.CountOfScreenShots();
}

HistoryItem* History::LastScribble() const
{
	if (_items.size())
	{
		int ix = _items.size() - 1;
		while (ix >= 0 && !_items[ix]->IsDrawable())
				--ix;
		return ix >= 0 ? _items[ix] : nullptr;
	}
	else
		return nullptr;
}

HistoryItem* History::operator[](int index)	// absolute index
{
	if (index < 0 || index >= _items.size())
		return nullptr;
	return _items[index];
}

HistoryItem* History::operator[](DrawableItemIndex dri)	// absolute index
{
	return operator[](dri.index);
}

/*========================================================
 * TASK:    save actual visible drawable items
 * PARAMS:  name - full path name
 * GLOBALS:
 * RETURNS:
 * REMARKS:
 *-------------------------------------------------------*/
SaveResult History::Save(QString name)
{
	// only save visible items

	if (name != _loadedName)
		_loadedName = _fileName = name;

	if (_drawables.Count() == 0 && !_modified)					// no elements or no visible elements
	{
		QMessageBox::information(nullptr, sWindowTitle, QObject::tr("Nothing to save"));
		return srSaveSuccess;
	}

	QFile f(name + ".tmp");
	f.open(QIODevice::WriteOnly);

	if (!f.isOpen())
		return srFailed;   // can't write file

	QDataStream ofs(&f);
	ofs << MAGIC_ID;
	ofs << MAGIC_VERSION;	// file version


	QRectF area = QuadAreaToArea(_drawables.Area());
	DrawableItem* phi = _drawables.FirstVisibleDrawable(area); // first in zOrder
	while(phi)
	{
		ofs << *phi;

		if (ofs.status() != QDataStream::Ok)
		{
			f.remove();
			return srFailed;
		}
		phi = _drawables.NextVisibleDrawable();	// in increasing zOrder
	}
	_modified = false;
	if (QFile::exists(name + "~"))
		QFile::remove(QString(name + "~"));
	QFile::rename(name, QString(name + "~"));

	f.rename(name);

	return srSaveSuccess;
}


/*========================================================
 * TASK:	Loads saved file whose name is set into _fileName
 * PARAMS:	'force' load it even when already loaded
 *				in which case it overwrites data in memory
 * GLOBALS: _fileName, _inLoad,_readCount,_items,
 *			_screenShotImageList,_modified, 
 * RETURNS:   -1: file does not exist
 *			< -1: file read error number of records read so
 *				  far  is -(ret. value+2)
 *			   0: invalid file
 *			>  0: count of items read
 * REMARKS: - beware of old version files
 *			- when 'force' clears data first
 *			- both _fileName and _loadedName can be empty
 *-------------------------------------------------------*/
int History::Load(bool force)
{
	if (!force && _fileName == _loadedName)	// already loaded?
		return _readCount;

	QFile f(_fileName);
	f.open(QIODevice::ReadOnly);
	if (!f.isOpen())
		return -1;			// File not found

	QDataStream ifs(&f);
	qint32 id, version;
	ifs >> id;
	if (id != MAGIC_ID)
		return 0;			// read error
	ifs >> version;		// like 0x56020101 for V 2.1.1
	if ((version >> 24) != 'V')	// invalid/damaged  file or old version format
		return 0;

	Clear();
	
	int res;
	if ((version & 0x00FF0000) < 0x020000)
		res = _LoadV1(ifs, version,  force);
	else
		res = _LoadV2(ifs, force);

	f.close();
	return res;
}

int History::_LoadV2(QDataStream&ifs, bool force)
{
	DrawableItem dh;

	int n = 0;

// z order counters are reset
	DrawableItem di, * pdrwh;
	DrawableDot dDot;
	DrawableCross dCross;
	DrawableEllipse dEll;
	DrawableLine dLin;
	DrawableRectangle dRct;
	DrawableScribble dScrb;
	DrawableText dTxt;
	DrawableScreenShot dsImg;

	while (!ifs.atEnd())
	{
		++n;
		ifs >> di;

		switch (di.dtType)
		{
			case DrawableType::dtDot:			(DrawableItem&)dDot   = di; ifs >> dDot;	pdrwh = &dDot;	break;
			case DrawableType::dtCross:			(DrawableItem&)dCross = di; ifs >> dCross;	pdrwh = &dCross;break;
			case DrawableType::dtEllipse:		(DrawableItem&)dEll	  = di; ifs >> dEll;	pdrwh = &dEll;	break;
			case DrawableType::dtLine:			(DrawableItem&)dLin   = di; ifs >> dLin;	pdrwh = &dLin;	break;
			case DrawableType::dtRectangle:		(DrawableItem&)dRct   = di; ifs >> dRct;	pdrwh = &dRct;	break;
			case DrawableType::dtScreenShot:	(DrawableItem&)dsImg  = di; ifs >> dsImg;	pdrwh = &dsImg;	break;
			case DrawableType::dtScribble:		(DrawableItem&)dScrb  = di; ifs >> dScrb;	pdrwh = &dScrb;	break;
			case DrawableType::dtText:			(DrawableItem&)dTxt   = di; ifs >> dTxt;	pdrwh = &dTxt;	break;
			default: break;
		}
		(void)AddDrawableItem(*pdrwh); // this will add the drawable to the list and sets its zOrder too
		di.erasers.clear();
	}

	_loadedName = _fileName;

	return _readCount = n;
}


int History::_LoadV1(QDataStream &ifs, qint32 version, bool force)
{
	_inLoad = true;

	DrawableItem dh, * pdrwh;
	DrawableDot dDot;
	DrawableLine dLin;
	DrawableRectangle dRct;
	DrawableScribble dScrb;
	DrawableScreenShot dsImg;

// z order counters are reset
	int n;
	while (!ifs.atEnd())
	{
		ifs >> n;	// HistEvent				
		if ((HistEvent(n) == heScreenShot))
		{
			int x, y, zo;
			ifs >> dsImg.zOrder >> x >> y;
			dsImg.startPos = QPoint(x, y);

			QPixmap pxm;
			ifs >> pxm;
			dsImg. AddImage(pxm);
			pdrwh = &dsImg;
		}
		else
		{
			// only screenshots and scribble is possible in a ver 1.0 file
			pdrwh = &dScrb;

			dScrb.dtType = DrawableType::dtScribble;

			qint32 n;
			ifs >> n; dScrb.zOrder = n;
			ifs >> n; dScrb.SetPenKind( (FalconPenKind)(n & ~128));
			// DEBUG
			//if (dScrb.PenKind() == penEraser)
			//	i = i;
			// end DEBUG
			// dScrb.SetPenColor();	// from penKind
			bool filled = n & 128;
			ifs >> n; dScrb.penWidth = n;

			qint32 x, y;
			dScrb.Reset();

		di.type = (HistEvent)n;

			if (dScrb.points.size() == 2)
			{
				if (dScrb.points[0] == dScrb.points[1])	// then its a Dot
				{
					(DrawableItem&)dDot = (DrawableItem&)dScrb;
					dDot.dtType = DrawableType::dtDot;
					pdrwh = &dDot;
				}
				else
				{
					(DrawableItem&)dLin = (DrawableItem&)dScrb;
					dLin.dtType = DrawableType::dtLine;
					dLin.endPoint = dScrb.points[1];
					pdrwh = &dLin;
				}
			}
			else
			{
				auto isRectangle = [&](QPolygonF& poly)
				{
					return dScrb.points.size() == 5 &&
						dScrb.points[4] == dScrb.points[0] &&
						dScrb.points[1].y() == dScrb.points[0].y() &&
						dScrb.points[2].x() == dScrb.points[1].x() &&
						dScrb.points[3].y() == dScrb.points[2].y();
				};
				if (isRectangle(dScrb.points))
				{
					(DrawableItem&)dRct = (DrawableItem&)dScrb;
					dRct.dtType = DrawableType::dtRectangle;
					dRct.rect = dScrb.points.boundingRect();
					pdrwh = &dRct;
				}
			}
			// patch for older versions:
			if (version < 0x56010108)
			{
				if (pdrwh->PenKind()== penEraser)
					pdrwh->SetPenKind(penYellow);
				else if (pdrwh->PenKind()== penYellow)
					pdrwh->SetPenKind( penEraser);
			}
			// end patch
			if (ifs.status() != QDataStream::Ok)
			{
				_inLoad = false;
				return -(_items.size() + 2);
			}
		}
		(void)AddDrawableItem(*pdrwh);	// this will add the drawable to the list and sets its zOrder too
	}
	_modified = false;

	_loadedName = _fileName;

	return  _readCount = _items.size();
}

//--------------------- Add Items ------------------------------------------

HistoryItem* History::AddClearRoll()
{
	_SaveClippingRect();
	_clpRect = QRectF(0, 0, 0x70000000, 0x70000000);
	HistoryItem* pi = AddClearVisibleScreen();
	_RestoreClippingRect();
	return pi;
}

HistoryItem* History::AddClearVisibleScreen() // ???
{
	DrawableIndexVector toBeDeleted;

	HistoryDeleteItems* p = new HistoryDeleteItems(this, toBeDeleted);
	return _AddItem(p);
}

HistoryItem* History::AddClearDown()
{
	_SaveClippingRect();
	_clpRect.setHeight(0x70000000);
	HistoryItem* pi = AddClearVisibleScreen();
	_RestoreClippingRect();
	return pi;
}

HistoryItem* History::AddDrawableItem(DrawableItem& itm)	
{				                                            
	if (itm.dtType == DrawableType::dtLine)		  // then delete previous item if it was a DrawableDot
	{										      // that started this line
		int n = _items.size() - 1;
		HistoryItem* pitem = n < 0 ? nullptr : _items[n];
		if (pitem && pitem->type == HistEvent::heDrawable)
		{
			HistoryDrawableItem* phdi = (HistoryDrawableItem*)pitem;
			DrawableItem* pdrwi = _drawables[phdi->indexOfDrawable];
			if (pdrwi->dtType == DrawableType::dtDot)	// remove dot that was a start of a line
			{
				if (pdrwi->startPos == itm.startPos)
				{
					_drawables.Remove(phdi->indexOfDrawable);	// there'll be a gap in zOrders :)
					delete pitem;
					_items.pop_back();
				}
			}
		}
	}
	if (itm.PenKind() == penEraser)
		return AddEraserItem(itm);
	else
		return _AddItem( new HistoryDrawableItem(this, itm) );
	// may be after an undo, so
}							 // delete all items after the last visible one (items[lastItem].scribbleIndex)

HistoryItem* History::AddEraserItem(DrawableItem& era)
{	
	HistoryEraserStrokeItem* phe = new HistoryEraserStrokeItem(this, era);
	if (phe->affectedIndexList.isEmpty())
	{
		delete phe;
		return nullptr;
	}
	return _AddItem(phe);
}


/*========================================================
 * TASK:	hides visible items on list
 * PARAMS:	pSprite: NULL   - use _driSelectedDrawables
 *					 exists - use its selection list
 * RETURNS: pointer to delete item on _items
 * REMARKS:
 *-------------------------------------------------------*/
HistoryItem* History::AddDeleteItems(Sprite* pSprite)
{
	DrawableIndexVector* pList = pSprite ? &pSprite->driSelectedDrawables : &_driSelectedDrawables;

	if (!pList->size())
		return nullptr;          // do not add an empty list
	HistoryDeleteItems* p = new HistoryDeleteItems(this, *pList);
	return _AddItem(p);
}


/*========================================================
 * TASK:		add items copied or combined into a sprite
 * PARAMS:		topLeft - paste here
 *				pSprite - pointer to sprite	or nullptr
 * GLOBALS:
 * RETURNS:
 * REMARKS: - pasted items are sandwiched between a bootom
 *				and a top marker.
 *			- When the original items were deleted after
 *				added to a sprite a HistoryDeleteItem is at
 *				the top of the item stack. As we want the
 *				Undo for paste a single operation, we must
 *				undo this deleteion too, therefore we NEED
 *				the top and bottom markers even for a single
 *				pasted item
 *-------------------------------------------------------*/
HistoryItem* History::AddPastedItems(QPointF topLeft, Sprite* pSprite)			   // tricky
{
	DrawableList* pCopiedItems = pSprite ? &pSprite->drawables : &_parent->_copiedItems;
	QRectF* pCopiedRect = pSprite ? &pSprite->rect : &_parent->_copiedRect;

	if (!pCopiedItems->Size())	// any drawable
		return nullptr;          // do not add an empty list
  // ------------add bottom item

	HistoryDeleteItems* phd = nullptr;
	if (pSprite && pSprite->itemsDeleted)	// then on top of _items there is a HistoryDeleteItems
	{										// which has to be moved above the past bottom item
		phd = (HistoryDeleteItems*)_items[_items.size() - 1];	// pointer to HistoryDeleteItems
		_items.pop_back();
	}
	// -------------- add the bottom item --------
	int indexOfBottomItem = _items.size();
	HistoryPasteItemBottom* pb = new HistoryPasteItemBottom(this, indexOfBottomItem, pCopiedItems->Size());
	_AddItem(pb);
	if (pSprite)
	{
		pb->moved = pSprite->itemsDeleted ? 1 : 0;
		_AddItem(phd);
	}

	//----------- add drawables
	for (auto si : *pCopiedItems->Items())
	{
		si->Translate(topLeft, -1);
		//si->zOrder = -1;	// so it will be on top
		HistoryDrawableItem* p = new HistoryDrawableItem(this, *si);	   // si's data remains where it was
		_AddItem(p);
	}
	// ------------Add Paste top marker item
	QRectF rect = pCopiedRect->translated(topLeft);
	HistoryPasteItemTop* pt = new HistoryPasteItemTop(this, indexOfBottomItem, pCopiedItems->Size(), rect);
	if (pSprite)
		pt->moved = pSprite->itemsDeleted ? 1 : 0;		// mark it moved

	return _AddItem(pt);
}

HistoryItem* History::AddRecolor(FalconPenKind pk)
{
	if (!_driSelectedDrawables.size())
		return nullptr;          // do not add an empty list

	HistoryReColorItem* p = new HistoryReColorItem(this, _driSelectedDrawables, pk);
	if (p->selectedList.isEmpty())
	{
		delete p;
		return nullptr;
	}
	return _AddItem(p);
}

HistoryItem* History::AddInsertVertSpace(int y, int heightInPixels)
{
	HistoryInsertVertSpace* phi = new HistoryInsertVertSpace(this, y, heightInPixels);
	return _AddItem(phi);
}

HistoryItem* History::AddRotationItem(MyRotation rot)
{
	if (!_driSelectedDrawables.size())
		return nullptr;          // do not add an empty list
	HistoryRotationItem* phss = new HistoryRotationItem(this, rot, _selectionRect, _driSelectedDrawables);
	return _AddItem(phss);
}

HistoryItem* History::AddRemoveSpaceItem(QRectF& rect)
{
	int  nCntRight = _driSelectedDrawablesAtRight.size(),	// both scribbles and screenshots
		nCntLeft = _driSelectedDrawablesAtLeft.size();

	if ((!nCntRight && nCntLeft))
		return nullptr;

	HistoryRemoveSpaceItem* phrs = new HistoryRemoveSpaceItem(this, _driSelectedDrawablesAtRight, nCntRight ? rect.width() : rect.height(), rect.bottom());
	return _AddItem(phrs);
}

HistoryItem* History::AddScreenShotTransparencyToLoadedItems(QColor trColor, qreal fuzzyness)
{
	HistorySetTransparencyForAllScreenshotsItems* psta = new HistorySetTransparencyForAllScreenshotsItems(this, trColor, fuzzyness);
	return _AddItem(psta);
}

HistoryItem* History::AddPenWidthChange(int increment)
{
	HistoryPenWidthChangeItem* ppwch = new HistoryPenWidthChangeItem(this, increment);
	return _AddItem(ppwch);
}

//********************************************************************* History ***********************************

void History::Rotate(HistoryItem* forItem, MyRotation withRotation)
{
	if (forItem)
		forItem->Rotate(withRotation, _selectionRect);
}

void History::InserVertSpace(int y, int heightInPixels)
{
	_drawables.VertShiftItemsBelow(y, heightInPixels);
	_modified = true;
}

HistoryItem* History::Undo()      // returns item on top of _items or null
{
	int actItem = _items.size();
	if (!actItem || actItem == _readCount)		// no more undo
		return nullptr;

	// ------------- first Undo top item
	HistoryItem* phi = _items[--actItem];
	int count = phi->Undo()-1;		// it will affect (count+1) elements (copy / paste )

	_redoList.push_back(phi);		// move to redo list
	_items.pop_back();				// and remove from _items
	--actItem;						// because for Past items item hide and show is done 
									// in the corresponding items in _items

	// -------------then move item(s) from _items to _redoList
	//				and remove them from the quad tree
	// starting at the top of _items
	while (count--)
	{
		phi = _items[actItem];	// here again, index is never negative 
		_redoList.push_back(phi);
		phi->Undo();
		_items.pop_back();	// we need _items for removing the
		--actItem;
	}
	_modified = true;

	return actItem >= 0 ? _items[actItem] : nullptr;
}

int History::GetDrawablesInside(QRectF rect, IntVector& hv)
{
	hv = _drawables.ListOfItemIndicesInRect(rect);
	return hv.size();
}


/*========================================================
 * TASK:	Redo changes to history
 * PARAMS:	none
 * GLOBALS:	_items, _redoList
 * RETURNS:	pointer to top element after redo
 * REMARKS: first element moved back from _redoList list to
 *			_items, then the element Redo() function is called
 *-------------------------------------------------------*/
HistoryItem* History::Redo()   // returns item to redone
{
	int actItem = _redoList.size();
	if (!actItem)
		return nullptr;

	HistoryItem* phi = _redoList[--actItem];

	int count = phi->RedoCount();

	while (count--)
	{
		phi = _redoList[actItem--];

		_items.push_back(phi);
		_redoList.pop_back();
		phi->Redo();

	}

	_modified = true;

	return phi;
}


/*========================================================
 * TASK:	add index-th drawable used in _items to selected list
 * PARAMS:	drix: index of drawable in _drawables, or -1
 *					for the last visible drawable added
 * GLOBALS:
 * RETURNS:
 * REMARKS: -
 *-------------------------------------------------------*/
void History::AddToSelection(int drix, bool clearSelections)
{
	if (clearSelections)
	{
		_driSelectedDrawables.clear();
		_selectionRect = QRectF();
	}

	if (drix < 0)
		drix = _drawables.Size() - 1;
	while (drix >= 0 && !_drawables[drix]->isVisible)
		--drix;

	_driSelectedDrawables.push_back(drix);
	DrawableItem* pdri = _drawables[drix];
	_selectionRect = _selectionRect.united(pdri->Area());
}

/*========================================================
 * TASK:   collects indices for drawable items in _drawables'
 *			that are completely inside a rectangular area
 *				into '_driSelectedDrawables',
 *			that are at the right&left of 'rect' into
 *				'_driSelectedDrawablesAtRight' and '_driSelectedDrawablesAtLeft'
 *
 * PARAMS:	rect: document (0,0) relative rectangle
 * GLOBALS: _drawables,
 * RETURNS:	size of selected item list + lists filled
 * REMARKS: - even pasted items require a single index
 *			- only selects items whose visible elements
 *			  are completely inside 'rect' (pasted items)
 *			- '_selectionRect' is set even, when no items
 *				are in '_driSelectedDrawables'. In that case
 *				it is equal to 'rect'
 *-------------------------------------------------------*/
int History::CollectDrawablesInside(QRectF rect) // only 
{
	_ClearSelectLists();
	_selectionRect = QRectF();     // minimum size of selection document (0,0) relative!

	// first select all items inside a band whose top and bottom are set from 'rect'
	// but it occupies the whole width of the paper
	QuadArea area = QuadArea(0, rect.top(), _drawables.Area().Width(), rect.height());
	DrawableIndexVector iv = _drawables.ListOfItemIndicesInQuadArea(area);
	for (int i = iv.size()-1; i >= 0; --i)
	{
		int ix = iv[i];
		if (!rect.contains(_drawables[ix]->Area()))
			iv.erase(iv.begin() + i);
	}
	if (iv.size())
	{
		std::sort(iv.begin(), iv.end(), [this](int i, int j) { return _drawables[i]->zOrder < _drawables[j]->zOrder; });

		// then separate these into lists
		for (auto ix : iv)
		{
			DrawableItem* pdri = _drawables[ix];
			if (rect.contains(pdri->Area()))
			{
				_driSelectedDrawables.push_back(ix);
				_selectionRect = _selectionRect.united(pdri->Area());
			}
			else if (pdri->Area().left() < rect.left())
				_driSelectedDrawablesAtLeft.push_back(ix);
			else
				_driSelectedDrawablesAtRight.push_back(ix);
		}
		if (_driSelectedDrawables.isEmpty())		// save for removing empty space
			_selectionRect = rect;
	}
	return _driSelectedDrawables.size();
}

int History::SelectTopmostImageUnder(QPointF p)
{
	_driSelectedDrawables.clear();
	_driSelectedDrawablesAtRight.clear();
	_driSelectedDrawablesAtLeft.clear();

	int /*DrawableItemIndex*/ dri;
	dri = _drawables.IndexOfTopMostItemUnder(p, 1, DrawableType::dtScreenShot);
	if (dri < 0 )
		return dri;

	_driSelectedDrawables.push_back(dri);
	_selectionRect = _drawables[dri]->Area();
	return dri;
}

/*=============================================================
 * TASK:	select drawables under the cursor
 * PARAMS:	p - document relative coordinates of point
 *			addToPrevious: do not clear previous selection
 *					in _selectionRect, merge with these together
 * GLOBALS: 	_driSelectedDrawables		 - input and output
 *				_driSelectedDrawablesAtRight - may be cleared
 *				_driSelectedDrawablesAtLeft	 - may be cleared
 *				_selectionRect				 - input and output
 *
 * RETURNS:	area containing selected items. if invalid, then no items
 * REMARKS: - if no drawable items found then the selection lists
 *			  are not cleared
 *			- even when the area returned has other drawables
 *			  they are not included into the selection!
 *			- a drawable is found if for any of its points
 *			  its pen width sized area intersects the
 *			  area around 'p' 
 *------------------------------------------------------------*/
QRect History::SelectScribblesFor(QPoint& p, bool addToPrevious)
{
	int cnt = 0;
	int ii;
	IntVector iv;

	int w = 3;	// +-
	int pw;			// pen width

	QRect rp = QRect(p, QSize(1, 1)).adjusted(-w, -w, w, w),
		r;
	QRect result;

	ScribbleItem* pscr;
	ScribbleSubType typ;

	auto SQUARE = [](int a)->float { return a * a; };

	auto isNearToLine = [&](ScribbleItem* pscr, int i) -> bool
	{
		float x1 = pscr->points[i].x(), x2 = pscr->points[i + 1].x(),
			  y1 = pscr->points[i].y(), y2 = pscr->points[i + 1].y(),
			  x0 = p.x(), y0 = p.y();
//		if (y1 > y2) std::swap(y1, y2);

		float d = 0;

		if (x1 == x2 && y1 == y2)	// point
			d = sqrt(qAbs(x0 - x1) * qAbs(x0 - x1) + qAbs(y0 - y1) * qAbs(y0 - y1));
		else if (x1 == x2)	// vertical line
		{
			if (y1 > y0 || y0 > y2)
				return false;
			d = qAbs(x0 - x1);
		}
		else if (y1 == y2)
		{
			if (x1 > x0 || x0 > x2)
				return false;
			d = qAbs(y0 - y1);
		}
		else
		{
			if ( (y1 < y2 && (y1 > y0 || y0 > y2)) || 
				 (y2 < y1 && (y2 > y0 || y0 > y1)) || 
				 (x1 < x2 && (x1 > x0 || x0 > x2)) || 
				 (x2 < x1 && (x2 > x0 || x0 > x1)) )
						return false;

			d = qAbs((x2 - x1) * (y1 - y0) - (x1 - x0) * (y2 - y1)) / sqrt(SQUARE(x2 - x1) + SQUARE(y2 - y1));
		}
		return (int)d < pscr->penWidth + w;
	};

	auto addToIv = [&](ScribbleItem* pscr, int i)
	{
		++cnt;
		ii = i;
		iv.push_back(ii);
		result = result.united(pscr->bndRect);
	};

	for (int i = 0; i < _items.size(); ++i)
	{
		if (_items[i]->type == heScribble)
		{
			pscr = _items[i]->GetVisibleScribble();
			if (pscr)
			{
				typ = pscr->SubType();
				if (pscr && pscr->bndRect.contains(p))
				{
					pw = (pscr->penWidth + 1) / 2;
					if (typ != sstScribble)	//line or quadrangle
					{
						for (int j = 0; j < pscr->points.size() - 1; ++j)
							if (isNearToLine(pscr, j))
								addToIv(pscr, i);
					}
					else		// ordinary scribble
					{
						for (int j = 0; j < pscr->points.size(); ++j)
						{
							r = QRect(pscr->points[j], QSize(1, 1)).adjusted(-pw, -pw, pw, pw);
							if (r.intersects(rp))
							{
								addToIv(pscr, i);
								break;	// no need to check more points
							}
							else if (j < pscr->points.size() - 1)	// possibly consecutive points in a scribble were too far away from each other
							{										// if so then consider it a line and see if we are near to it
								if (SQUARE(pscr->points[j].x() - pscr->points[j + 1].x()) + SQUARE(pscr->points[j].y() - pscr->points[j + 1].y()) > SQUARE(pscr->penWidth))
								{
									if (isNearToLine(pscr, j))
									{
										addToIv(pscr, i);
										break;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	if (cnt)
	{
		if (!addToPrevious)
		{
			_ClearSelectLists();
			_selectionRect = result;
			_driSelectedDrawables = iv;
		}
		else
		{
			_selectionRect = result.united(_selectionRect);
			// merge lists
			for (auto ii : iv)
			{
				if (!_driSelectedDrawables.contains(ii))
					_driSelectedDrawables.push_back(ii);
			}
		}
		return _selectionRect;
	}

	return result;
}


/*========================================================
 * TASK:   copies selected drawables to a sprite or
 *			into '_copiedItems' and to the clipboard
 *
 * PARAMS:	sprite: possible null pointer to sprite
 *				that contains the copied drawables
 *				when null _copiedItems is used
 * GLOBALS: _parent (HistoryList)
 *			_driSelectedDrawables -only used if
 *					the sprite does not yet has its list
 *					filled in
 *			_selectionRect
 * RETURNS:	nothing
 * REMARKS: - if sprite is null copies items into
 *				'_copiedItems' else into the sprite
 *			- origin of drawables in '_copiedItems'
 *				will be (0,0)
 *			- '_copiedRect's' top left will also be (0,0)
 *-------------------------------------------------------*/
void History::CopySelected(Sprite* sprite)
{
	DrawableList& copiedItems = sprite ? sprite->drawables : _parent->_copiedItems;
	DrawableIndexVector* pSelectedDri;
	if (sprite)
	{
		if (sprite->driSelectedDrawables.size() == 0)
			sprite->driSelectedDrawables = _driSelectedDrawables;
		pSelectedDri = &sprite->driSelectedDrawables;
	}
	else
		pSelectedDri = &_driSelectedDrawables;

	if (!pSelectedDri->isEmpty())
	{
		copiedItems.Clear();

		for (auto ix : _driSelectedDrawables)  // absolute drawable item indices of visible items selected
		{
			DrawableItem* pdri = _drawables[ix];
			QPointF copyTopLeft = _selectionRect.topLeft();
			if (sprite)
				copyTopLeft -= QPointF(sprite->margin, sprite->margin);

			copiedItems.CopyDrawable(*pdri, copyTopLeft);	// shift coordinates to be relative to top left of selection
		}
		_parent->_copiedRect = _selectionRect.translated(-_selectionRect.topLeft());

		DrawableItemList *pdrl = copiedItems.Items();

		std::sort(pdrl->begin(), pdrl->end(), [](DrawableItem* pl, DrawableItem* pr) {return pl->zOrder < pr->zOrder; });
																			
		if (sprite)
		{
			sprite->rect = _parent->_copiedRect;	// (0,0, width, height)
			sprite->topLeft = _selectionRect.topLeft();
		}

		_parent->CopyToClipboard();
	}
}


/*========================================================
 * TASK:	if there were items pasted then copies their 
 *			indices to lists
 * PARAMS:	rect - set selectionRect to this
 * GLOBALS:	_driSelectedDrawables - output
 *			_driSelectedDrawablesRight/Left - cleared
 * RETURNS:
 * REMARKS: - after a paste the topmost item on stack
 *				'_items' is historyPasteItemsTop
 *-------------------------------------------------------*/
void History::CollectPasted(const QRectF& rect)
{
	int n = _items.size() - 1;
	HistoryItem* phi = _items[n];
	if (phi->type != HistEvent::heItemsPastedTop)
		return;

	_driSelectedDrawablesAtRight.clear();
	_driSelectedDrawablesAtLeft.clear();
	//      n=4
	//B 1 2 3 T
	//      m = 3       n - m,
	int m = ((HistoryPasteItemTop*)phi)->count;
	_driSelectedDrawables.resize(m);		// because it is empty when pasted w. rubberBand
	for (int j = 0; j < m; ++j)
		_driSelectedDrawables[j] = ((HistoryDrawableItem*)_items[n - m + j])->indexOfDrawable;
	_selectionRect = rect;

}

// ********************************** Sprite *************
Sprite::Sprite(History* ph, const QRectF& rectf, const DrawableIndexVector &dri) : pHist(ph), driSelectedDrawables(dri)
{
	ph->CopySelected(this);
	rect = rectf;	// set in CopySelected() so must overwrite it here
}

Sprite::Sprite(History* ph) : pHist(ph)
{
	ph->CopySelected(this);
}

// ********************************** HistoryList *************

constexpr const char* fbmimetype = "falconBoard-data/mwb";

/*=============================================================
 * TASK:	copies selected images and scribbles to clipboard
 * PARAMS:
 * GLOBALS: _copiedItem, _copiedImages, _copyGUID
 * RETURNS:
 * REMARKS:
 *------------------------------------------------------------*/
void HistoryList::CopyToClipboard()
{
	if (!_copiedItems.Size())
		return;

	// copy to system clipboard
	QByteArray clipData;
	QDataStream data(&clipData, QIODevice::WriteOnly);		  // uses internal QBuffer

	data << _copiedRect;
	data << _copiedItems.Items()->size();
	for (auto sr : *_copiedItems.Items())
		data << sr;

	QMimeData* mimeData = new QMimeData;
	_copyGUID = QUuid().createUuid();

	mimeData->setData(fbmimetype, clipData);

	QString s = "fBClipBoardDataV2.0"+ _copyGUID.toString();
	mimeData->setText(s);
	_pClipBoard->setMimeData(mimeData);
}

void HistoryList::PasteFromClipboard()
{
	const QMimeData* pMime = _pClipBoard->mimeData();
	
	QStringList formats = pMime->formats(); // get correct mime type 
	int formatIndex=-1;	// this is the index in'formats' 
						// for the real name for our format stored as it is not standard
	for (int i = 0; i < formats.size(); ++i)
		if (formats[i].indexOf(fbmimetype) >= 0)
		{
			formatIndex = i;
			break;
		}
	if (formatIndex < 0)	// not our data
		return;

	QString s = _pClipBoard->text();
	if (s.isEmpty() || s.left(19) != "fBClipBoardDataV2.0")
		return;

	if (_copyGUID != s.mid(19))	// new clipboard data
	{
		_copyGUID = s.mid(19);

		_copiedItems.Clear();
		QByteArray pclipData = pMime->data(formats[formatIndex]);
		QDataStream data(pclipData);		  // uses internal QBuffer

		DrawableItem dri;

		QString qs;
		int cnt;

		data >> _copiedRect;
		data >> cnt;
		while (cnt--)
		{
			data >> dri.dtType;	
			data >> dri;			// this must be called aftetr type was read
			_copiedItems.AddDrawable(&dri);
		}
	}
}
