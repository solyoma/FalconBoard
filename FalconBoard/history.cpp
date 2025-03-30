#include <QApplication>
#include <QMessageBox>
#include <QMainWindow>
#include <QTemporaryFile>

#include <algorithm>

#include "config.h"		  // version
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

//HistoryItem::HistoryItem(const HistoryItem& other) : pHist(other.pHist), type(other.type), pDrawables(other.pDrawables)
//{
//}


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

QPointF HistoryItem::_TopLeftFrom(const DrawableIndexVector& dvi) const
{
	QRectF a;
	for (auto& d : dvi)
	{
		a = a.united(pHist->Drawables()->Items()[d]->Area());
	}
	return a.topLeft();
}



//-------------------------------------------- 
// HistoryDrawableItem
//--------------------------------------------
// expects a complete subclass cast as DrawableItem and adds a copy of it
HistoryDrawableItem::HistoryDrawableItem(History* pHist, DrawableItem& dri) : HistoryItem(pHist, HistEvent::heDrawable)
{
	DrawableItem* _pDrawable=nullptr;
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
		default: break; // dtPen, etc
	}
	indexOfDrawable = pHist->AddToDrawables(_pDrawable);
}

HistoryDrawableItem::HistoryDrawableItem(History* pHist, DrawableItem* pdri) : HistoryItem(pHist, HistEvent::heDrawable)
{
	indexOfDrawable = pHist->AddToDrawables(pdri);
}

HistoryDrawableItem::HistoryDrawableItem(const HistoryDrawableItem& other) : HistoryItem(other)
{
	indexOfDrawable = other.indexOfDrawable;
}
HistoryDrawableItem::HistoryDrawableItem(HistoryDrawableItem&& other) noexcept : HistoryItem(other)
{
	indexOfDrawable = other.indexOfDrawable;
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

void HistoryDrawableItem::Rotate(MyRotation rot, QPointF center)
{
	pDrawables->RotateDrawable(indexOfDrawable,rot, center);
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
	if (deletedList.isEmpty())	// then delete all items
	{
		for (int i = 0; i < pDrawables->Size(); ++i)
			deletedList.push_back(i);
	}

	for (auto drix : deletedList)
			pDrawables->SetVisibility(drix, false);
	return 0;
}

HistoryDeleteItems::HistoryDeleteItems(History* pHist, DrawableIndexVector& selected) : HistoryItem(pHist, HistEvent::heItemsDeleted), deletedList(selected)
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
	if (lstItemsRight.isEmpty())	 // vertical movement
	{
		pDrawables->VertShiftItemsBelow(rr.bottom(), -rr.height());	 //  < 0 move up
	}
	else	// horizontal movement
	{
		QPointF dr(-rr.width(), 0);									// move left
		for (auto index : lstItemsRight)
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
	if (lstItemsRight.isEmpty())	 // vertical movement
	{
		pDrawables->VertShiftItemsBelow(rr.top(), rr.height());	 // move down
	}
	else	// horizontal movement
	{
		QPointF dr(rr.width(), 0);						 // delta >0 -> move right
		for (auto index : lstItemsRight)
			(*pHist)[index]->Translate(dr, -1);
	}
	return 1;
}

HistoryRemoveSpaceItem::HistoryRemoveSpaceItem(History* pHist, DrawableIndexVector& toModify, QRectF rr) :
	HistoryItem(pHist, HistEvent::heSpaceDeleted), lstItemsRight(toModify), rr(rr)
{
	Redo();
}

HistoryRemoveSpaceItem::HistoryRemoveSpaceItem(const HistoryRemoveSpaceItem& other) : HistoryItem(other)
{
	*this = other;
}

HistoryRemoveSpaceItem& HistoryRemoveSpaceItem::operator=(const HistoryRemoveSpaceItem& other)
{
	lstItemsRight = other.lstItemsRight;
	rr = other.rr;
	return *this;
}
HistoryRemoveSpaceItem::HistoryRemoveSpaceItem(HistoryRemoveSpaceItem&& other)  noexcept : HistoryItem(other)
{
	*this = other;
}

HistoryRemoveSpaceItem& HistoryRemoveSpaceItem::operator=(HistoryRemoveSpaceItem&& other)  noexcept
{
	lstItemsRight = other.lstItemsRight;
	rr = other.rr;
	return *this;
}

//--------------------------------------------

HistoryPasteItemBottom::HistoryPasteItemBottom(History* pHist, int index, int count) :
	HistoryItem(pHist, HistEvent::heItemsPastedBottom), index(index), count(count)
{
}

HistoryPasteItemBottom::HistoryPasteItemBottom(HistoryPasteItemBottom& other) :
	HistoryItem(other), index(other.index), count(other.count)
{
}

HistoryPasteItemBottom& HistoryPasteItemBottom::operator=(const HistoryPasteItemBottom& other)
{
	index = other.index;
	count = other.count;
	return *this;
}

//--------------------------------------------
HistoryPasteItemTop::HistoryPasteItemTop(History* pHist, int index, int count, QRectF& rect) :
	HistoryItem(pHist, HistEvent::heItemsPastedTop), indexOfBottomItem(index), count(count), boundingRect(rect)
{
}

HistoryPasteItemTop::HistoryPasteItemTop(const HistoryPasteItemTop& other) : HistoryItem(other)
{
	*this = other;
}
HistoryPasteItemTop& HistoryPasteItemTop::operator=(const HistoryPasteItemTop& other)
{
	indexOfBottomItem = other.indexOfBottomItem;
	count = other.count;
	boundingRect = other.boundingRect;
	return *this;
}
HistoryPasteItemTop::HistoryPasteItemTop(HistoryPasteItemTop&& other)  noexcept : HistoryItem(other)
{
	*this = other;
}
HistoryPasteItemTop& HistoryPasteItemTop::operator=(HistoryPasteItemTop&& other) noexcept
{
	indexOfBottomItem = other.indexOfBottomItem;
	count = other.count;
	boundingRect = other.boundingRect;
	return *this;
}

int  HistoryPasteItemTop::Undo() // elements are in _items and will be moved to _redoList after this
{
	if (moved)	// then the first item above the bottom item is a history delete item
	{
		HistoryDeleteItems* pHDelItems = reinterpret_cast<HistoryDeleteItems*>( (*pHist)[indexOfBottomItem + 1] );
		if (!pHDelItems->deletedList.isEmpty())	// then set original selections back
			pHist->CollectDeleted(pHDelItems);
		pHDelItems->Undo();
	}
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

void HistoryPasteItemTop::Rotate(MyRotation rot, QPointF center)
{
	for (int i = 1; i <= count; ++i)
		(*pHist)[indexOfBottomItem + i]->Rotate(rot, center);
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
	HistoryItem(pHist, HistEvent::heRecolor), selectedList(listOfSelected), pk(pk)
{
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

HistoryReColorItem::HistoryReColorItem(HistoryReColorItem& other) : HistoryItem(other)
{
	*this = other;
}

HistoryReColorItem& HistoryReColorItem::operator=(const HistoryReColorItem& other)
{
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

HistoryMoveItems::HistoryMoveItems(History* pHist, QPointF displacement, DrawableIndexVector& selection) :
	HistoryItem(pHist, HistEvent::heMoveItems), dr(displacement), movedItemList(selection)
{
	Redo();
}

HistoryMoveItems::HistoryMoveItems(const HistoryMoveItems& other) : HistoryItem(other)
{
	*this = other;
}
HistoryMoveItems& HistoryMoveItems::operator=(const HistoryMoveItems& other)
{
	pHist = other.pHist;
	dr = other.dr;
	movedItemList = other.movedItemList;
	return *this;
}

int  HistoryMoveItems::Undo()
{
	pHist->MoveItems(-dr, movedItemList);
	return 1;
}

int  HistoryMoveItems::Redo()
{
	pHist->MoveItems(dr, movedItemList);
	return 0;
}
QRectF HistoryMoveItems::Area() const
{
	QRectF rect;
	for (auto& a : movedItemList)
		rect = rect.united(pHist->Drawable(a)->Area());
	return rect;
}

//---------------------------------------------------

HistoryInsertVertSpace::HistoryInsertVertSpace(History* pHist, int top, int pixelChange) :
	HistoryItem(pHist, HistEvent::heVertSpace), y(top), heightInPixels(pixelChange)
{
	Redo();
}

HistoryInsertVertSpace::HistoryInsertVertSpace(const HistoryInsertVertSpace& other) : HistoryItem(other)
{
	*this = other;
}
HistoryInsertVertSpace& HistoryInsertVertSpace::operator=(const HistoryInsertVertSpace& other)
{
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

HistoryRotationItem::HistoryRotationItem(History* pHist, MyRotation rotation, QRectF rect, DrawableIndexVector selList) :
	HistoryItem(pHist, HistEvent::heRotation), rot(rotation), driSelectedDrawables(selList), encRect(rect), center(rect.center())
{
	Redo();
}

HistoryRotationItem::HistoryRotationItem(const HistoryRotationItem& other) :
	HistoryItem(other.pHist), rot(other.rot), driSelectedDrawables(other.driSelectedDrawables), encRect(other.encRect),center(other.center)
{
	
}

HistoryRotationItem& HistoryRotationItem::operator=(const HistoryRotationItem& other)
{
	pHist = other.pHist;
	rot = other.rot;
	driSelectedDrawables = other.driSelectedDrawables;
	encRect = other.encRect;
	center = other.center;

	return *this;
}

// helper
//static void SwapWH(QRectF& r)
//{
//	int w = r.width();
//	r.setWidth(r.height());
//	r.setHeight(w);
//}


int HistoryRotationItem::Undo()
{
	MyRotation rotation = rot;
	rotation.InvertAngle();
	QRectF area;

	for (auto dri : driSelectedDrawables)
	{
		pHist->Rotate(dri, rotation, center);
		area = area.united(pHist->Drawable(dri)->Area());
	}
	encRect = area;

	return 1;
}

int HistoryRotationItem::Redo()
{
	if (!pHist->RotateSelected(rot))	// '_driSelectedDrawables', uses then recalculates pHist->_selectionRect
	{
		encRect = QRectF();
		return 0;
	}
	encRect = pHist->SelectionRect();
	return 0;
}


//****************** HistorySetTransparencyForAllScreenshotsItem ****************
HistorySetTransparencyForAllScreenshotsItems::HistorySetTransparencyForAllScreenshotsItems(History* pHist, QColor transparentColor, qreal fuzzyness) : 
	fuzzyness(fuzzyness), transparentColor(transparentColor), HistoryItem(pHist)
{
	Redo();
}

int HistorySetTransparencyForAllScreenshotsItems::Redo()
{
	DrawableList* pdrbl = &pHist->_drawables;
	undoBase = pdrbl->Size(DrawableType::dtScreenShot); // for undo this will be the first position
	DrawableItem* psi;
	DrawableScreenShot* pds;
	//DrawableItemIndex int drix;
	//int imgIndex = 0;
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
			pds->SetImage(image);
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

HistoryEraserStrokeItem::HistoryEraserStrokeItem(History* pHist, DrawableItem& dri) : eraserPenWidth(dri.penWidth), HistoryItem(pHist, HistEvent::heEraserStroke)
{
	eraserStroke.clear(); // ??

	DrawableCross* pdrC;
	DrawableEllipse* pdrE;
	DrawableRectangle* pdrR;

	QPainterPath myPath;
	switch (dri.dtType)
	{
		case DrawableType::dtCross:
			{
				QPointF dist;
				pdrC = (DrawableCross*)&dri;
				dist = { pdrC->length / sqrt(2), pdrC->length / sqrt(2) };
				QRectF rect(pdrC->refPoint - dist, pdrC->refPoint + dist);
				eraserStroke.append(rect.topLeft());
				eraserStroke.append(rect.bottomRight());
				eraserStroke.append(pdrC->refPoint);
				eraserStroke.append(rect.bottomLeft());
				eraserStroke.append(rect.topRight());
			}
			break;
		case DrawableType::dtDot:
			eraserStroke.append(dri.refPoint);
			break;
		case DrawableType::dtEllipse:
			pdrE = (DrawableEllipse*)&dri;
			myPath.addEllipse(pdrE->rect);
			eraserStroke = myPath.toFillPolygon();
			break;
		case DrawableType::dtLine:
			eraserStroke.append(((DrawableLine&)dri).refPoint);
			eraserStroke.append(((DrawableLine&)dri).endPoint);
			break;
		case DrawableType::dtRectangle:
			pdrR = (DrawableRectangle*)&dri;
			myPath.addRect(pdrR->rect);
			eraserStroke = myPath.toFillPolygon();
			break;
		case DrawableType::dtScribble:
			eraserStroke = ((DrawableScribble&)dri).points;
			break;
		default:	// no dtPen,dtScreenShot, etc
			break;
	}
	pHist->GetDrawablesInside(dri.Area(), affectedIndexList);
	// not all drawables inside dri's area are really affected by 
	// the eraser strokes (or rectangle, or ellipse
	if (affectedIndexList.isEmpty())				// will not be used or saved in callers!
		return;
	Redo();
}

HistoryEraserStrokeItem::HistoryEraserStrokeItem(const HistoryEraserStrokeItem& o)  
	:  HistoryItem(o), eraserPenWidth(o.eraserPenWidth), eraserStroke(o.eraserStroke), affectedIndexList(o.affectedIndexList),subStrokesForAffected(o.subStrokesForAffected)
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
	affectedIndexList = pHist->SelectedDrawables();
	if (affectedIndexList.isEmpty())				// will not be used or saved in callers!
		return;
	Redo();
}

HistoryPenWidthChangeItem::HistoryPenWidthChangeItem(const HistoryPenWidthChangeItem& o) :
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

//****************** HistoryPenColorChangeItem ****************
HistoryPenColorChangeItem::HistoryPenColorChangeItem(History* pHist, const DrawColors& origc, const DrawColors& newc)  :
	original(origc), redefined(newc), HistoryItem(pHist, HistEvent::hePenColorChanged)
{
	Redo();
}

HistoryPenColorChangeItem::HistoryPenColorChangeItem(const HistoryPenColorChangeItem& o):
	original(o.original), redefined(o.redefined), HistoryItem(o)
{
	Redo();
}

HistoryPenColorChangeItem& HistoryPenColorChangeItem::operator=(const HistoryPenColorChangeItem& o)
{
	(HistoryItem&)(*this) = (HistoryItem&)o;
	original = o.original;
	redefined = o.redefined;
	return *this;
}

int HistoryPenColorChangeItem::Undo()
{
	pHist->drawColors = globalDrawColors = original;
	return 1;
}

int HistoryPenColorChangeItem::Redo()
{
	pHist->drawColors = globalDrawColors = redefined;
	return 1;
}


//****************** HistoryRubberBandItem ****************
HistoryRubberBandItem::HistoryRubberBandItem(History* pHist, QRectF r) : rect(r), HistoryItem(pHist, HistEvent::heRubberBand)
{
}

HistoryRubberBandItem::HistoryRubberBandItem(const HistoryRubberBandItem& o) : rect(o.rect), HistoryItem(o)
{
}

int HistoryRubberBandItem::Undo()
{
	return 0;
}

int HistoryRubberBandItem::Redo()
{
	return 0;
}

//****************** HistoryZoomItem ****************
HistoryZoomItem::HistoryZoomItem(History* pHist, QRectF r, bool zoomIn, int steps) : zoomIn(zoomIn), zoomedRect(r), steps(steps),HistoryItem(pHist, HistEvent::heZoom)
{
	zoomedItemsList = pHist->SelectedDrawables();
	_SetZoomedRectangle();
}

HistoryZoomItem::HistoryZoomItem(const HistoryZoomItem& o) : HistoryItem(o)
{
	zoomIn = o.zoomIn;
	zoomedRect = o.zoomedRect;
	zoomedItemsList = o.zoomedItemsList;
}

HistoryZoomItem& HistoryZoomItem::operator=(const HistoryZoomItem& o)
{
	zoomIn = o.zoomIn;
	zoomedRect = o.zoomedRect;
	zoomedItemsList = o.zoomedItemsList;
	return *this;
}

int HistoryZoomItem::Undo()
{
	for (auto& ix : zoomedItemsList)
	{
		DrawableItem* pdrwi = pHist->Drawable(ix);
		pdrwi->Zoom(!zoomIn, zoomedRect.center(), steps);
	}
	return 0;
}

int HistoryZoomItem::Redo()
{

	for (auto& ix : zoomedItemsList)
	{
		DrawableItem* pdrwi = pHist->Drawable(ix);
		pdrwi->Zoom(zoomIn, zoomedRect.center(), steps);
	}
	return 0;
}

void HistoryZoomItem::_SetZoomedRectangle()
{
	DrawableRectangle* pdr = new DrawableRectangle(zoomedRect, -1, penBlackOrWhite, 1, false);
	pdr->Zoom(zoomIn, zoomedRect.center(), steps);
	zoomedRect = pdr->Area();
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
	drawColors = o.drawColors;	// visible
	_parent = o._parent;
	_items = o._items;
	_redoList = o._redoList;
	_drawables = o._drawables;
	_quadTreeDelegate = o._quadTreeDelegate;

	_zorderStore = o._zorderStore;
}

History::History(History&& o) noexcept
{
	drawColors = o.drawColors;	// visible

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

QString History::SnapshotName(bool withPath)
{
	if (_snapshotName.isEmpty())
	{
		QTemporaryFile f;
		if (f.open())
		{
			_snapshotName = f.fileName();
			_snapshotName = _snapshotName.mid(_snapshotName.indexOf('.') + 1);
		}
	}
	if (withPath)
		return FBSettings::homePath + _snapshotName;
	else
		return _snapshotName;
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
	for (auto item : _drawables.Items())
	{
		if (item->IsVisible())
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

void History::_CantRotateWarning() const
{
	QMessageBox::warning(nullptr, QObject::tr("FalconG - Warning"), QObject::tr("Can't rotate, as part of rotated area would be outside 'paper'"));
}

bool History::CanRotateSelected(MyRotation rot)
{
	if (rot.IsNull())	// no rotation or reflection
		return true;

	QPointF center = _selectionRect.center();
	for (auto dri : _driSelectedDrawables)
		if (!_drawables[dri]->CanRotate(rot, center))
			return false;
	return true;
}

bool History::RotateSelected(MyRotation rot)	 // if it can and it does then
{																 // calculate new selection area
	if (rot.IsNull())	// no rotation or reflection
		return true;
	if(!CanRotateSelected(rot))
	{
		_CantRotateWarning();
		return false;
	}

	QPointF center = _selectionRect.center();
	_selectionRect = QRectF();
	for (auto dri : _driSelectedDrawables)
	{
		DrawableItem* pdri = _drawables[dri];
		pdri->Rotate(rot, center);
		_selectionRect = _selectionRect.united(pdri->Area());
	}
	return true;
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
		DrawableItem* pdrwi = Drawable(ix);
		if (x < pdrwi->Area().right())
			x = pdrwi->Area().right();
	}
	return x;
}


QPointF History::BottomRightLimit(QSize screenSize)
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
	//	  // no undo after new item added
	if (p->type == HistEvent::heDrawable)
	{
		_drawables.ClearRedo();			// any  Undo()-t drawbles must be removed
		_redoList.clear();
	}
	//for(auto a: _redoList)
	//	if (a->type == HistEvent::heDrawable)
	//	{
	//		_drawables.ClearRedo();			// any  Undo()-t drawbles must be removed
	//		break;
	//	}
	//_redoList.clear();	

	p = _items.last();

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
	_readCount = _savedItemCount = 0;
	_loadedName.clear();
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
 * TASK:    save actual visible drawable items in either a
 *			normal file or as a snapshot
 * PARAMS:  'asSnapshot' - true: save as a snapshot, even when
 *								 _fileName is set
 *						   false: if _fileName is empty save
 *								 snapshot, otherwise save using
 *								 _fileName
 * GLOBALS:
 * RETURNS:	result of save
 * REMARKS: - when saved as snapshot '_lastSavedAsSnapshot is set
 *			  to true
 *			- if 'asSnapshot' is false removes snapshot after save
 *			  and set '_lastSavedAsSnapshot' to false
 *			- do not save file if it was not modified and
 *			  either it was not saved as snapshot or 
 *			  it was saved as snapshot and it doesn't have
 *			  a _fileName
 *-------------------------------------------------------*/
SaveResult History::Save(bool asSnapshot)
{
	// only save visible items
	// snapshot file is in FalconBoard folder
	QString snapshotName = SnapshotName(true);	

	if (_fileName != _loadedName)
		_loadedName = _fileName;

	if (_drawables.Count() == 0 && _savedItemCount == 0)					// no elements or no visible elements
	{
		// QMessageBox::information(nullptr, sWindowTitle, QObject::tr("Nothing to save"));
		return srNoNeedToSave;
	}
	if(!IsModified() && (!_lastSavedAsSnapshot || _fileName.isEmpty() ) )
		return srNoNeedToSave;

	if (_fileName.isEmpty())		
		asSnapshot = true;

	QString name = asSnapshot ? snapshotName : _fileName;

	if (asSnapshot)		// then save an index file beside the main file with the file name in it
	{
		QFile f(name);
		f.open(QIODevice::WriteOnly);
		if (!f.isOpen())
			return srFailed;   // can't write file
		QDataStream ofsdat(&f);
		ofsdat << _fileName;
		name = name + ".dat";	 // the mwb file
	}
					  // *********  start saving ******** 
	QFile f(name + ".tmp");
	f.open(QIODevice::WriteOnly);

	if (!f.isOpen())
		return srFailed;   // can't write file
	QDataStream ofs(&f);

	ofs << MAGIC_ID;
	ofs << MAGIC_VERSION;	// file version

	ofs << (uint16_t)gridOptions;
	ofs << _resolutionIndex << _pageWidthInPixels << _useResInd;
	for (int i = (int)penT2; i < PEN_COUNT; ++i)
		drawColors.SavePen(ofs, (FalconPenKind)i);

					  // save drawables
#if 1
	DrawableItemList drbl = _drawables.Items();		// get a copy of the actual state of the list, 
													// so if it changes during saving it doesn't matter
	QMap<int, DrawableItem*> drblMap;				// only visible, ordered by the unique zOrder
													// zOrder is not saved explicitely
	for (auto& pdrwi : drbl)
	{
		if (pdrwi->IsVisible())
			drblMap.insert(pdrwi->zOrder, pdrwi);
	}

	for (auto& pdrwi : drbl)
	{
		if (pdrwi->IsVisible())
			ofs << *pdrwi;
		if (ofs.status() != QDataStream::Ok)
		{
			f.remove();
			return srFailed;
		}
	}
#else
	QRectF area = _drawables.Area();
	DrawableItem* phi = _drawables.FirstVisibleDrawable(area); // lowest in zOrder
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
#endif
					  // *********  saving finished ******** 
	f.close();
	if (QFile::exists(name + "~"))
		QFile::remove(QString(name + "~"));

	if(asSnapshot)	// then don't use a backup file and delete previous snapshot
		QFile::remove(QString(name));
	else			// create backup file from original
		QFile::rename(name, QString(name + "~"));

	f.rename(name);	

	_savedItemCount = _items.count();	// saved at this point

	if (asSnapshot)
	{
		_lastSavedAsSnapshot = true;
	}
	else // remove snapshot files
	{
		_lastSavedAsSnapshot = false;
		QFile::remove(snapshotName);
		QFile::remove(snapshotName+".dat");
	}	
	_loaded = true;	// if saved, then already is in memory as if it was loaded

	return srSaveSuccess;
}


void History::SetName(QString name, bool clear)
{
	bool isSnapshotName = name.indexOf('/') < 0;
	if(!isSnapshotName)
		_fileName = name;

	if (isSnapshotName)
	{
		_snapshotName = name;
		_lastSavedAsSnapshot = true;
		QString s = FBSettings::homePath + name;
		_NameFromTmpData(s);
	}

	if(clear)
		Clear();
}

void History::_NameFromTmpData(QString &nameOfSnapshot)
{
	_fileName.clear();
	if (!QFile::exists(nameOfSnapshot))	// File not found	
		return;
	// then get file name from there
	QFile f(nameOfSnapshot);
	f.open(QIODevice::ReadOnly);
	if (!f.isOpen())
		return;			
	QDataStream ifs(&f);
	ifs >> _fileName;
}

/*========================================================
 * TASK:	Loads saved file whose full path name is set 
 *			into _fileName or a snapshot file when 
 *			_lastSavedAsSnapshot is true
 * PARAMS:	'version_loaded' set version of loaded file into this
 *			'force' load it even when already loaded
 *				in which case it overwrites data in memory
 *			'fromY' - add this to the y coord of every point loaded
 *						(used for append)
 * GLOBALS: _fileName, _snapshotName, 
 *			_lastSavedAsSnapshot - set this only for snapshot files
 *			_inLoad,_readCount,_items,
 *			_screenShotImageList,_modified, 
 * RETURNS:   -1: file does not exist
 *			< -1: file read error number of records read so
 *				  far  is -(ret. value+2)
 *			   0: invalid file
 *			>  0: count of items read
 * REMARKS: - beware of old version files
 *			- when 'force' clears data first
 *			- _fileName and _loadedName are full path names, 
 *			  _snapshotName is not
 *			- both _fileName and _loadedName can be empty
 *			  in that case _snapshotName is used
 *-------------------------------------------------------*/
int History::Load(quint32& version_loaded, bool force, int fromY)
{
	(void)SnapshotName(false);	// generate new name
	QString fname = _lastSavedAsSnapshot ? SnapshotName(true)+".dat" : _fileName;

	if (fname.isEmpty())
		return 1;			//  no record loaded, but this is not an error

	if ( (!force && _fileName == _loadedName && !_lastSavedAsSnapshot) || _loaded)	// already loaded?
		return _items.count();				// can't use _readCount

	DrawableItem::yOffset = 0.0;

	QFile f(fname);
	f.open(QIODevice::ReadOnly);
	if (!f.isOpen())
		return -1;			// File not found

	QDataStream ifs(&f);
	qint32 id;
	ifs >> id;
	if (id != MAGIC_ID)
		return 0;			// read error
	ifs >> version_loaded;		// like 0x56020101 for V 2.1.1
	if ((version_loaded >> 24) != 'V')	// invalid/damaged  file or old version format
		return 0;

	Clear();
	
	int res=0;
	try
	{
		if ((version_loaded & 0x00FF0000) < 0x020000)
			res = _LoadV1(ifs, version_loaded);
		else
			res = _LoadV2(ifs, version_loaded);
	}
	catch (...)
	{
		res= 0;		// invalid file
	}
	f.close();
	_loaded = true;
	return res;
}

int History::Append(QString fileName, quint32& version_loaded)
{
	if (fileName.isEmpty())
		return 1;			//  no record loaded, but this is not an error

	if (fileName == _loadedName)	// don't append itself to itself
		return _readCount;
	QFile f(fileName);
	f.open(QIODevice::ReadOnly);
	if (!f.isOpen())
		return -1;			// File not found

	QDataStream ifs(&f);
	qint32 id;
	ifs >> id;
	if (id != MAGIC_ID)
		return 0;			// read error
	ifs >> version_loaded;		// like 0x56020101 for V 2.1.1
	if ((version_loaded >> 24) != 'V')	// invalid/damaged  file or old version format
		return 0;

	DrawableItem di;
	DrawableItem::yOffset = BottomRightLimit(QSize()).y();
	int nPosBottom = _items.size();
	HistoryPasteItemBottom* pb = new HistoryPasteItemBottom(this, nPosBottom, -1);
	_AddItem(pb);			   // added at index 'nPosBottom'
				// read items
	int nReadLines;
	if ((version_loaded & 0x00FF0000) < 0x020000)
		nReadLines = _ReadV1(ifs, di, version_loaded);
	else
	{
		uint16_t u = 0;
		int n;
		bool b;

		ifs >> u;	// drop grid options
		if (version_loaded > 0x56020200)	// and
			ifs >> n >> n >> b;				// resolution Index, page Width In Pixels, use Resolution Index flag
		nReadLines = _ReadV2(ifs, di);
	}

	if (nReadLines < 0)	// error in appending file
	{
		if (nReadLines < -2)				// read some lines
			nReadLines = -nReadLines + 2;
		else  // no lines read
		{
			_items.pop_back();	// drop history item for append
			return 0;
		}
	}
		// set correct count
	pb = reinterpret_cast<HistoryPasteItemBottom*>(_items[nPosBottom]);
	pb->count = nReadLines;
	QuadArea qarea = _quadTreeDelegate.Area();
	QRectF rect = { 0, DrawableItem::yOffset, qarea.width(), qarea.bottom() - DrawableItem::yOffset };
	HistoryPasteItemTop* pt = new HistoryPasteItemTop(this, nPosBottom, nReadLines, rect);
	_AddItem(pt);
	f.close();

	return nReadLines;
}

int History::_ReadV1(QDataStream& ifs, DrawableItem& di, qint32 version)	// returns OK: count of items read
{																			//        ERR: -(count read so far +2)
	DrawableItem *pdrwh;

	DrawableDot dDot;
	DrawableLine dLin;
	DrawableRectangle dRct;
	DrawableScribble dScrb;
	DrawableScreenShot dsImg;

// z order counters are reset
	int n, nRead = _items.size();
	while (!ifs.atEnd())
	{
		ifs >> n;	// HistEvent				
		if ((HistEventV1(n) == HistEventV1::heScreenShot))
		{
			int x, y;
			ifs >> dsImg.zOrder >> x >> y;
			dsImg.refPoint = QPoint(x, y);

			QPixmap pxm;
			ifs >> pxm;
			dsImg. SetImage(pxm);
			pdrwh = &dsImg;
		}
		else
		{
			// only screenshots and scribble are possible in a ver 1.0 file
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
			// bool filled = n & 128;
			ifs >> n; dScrb.penWidth = n;

			qint32 x, y;
			dScrb.Reset();

			ifs >> n;
			while (n--)
			{
				ifs >> x >> y;
				dScrb.Add(x, y);
			}
			dScrb.refPoint = dScrb.points[0];

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
					pdrwh->SetPenKind(penT5);
				else if (pdrwh->PenKind()== penT5)
					pdrwh->SetPenKind( penEraser);
			}
			// end patch
			if (ifs.status() != QDataStream::Ok)
			{
				_inLoad = false;
				return -((_items.size() - nRead) + 2);
			}
		}
		(void)AddDrawableItem(*pdrwh);	// this will add the drawable to the list and sets its zOrder too
	}
	return   _items.size() - nRead;
}
int History::_ReadV2(QDataStream& ifs, DrawableItem& di)
{
// z order counters are reset
	DrawableItem * pdrwh=nullptr;
	DrawableDot dDot;
	DrawableCross dCross;
	DrawableEllipse dEll;
	DrawableLine dLin;
	DrawableRectangle dRct;
	DrawableScribble dScrb;
	DrawableText dTxt;
	DrawableScreenShot dsImg;

	// set default colors
	drawColors.Initialize();
	int nRead = _items.count();
	while (!ifs.atEnd())
	{
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
			case DrawableType::dtPen:			(void)drawColors.ReadPen(ifs);
												break;
			default: break;
		}
		globalDrawColors = drawColors;
		if(pdrwh && (int)di.dtType < (int)DrawableType::dtNonDrawableStart)	// only add drawables
			(void)AddDrawableItem(*pdrwh);		// this will set its zOrder too
		di.erasers.clear();
	}

	return _items.count()-nRead;
}

int History::_LoadV1(QDataStream &ifs, qint32 version)
{
	_inLoad = true;

	DrawableItem di;
	di.yOffset = 0;		

	_savedItemCount = 0;		

	int res = _ReadV1(ifs, di, version);

	_loadedName = _fileName;
	return 	_readCount = _savedItemCount += res;
}

int History::_LoadV2(QDataStream&ifs, qint32 version_loaded)
{
	uint16_t u = 0;

	ifs >> u;
	gridOptions = u;

	if (version_loaded > 0x56020200)
		ifs >> _resolutionIndex >> _pageWidthInPixels >> _useResInd;

	DrawableItem di;
	di.yOffset = 0;		

	_savedItemCount = 0;		
		// file offset is now 19 (0x13)
	int res = _ReadV2(ifs, di);

	_loadedName = _fileName;

	return 	_readCount = _savedItemCount += res;
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
				if (pdrwi->refPoint == itm.refPoint)
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
HistoryItem* History::AddCopiedItems(QPointF topLeft, Sprite* pSprite)			   // tricky
{
	DrawableList* pCopiedItems = pSprite ? &pSprite->drawables : &_parent->_copiedItems;
	if (!pCopiedItems->Size())	// any drawable
		return nullptr;          // do not add an empty list

	QRectF* pCopiedRect = pSprite ? &pSprite->rect : &_parent->_copiedRect;
  // ------------add bottom item

	HistoryDeleteItems* phd = nullptr;
	bool mustUseEnvelop = pCopiedItems->Size() > 1 || (pSprite && pSprite->itemsDeleted);
	if (pSprite && pSprite->itemsDeleted)	// then on top of _items there is a HistoryDeleteItems
	{										// which has to be moved above the past bottom item
		phd = (HistoryDeleteItems*)_items[_items.size() - 1];	// pointer to HistoryDeleteItems
		_items.pop_back();
	}
	// -------------- add the bottom item but only for more than one item, or one with deleted items --------
	int indexOfBottomItem = _items.size();
	if (mustUseEnvelop)
	{
		HistoryPasteItemBottom* pb = new HistoryPasteItemBottom(this, indexOfBottomItem, pCopiedItems->Size());
		_AddItem(pb);
		if (pSprite)
		{
			pb->moved = pSprite->itemsDeleted ? 1 : 0;
			_AddItem(phd);
		}
	}
	HistoryDrawableItem* p = nullptr;
	//----------- add drawable(s)
	_selectionRect = QRect();

	for (auto &si : pCopiedItems->Items() )
	{
		si->Translate(topLeft, -1);		// transforms original item
		si->zOrder = -1;	// so it will be on top
		p = new HistoryDrawableItem(this, *si);	   // just a copy, si's data remains where it was
		_AddItem(p);
		_selectionRect  = _selectionRect.united(si->Area());
		si->Translate(-topLeft, -1);	// transform back original item
	}
	// ------------Add Paste top marker item
	if (mustUseEnvelop)
	{
		QRectF rect = pCopiedRect->translated(topLeft);
		HistoryPasteItemTop* pt = new HistoryPasteItemTop(this, indexOfBottomItem, pCopiedItems->Size(), rect);
		if (pSprite)
			pt->moved = pSprite->itemsDeleted ? 1 : 0;		// mark it moved
		return _AddItem(pt);
	}
	return p;	// last added item
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

HistoryItem* History::AddMoveItems(QPointF displacement)
{
	HistoryMoveItems* phmv = new HistoryMoveItems(this, displacement, _driSelectedDrawables);
	return _AddItem(phmv);
}

HistoryItem* History::AddRotationItem(MyRotation rot)
{									   // only called when rotation was possible, no need to check here
	if (!_driSelectedDrawables.size() )
		return nullptr;          // do not add an empty list or a non rotatable region
												// this performs the rotation	
	HistoryRotationItem* phss = new HistoryRotationItem(this, rot, _selectionRect, _driSelectedDrawables);
	if (phss->encRect.isNull())
	{
		delete phss;
		return nullptr;
	}
	_selectionRect = phss->encRect;
	return _AddItem(phss);
}

HistoryItem* History::AddRemoveSpaceItem(QRectF& rect)
{
	int  nCntRight = _driSelectedDrawablesAtRight.size(),	// both scribbles and screenshots
		 nCntLeft = _driSelectedDrawablesAtLeft.size();

	if ((!nCntRight && nCntLeft))	// can't remove space from just the left
		return nullptr;

	HistoryRemoveSpaceItem* phrs = new HistoryRemoveSpaceItem(this, _driSelectedDrawablesAtRight, rect);
	return _AddItem(phrs);
}

HistoryItem* History::AddScreenShotTransparencyToLoadedItems(QColor trColor, qreal fuzzyness)
{
	HistorySetTransparencyForAllScreenshotsItems* psta = new HistorySetTransparencyForAllScreenshotsItems(this, trColor, fuzzyness);
	return _AddItem(psta);
}

HistoryItem* History::AddRubberBand(QRectF rect)
{
	HistoryRubberBandItem* phrb = new HistoryRubberBandItem(this, rect);
	return _AddItem(phrb);
}

HistoryItem* History::AddZoomItem(QRectF rect, bool zoomIn, int steps)
{
	HistoryZoomItem* phzi = new HistoryZoomItem(this, rect, zoomIn, steps);
	return _AddItem(phzi);
}

void History::ReplaceLastItemWith(DrawableItem& di)
{
	if(_items.size())
	{
		Undo();
		HistoryDrawableItem* phdi = new HistoryDrawableItem(this, di);
		_AddItem(phdi);
	}
}

HistoryItem* History::AddPenWidthChange(int increment)
{
	HistoryPenWidthChangeItem* ppwch = new HistoryPenWidthChangeItem(this, increment);
	if (ppwch && !_selectionRect.isNull())
	{
		qreal d = increment / 2.0;
		_selectionRect.adjust(-d, -d, d, d);
	}
	return _AddItem(ppwch);
}

HistoryItem* History::AddPenColorChange(const DrawColors& newdc)
{
	HistoryPenColorChangeItem* ppcwc = new HistoryPenColorChangeItem(this, globalDrawColors, newdc);
	return _AddItem(ppcwc);
}

//********************************************************************* History ***********************************

bool History::MoveItems(QPointF displacement, const DrawableIndexVector& driv)
{
	if (driv.isEmpty())
		return false;
	return _drawables.MoveItems(displacement, driv);
}

void History::Rotate(HistoryItem* forItem, MyRotation withRotation)
{
	if (forItem)
		forItem->Rotate(withRotation, _selectionRect.center());
}

void History::Rotate(int index, MyRotation rot, QPointF center)
{
	_drawables[index]->Rotate(rot, center);
}

void History::InserVertSpace(int y, int heightInPixels)
{
	_drawables.VertShiftItemsBelow(y, heightInPixels);
//	_modified = true;
}

HistoryItem* History::Undo()      // returns item on top of _items or null
{
	int actItem = _items.size();
	if (!actItem || actItem == _readCount)		// no more undo
		return nullptr;

	// ------------- first Undo top item
	HistoryItem* phi = _items[--actItem];
	int count = phi->Undo()-1;		// it will affect (count+1) elements (copy / paste / append)

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
//	_modified = true;

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

//	_modified = true;

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
 * TASK:   collects indices for drawable items (in '_drawables')
 *			that are completely inside a rectangular area
 *				into '_driSelectedDrawables',
 *			that are at the right&left of 'rect' into
 *				'_driSelectedDrawablesAtRight' and '_driSelectedDrawablesAtLeft'
 *
 * PARAMS:	rect: document (0,0) relative rectangle
 *			doNotShrinkSelectionRectangle : 
 * GLOBALS: _drawables,	_selectionRect
 * RETURNS:	size of selected item list + lists filled +
 *			_selectionRect shrinked
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

	// first select all items inside a horizontal band whose top and bottom are set from 'rect'
	// but it occupies the whole width of the 'paper' into "horizontal band" hb
	QRectF hb = QRectF(0, rect.top(), _drawables.Area().width(), rect.height());
	DrawableIndexVector iv = _drawables.ListOfItemIndicesInRect(hb);
	for (int i = iv.size()-1; i >= 0; --i)		// purge partial overlaps with HB
	{
		int ix = iv[i];
		if (!hb.contains(_drawables[ix]->Area()))
			iv.erase(iv.begin() + i);
	}

	if (iv.size())
	{
		std::sort(iv.begin(), iv.end(), [this](int i, int j) { return _drawables[i]->zOrder < _drawables[j]->zOrder; });

		// then separate these into 3 lists
		for (auto ix : iv)
		{
			DrawableItem* pdri = _drawables[ix];
			QRectF area = pdri->Area();
			if (rect.contains(area))
			{
				_driSelectedDrawables.push_back(ix);
				_selectionRect = _selectionRect.united(pdri->Area());
			}
			else if (area.left() < rect.left())
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
QRectF History::SelectDrawablesUnder(QPointF& p, bool addToPrevious)
{
	DrawableIndexVector iv;
	QRectF result;
	result = _drawables.ItemsUnder(p, iv);

	if (iv.size())
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
 *			into '_parent->_copiedItems' in the latter case
 *			also copies it to the clipboard
 *
 * PARAMS:	sprite: either null or pointer to sprite
 *				that contains the copied drawables
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

		DrawableItemList &pdrl = copiedItems.Items();

		std::sort(pdrl.begin(), pdrl.end(), [](DrawableItem* pl, DrawableItem* pr) {return pl->zOrder < pr->zOrder; });
																			
		if (sprite)
		{
			sprite->rect = _parent->_copiedRect;	// (0,0, width, height)
			sprite->topLeft = _selectionRect.topLeft();
		}
		else
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
	_driSelectedDrawables.clear();
	if (phi->type == HistEvent::heItemsPastedTop) // then more than one items pasted
	{
		//      n=4
		//B 1 2 3 T
		//      m = 3       n - m,
		int m = ((HistoryPasteItemTop*)phi)->count;
		_driSelectedDrawables.resize(m);		// because it is empty when pasted w. rubberBand
		for (int j = 0; j < m; ++j)
			_driSelectedDrawables[j] = ((HistoryDrawableItem*)_items[n - m + j])->indexOfDrawable;
	}
	else /*if (_driSelectedDrawables.isEmpty())	*/	// single item pasted
			_driSelectedDrawables.push_back( ((HistoryDrawableItem*)_items[n])->indexOfDrawable);

	_driSelectedDrawablesAtRight.clear();
	_driSelectedDrawablesAtLeft.clear();

	_selectionRect = rect;
}

void History::CollectDeleted(HistoryDeleteItems* phd)	// use when undoing a paste when the sprite was not moved
{
	_driSelectedDrawables.clear();
	_driSelectedDrawables = phd->deletedList;
}

// ********************************** Sprite *************
Sprite::Sprite(History* ph, const QRectF& rectf, const DrawableIndexVector &dri) : pHist(ph), driSelectedDrawables(dri)
{
	ph->CopySelected(this);
	rect = rectf;	// set in CopySelected() so must overwrite it here
}

Sprite::Sprite(History* ph, const QRectF& copiedRect, const DrawableList& lstDri)
{
	pHist = ph;
	drawables = lstDri;
	rect = copiedRect;
	itemsDeleted = false;
	for (int i = 0; i < drawables.Size(); ++i)
		driSelectedDrawables.push_back(i);
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
	data << _copiedItems.Items().size();
	for (auto sr : _copiedItems.Items())
		data << *sr;

	QMimeData* mimeData = new QMimeData;
	_copyGUID = QUuid().createUuid();

	mimeData->setData(fbmimetype, clipData);

	QString s = "fBClipBoardDataV2.0"+ _copyGUID.toString();
	mimeData->setText(s);
	_pClipBoard->setMimeData(mimeData);
}

void HistoryList::GetFromClipboard()
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

	if (formatIndex < 0)	// not our data. Maybe an image
	{
		QImage img = _pClipBoard->image();
		if (!img.isNull())
		{
			_copiedItems.Clear();
			DrawableScreenShot *pDsi = new DrawableScreenShot();
			QPixmap pxm; pxm.convertFromImage(img);
			_copiedRect = pxm.rect();
			pDsi->refPoint = _copiedRect.center();
			pDsi->SetImage(pxm);
			(*this)[-1]->SetSelectionRect(_copiedRect);	// history->-selectionRect
			_copiedItems.AddDrawable(pDsi);
		}
		return;
	}

	QString s = _pClipBoard->text();
	if (s.isEmpty() || s.left(19) != "fBClipBoardDataV2.0")
		return;

	if (_copyGUID != s.mid(19))	// new clipboard data, otherwise just copied those to _copiedItems
	{							// and there's no need to get it again from the clipboard
		_copyGUID = s.mid(19);

		_copiedItems.Clear();

		QByteArray pclipData = pMime->data(formats[formatIndex]);
		QDataStream data(pclipData);		  // uses internal QBuffer


		QString qs;
		int cnt;

		data >> _copiedRect;
		data >> cnt;

		DrawableItem* pdrwh = nullptr;
		DrawableDot dDot;
		DrawableCross dCross;
		DrawableEllipse dEll;
		DrawableLine dLin;
		DrawableRectangle dRct;
		DrawableScribble dScrb;
		DrawableText dTxt;
		DrawableScreenShot dsImg;

		DrawableItem dri;
		while (cnt--)
		{
			data >> dri;	// reads type and common data,including erasers

			switch (dri.dtType)
			{
				case DrawableType::dtCross:			(DrawableItem&)dCross= dri; data >> dCross	;pdrwh = new DrawableCross(dCross)		;break;
				case DrawableType::dtDot:			(DrawableItem&)dDot  = dri; data >> dDot	;pdrwh = new DrawableDot(dDot)			;break;
				case DrawableType::dtEllipse:		(DrawableItem&)dEll  = dri; data >> dEll	;pdrwh = new DrawableEllipse(dEll)		;break;
				case DrawableType::dtLine:			(DrawableItem&)dLin  = dri; data >> dLin	;pdrwh = new DrawableLine(dLin)			;break;
				case DrawableType::dtRectangle:		(DrawableItem&)dRct  = dri; data >> dRct	;pdrwh = new DrawableRectangle(dRct)	;break;
				case DrawableType::dtScreenShot:	(DrawableItem&)dsImg = dri; data >> dsImg	;pdrwh = new DrawableScreenShot(dsImg)	;break;
				case DrawableType::dtScribble:		(DrawableItem&)dScrb = dri; data >> dScrb	;pdrwh = new DrawableScribble(dScrb)	;break;
				case DrawableType::dtText:			(DrawableItem&)dTxt  = dri; data >> dTxt	;pdrwh = new DrawableText(dTxt)			;break;
				default: break;
			}
			if (pdrwh && (int)dri.dtType < (int)DrawableType::dtNonDrawableStart)	// only add drawables
				_copiedItems.AddDrawable(pdrwh);
			dri.erasers.clear();
		}
	}
}
