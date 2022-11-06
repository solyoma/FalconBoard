#include <QApplication>
#include <QMouseEvent>

#include <QFileDialog>
#include <QPainter>

#include <QMessageBox>
#include <QDesktopServices>

#if defined(QT_PRINTSUPPORT_LIB)
	#include <QtPrintSupport/qtprintsupportglobal.h>
	#if QT_CONFIG(printdialog)
		#include <QPrinter>
		#include <QPrintDialog>
		#include <QPrinterInfo>
	#endif
#endif

#include <math.h>

#include "DrawArea.h"

#define DEBUG_LOG(qs) \
{							 \
	QFile f("debug.log");		 \
	f.open(QIODevice::Append);	 \
	QTextStream ofsdbg(&f);		 \
	ofsdbg << qs << "\n";			 \
}

// !!!
DrawColors drawColors;      // global used here and for print, declared in common.h
PenCursors penCursors;

//----------------------------- DrawArea ---------------------
DrawArea::DrawArea(QWidget* parent) : QWidget(parent)
{
	_pActCanvas = &_canvas1;            // actual draw canvas
	_pOtherCanvas = &_canvas2;           // used for smooth scrolling

	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_TabletTracking);
	setCursor(Qt::CrossCursor);
	setMouseTracking(true);
	historyList.reserve(10);                  // max number of TABs possible is 10, must be checked
	drawColors.Setup();
	penCursors.Setup();
}

void DrawArea::SetScreenSize(QSize screenSize)
{
	_prdata.screenPageWidth = screenSize.width();
	_screenHeight = screenSize.height();
}

void DrawArea::SetMyPrinterData(const MyPrinterData& prdata)
{
	_prdata = prdata;
}

void DrawArea::ClearRoll()
{
#ifndef _VIEWER
	HideRubberBand(true);
#endif
	_history->AddClearRoll();
	_ClearCanvas();
}

void DrawArea::ClearVisibleScreen()
{
#ifndef _VIEWER
	HideRubberBand(true);
#endif
	_history->AddClearVisibleScreen();
	_ClearCanvas();
}

void DrawArea::ClearDown()
{
#ifndef _VIEWER
	HideRubberBand(true);
#endif
	_history->AddClearDown();
	_ClearCanvas();
}

void DrawArea::ClearBackground()
{
	_background = QImage();
	_isBackgroundSet = false;
	update();
}


/*========================================================
 * TASK:    adds new history and set it to be the current one
 *          if a name is given tries to load the history as well
 * PARAMS:  name: name for new history. String may be empty
 *                  in which case a new unsaved page is added
 *                  or if it is 'Untitled.mwb' (depending on language)
 *                  then does not load it even if it exists
 *          insertAt - insert at this position.
 *                  if > capacity then at the end
 *          loadIt - if name is not empty also load data
 * GLOBALS: _history, _topLeft,_currentHistoryIndex
 * RETURNS: index of added history or
 *              -1 if capacity reached or
 *              -2 if file could not not be loaded
 *					because either file does not exist
 *					or a read error
 * REMARKS: - adding history is unsucessfull if all slots are full
 *          - capacity is always smaller than 100000!
 *-------------------------------------------------------*/
int DrawArea::AddHistory(const QString name, bool loadIt, int insertAt)
{
	if (historyList.capacity() == (unsigned long)HistoryListSize())
		return -1;

	if (_history)
		_history->SetTopLeft(_topLeft);

	History* ph = new History(&historyList);
	//    _history.SetCopiedLists(&_copiedImages, &_copiedItems, &_copiedRect);
	if (!name.isEmpty())
		ph->SetName(name);

	if ((unsigned long)insertAt > historyList.capacity())
	{
		historyList.push_back(ph);
		insertAt = HistoryListSize() - 1;
	}
	else
		historyList.insert(historyList.begin() + insertAt, ph);

	bool b = _currentHistoryIndex == insertAt;
	_currentHistoryIndex = insertAt;
	_history = historyList[insertAt];
	if (!name.isEmpty() && loadIt)
	{
		int res = _history->Load();
		if(res < 0)
			return -2;
	}
	_topLeft = QPointF(0, 0);

	if (!b)
		_Redraw(true);
	return insertAt;
}


/*========================================================
 * TASK: set a new history into _currentHistoryIndex and
 *          _history
 * PARAMS: index: switch to here, if < 0
 *              use curent history
 *         redraw: redraw history after the switch
 *         invalidate: set invalid current history?
 *          used only in ShowEvent
 * GLOBALS:
 * RETURNS:
 * REMARKS: - redraw is false when e.g. not the active
 *              tab is closed in FalconBoard
 *-------------------------------------------------------*/
bool DrawArea::SwitchToHistory(int index, bool redraw, bool invalidate)   // use this before others
{
	if (invalidate)
	{
		_currentHistoryIndex = -1;
		return true;
	}

	if (index >= HistoryListSize())
		return false;
	if (index != _currentHistoryIndex)
	{
#ifndef _VIEWER
		HideRubberBand(true);
#endif
		if (index >= 0)     // store last viewport into previously shown history
		{
			if (_history)    // there is a history
				_history->SetTopLeft(_topLeft);
			_currentHistoryIndex = index;
			_history = historyList[index];
			_topLeft = _history->TopLeft();
		}
		else
			index = _currentHistoryIndex;
	}
	int res = 1;
	if (redraw)
	{
		res = _history->Load();   // only when it wasn't loaded before
		_SetCanvasAndClippingRect();
		_Redraw(true);
#ifndef _VIEWER
		_ShowCoordinates(QPointF());
		emit CanUndo(_history->CanUndo());
		emit CanRedo(_history->CanRedo());
#endif
	}
	return res >= 0;
}


/*========================================================
 * TASK:    removes the index-th history from the list
 * PARAMS:  index - remove this
 * GLOBALS:
 * RETURNS: history size
 * REMARKS: - synchronize with TABs in FalconBoard
 *          - after called _currentHistoryIndex is invalid
 *-------------------------------------------------------*/
int DrawArea::RemoveHistory(int index)
{
	int cnt = HistoryListSize();
	if (index < 0 || index > cnt)
		return -1;

	History *phi = historyList[index];
	historyList.erase(historyList.begin() + index);
	delete phi;
	--cnt;
	if (index == _currentHistoryIndex)
	{
		_currentHistoryIndex = -1;
		_history = nullptr;
	}
	else if (index < _currentHistoryIndex)
		--_currentHistoryIndex;
	if (!cnt)
		_ClearCanvas();
	return cnt;
}


/*========================================================
 * TASK:    moves history index' from' to 'to', while
 *          the 'to'th history is moved to 'from'
 * PARAMS:
 * GLOBALS:
 * RETURNS:
 * REMARKS: - _history will not change
 *          - _to == _currentHistoryIndex
 *-------------------------------------------------------*/
void DrawArea::MoveHistory(int from, int to)
{
	History* pdh = historyList[to];
	historyList[to] = historyList[from];
	historyList[from] = pdh;
	_currentHistoryIndex = from;
}

int DrawArea::Load()
{
#ifndef _VIEWER
	HideRubberBand(true);
#endif
	int res = _history->Load(); // only loads if names diferent

	QString qs = _history->Name(); //  .mid(_history->Name().lastIndexOf('/') + 1);
	if (!res)
		QMessageBox::about(this, tr(WindowTitle), QString(tr("'%1'\nInvalid file")).arg(qs));
	else if (res == -1)
		QMessageBox::about(this, tr(WindowTitle), QString(tr("File\n'%1'\n not found")).arg(qs));
	else if (res < 0)    // i.e. < -1
		QMessageBox::about(this, tr(WindowTitle), QString(tr("File read problem. %1 records read. Please save the file to correct this error")).arg(-res - 2));
	if (res && res != -1)    // TODO send message if read error
	{
		_topLeft = QPointF(0, 0);
		_Redraw();
	}
	emit CanUndo(_history->CanUndo());    // no undo or redo after open file
	emit CanRedo(_history->CanRedo());
	update();
	return res;
}

bool DrawArea::EnableRedraw(bool value)
{
	bool b = _mustRedrawArea;
	_mustRedrawArea = value;
	if (value && _redrawPending)  // redraw was requested but not performed
		_Redraw();

	return b;
}

#ifndef _VIEWER
bool DrawArea::OpenBackgroundImage(const QString& fileName)
{
	if (!_background.load(fileName))
		return false;

	return true;
}

bool DrawArea::SaveVisibleImage(const QString& fileName, const char* fileFormat)
{
	HideRubberBand(true);

	QImage visibleImage = grab().toImage();
	if (visibleImage.save(fileName, fileFormat))
	{
		return true;
	}
	return false;
}

void DrawArea::ApplyTransparencyToLoadedScreenshots(QColor trcolor, qreal fuzzyness)
{
	_history->AddScreenShotTransparencyToLoadedItems(trcolor, fuzzyness);
	_Redraw();
}
#endif

void DrawArea::GotoPage(int page)
{
	_SetOrigin(QPointF(0, --page * _prdata.screenPageHeight));
	_Redraw();
}

void DrawArea::SetMode(bool darkMode, QString color, QString sGridColor, QString sPageGuideColor)
{
	_backgroundColor = color;
	_gridColor = sGridColor;
	_pageGuideColor = sPageGuideColor;
	drawColors.SetDarkMode(_darkMode = darkMode);
	penCursors.Setup();
	_Redraw();                  // because pen color changed!
	SetCursor(_erasemode ? csEraser : csPen);
}

void DrawArea::SetPenKind(FalconPenKind newKind, int width)
{
	_actPenKind = newKind;
	if (width > 0)
		_penWidth = width;
}

void DrawArea::AddScreenShotImage(QPixmap& animage)
{
	DrawableScreenShot bimg;
	int x = (geometry().width() -  animage.width()) / 2,
		y = (geometry().height() - animage.height()) / 2;

	if (x < 0) x = 0;
	if (y < 0) y = 0;
	bimg.startPos = QPointF(x, y) + _topLeft;
	bimg.image = animage;
	HistoryItem *phi = _history->AddDrawableItem(bimg);

#if !defined _VIEWER
	int drix = ((HistoryDrawableItem*)phi)->indexOfDrawable;    // latest image
	if (drix >= 0)
	{
		delete _rubberBand;
		_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
		_rubberBand->setGeometry(_history->Drawable(drix)->Area().translated(-_topLeft).toRect() );
		_rubberRect = _rubberBand->geometry();  // _history->BoundingRect().translated(-_topLeft);
		_rubberBand->show();
		_history->AddToSelection(drix, true);
	}
#endif

	emit CanUndo(_history->CanUndo());
	emit CanRedo(_history->CanRedo());
}


/*========================================================
 * TASK:    check history if it is modified
 * PARAMS:  fromIndex - start checking at this index, except
 *						when it is -1: then use '_currentHistoryIndex'
 *          any		  -	if true check all histories until the
 *						first modified is found this case
 * GLOBALS: _currentHistoryIndex
 * RETURNS: if any == false: history index + 1 if modified or 0
 *                 == true : the index +1 of the first modified
 *							 history starting at 'fromIndex'
 * REMARKS: - only set 'any' to true when closing the program.
 *-------------------------------------------------------*/
int DrawArea::IsModified(int fromIndex, bool any) const
{
	if (fromIndex < 0)
		fromIndex = _currentHistoryIndex;
	if (!any)
		return historyList[fromIndex]->IsModified() ? fromIndex + 1 : 0;
	// else 
	for (int i = fromIndex; i < HistoryListSize(); ++i)
		if (historyList[i]->IsModified())
			return  i + 1;
	return 0;
}

#ifndef _VIEWER
void DrawArea::InsertVertSpace()
{
	HideRubberBand(true);
	_history->AddInsertVertSpace(_rubberRect.y() + _topLeft.y(), _rubberRect.height());
	_Redraw();
}
FalconPenKind DrawArea::PenKindFromKey(int key)
{
	switch (key)
	{
	case Qt::Key_1: return penBlack;
	case Qt::Key_2: return penRed;
	case Qt::Key_3: return penGreen;
	case Qt::Key_4: return penBlue;
	case Qt::Key_5: return penYellow;
	default: return penNone;
	}
}
bool DrawArea::RecolorSelected(int key)
{
	if (!_rubberBand)
		return false;
	if (!_history->SelectedSize())
		_history->CollectDrawablesInside(_rubberRect.translated(_topLeft));

	FalconPenKind pk = PenKindFromKey(key);
	HistoryItem* phi = _history->AddRecolor(pk);

	if (phi)
		_Redraw();
	return true;
}
void DrawArea::SynthesizeKeyEvent(Qt::Key key, Qt::KeyboardModifier mod)
{
	QKeyEvent event(QEvent::KeyPress, key, mod);
	keyPressEvent(&event);
}
#endif

void DrawArea::NewData()    // current history
{
	_history->Clear();
	ClearBackground();
	_ClearCanvas();
	_erasemode = false; // previous last operation might have been an erease
}

void DrawArea::_ClearCanvas() // uses _clippingRect
{
	if (_clippingRect == _canvasRect || !_clippingRect.isValid() || _clippingRect.isNull())
	{
		_pActCanvas->fill(Qt::transparent);     // transparent
		_clippingRect = _canvasRect;
		_lastPointC.setX(-1);   // invalid point
	}
	else
	{
		QPainter painter(_pActCanvas);
		painter.setCompositionMode(QPainter::CompositionMode_Clear);
		// DEBUG
	//qDebug("topLeft: (%d, %d), clipping: (%d, %d, %d, %d)", _topLeft.x(), _topLeft.y(), 
	//                                                        _clippingRect.x(),     _clippingRect.y(), 
	//                                                        _clippingRect.width(), _clippingRect.height());
	//qDebug("  translated by topLeft(%d, %d, %d, %d)", _clippingRect.translated(-_topLeft).x(), _clippingRect.translated(-_topLeft).y(),
	//                                                  _clippingRect.translated(-_topLeft).width(), _clippingRect.translated(-_topLeft).height());
	// /DEBUG            

		painter.fillRect(_clippingRect.translated(_topLeft), qRgba(255, 255, 255, 0));
	}
	update();
}

#ifndef _VIEWER
void DrawArea::ChangePenColorByKeyboard(int key)
{
	FalconPenKind pk = PenKindFromKey(key);
	emit PenKindChange(pk);
}

#endif

void DrawArea::keyPressEvent(QKeyEvent* event)
{
	_mods = event->modifiers();
	int key = event->key();

	// DEBUG
#ifndef _VIEWER
	if (key == Qt::Key_S)
	{
		_bSmoothDebug = !_bSmoothDebug;
		qDebug("Smoothing %s", (_bSmoothDebug ? "on" : "off"));
	}
#endif
	// end DEBUG


#if !defined _VIEWER && defined _DEBUG
	bool redraw = false;
	if (key == Qt::Key_D && (_mods.testFlag(Qt::ControlModifier) && _mods.testFlag(Qt::ShiftModifier)))
		_pencilmode = !_pencilmode, redraw=true;
	if (key == Qt::Key_D && (_mods.testFlag(Qt::ControlModifier) && _mods.testFlag(Qt::AltModifier)))
	{                           // toggle debug mode
		_debugmode = !_debugmode;
		redraw = true;
	}
	if(redraw)
		_Redraw();
#endif

	if (!_scribbling && !_pendown && key == Qt::Key_Space && !event->isAutoRepeat())
	{
		_spaceBarDown = true;
		QWidget::keyPressEvent(event);
	}
	else // if (event->spontaneous())
	{
#ifndef _VIEWER
		bool bPaste = // _itemsCopied &&
			((key == Qt::Key_Insert && _mods.testFlag(Qt::ShiftModifier)) ||
				(key == Qt::Key_V && _mods.testFlag(Qt::ControlModifier))
				);

		// lambda to draw a cross at a given point
		auto __DrawCross = [&](QPointF p, int halflen) // p rel. to Document top/left
		{
			_actPenWidth = _penWidth;
			_lastDrawableCross = DrawableCross(p, halflen, _history->GetZorder(false), _actPenKind, _actPenWidth);

			QPainter* painter = _GetPainter(_pActCanvas);
			_lastDrawableCross.Draw(painter, _topLeft);
			delete painter;
			_firstPointC = _lastPointC = _lastDrawableCross.startPos;;

			(void)_history->AddDrawableItem(_lastDrawableCross);
			_history->AddToSelection(-1);
			update();
		};

		if (_rubberBand) 
		{

			bool bDelete = key == Qt::Key_Delete || key == Qt::Key_Backspace,
				bCut = ((key == Qt::Key_X) && _mods.testFlag(Qt::ControlModifier)) ||
				((key == Qt::Key_Insert) && _mods.testFlag(Qt::ShiftModifier)),
				bCopy = (key == Qt::Key_Insert || key == Qt::Key_C || key == Qt::Key_X) &&
				_mods.testFlag(Qt::ControlModifier),
				bBracketKey = (key == Qt::Key_BracketLeft || key == Qt::Key_BracketRight),
				bRemove = (bDelete | bCopy | bCut | bPaste) ||
					(!bBracketKey && key != Qt::Key_Control && key != Qt::Key_Shift && key != Qt::Key_Alt && key != Qt::Key_R && key != Qt::Key_C &&
					 key != Qt::Key_Space && key != Qt::Key_Up && key != Qt::Key_Down && key != Qt::Key_Left &&
					 key != Qt::Key_Right && key != Qt::Key_PageUp && key != Qt::Key_PageDown),
				bCollected = _history->SelectedSize(),
				bRecolor = (key == Qt::Key_1 || key == Qt::Key_2 || key == Qt::Key_3 || key == Qt::Key_4 || key == Qt::Key_5),

				bRotate = (key == Qt::Key_0 ||  // rotate right by 90 degrees
					key == Qt::Key_8 ||  // rotate by 180 degrees
					key == Qt::Key_9 ||  // rotate left by 90 degrees
					key == Qt::Key_H ||  // flip horizontally
					(key == Qt::Key_V && !_mods)       // flip vertically when no modifier keys pressed
					);
			if (bCollected && bBracketKey)
			{
				if (key == Qt::Key_BracketLeft) // decrease pen width for all drawables inside selection by 1
				{
					_history->AddPenWidthChange(-1);
					_Redraw();
				}
				else if (key == Qt::Key_BracketRight) // increase pen width for all drawables inside selection by 1
				{
					_history->AddPenWidthChange(1);
					_Redraw();
				}
			}

			if (bDelete || bCopy || bRecolor || bRotate)
			{
				if (bCollected && !bDelete && !bRotate)
				{
					_history->CopySelected();
					_itemsCopied = true;        // never remove selected list
				}
			}
			// if !bCollected then history's _selectedList is empty, but the rubberRect is set into _selectionRect
			if ((bCut || bDelete) && bCollected)
			{
				_history->AddDeleteItems();
				HideRubberBand(true);
				_Redraw();
			}
			else if (bDelete && !bCollected)     // delete empty area
			{
				QRectF rect = _rubberRect.translated(_topLeft);
				if (_history->AddRemoveSpaceItem(rect))     // there was something (not delete below the last item)
				{
					HideRubberBand(true);
					_Redraw();
				}
			}
			else if (bPaste)
			{           // _history's copied item list is valid, each item is canvas relative
						// get offset to top left of encompassing rect of copied items relative to '_topLeft'
				QPointF dr = _rubberRect.translated(_topLeft).topLeft();

				HistoryItem* phi = _history->AddPastedItems(dr, nullptr);	// no sprite, add
				if (phi)
				{
					_Redraw(); // update();
					_rubberRect = QRectF(_rubberRect.topLeft(), historyList.CopiedRect().size());
					_rubberBand->setGeometry(_rubberRect.toRect());
					_history->CollectPasted(_rubberRect.translated(_topLeft));
				}
			}
			else if (bRecolor)
			{
				RecolorSelected(key);
			}

			else if (bRotate && bCollected)
			{
				MyRotation rot = rotNone;
				switch (key)
				{
				case Qt::Key_0:rot = rotL90; break;
				case Qt::Key_8: rot = rot180; break;
				case Qt::Key_9: rot = rotR90; break;
				case Qt::Key_H: rot = rotFlipH; break;
				case Qt::Key_V: rot = rotFlipV; break;
				}
				if (rot != rotNone)
				{
					_history->AddRotationItem(rot);
					_Redraw();
				}
			}
			else if (key == Qt::Key_R)    // draw rectangle with margin or w,o, margin with Ctrl or filled with Shift
			{
				_actPenWidth = _penWidth;

				// when any drawables is selected draw the rectangle around them 
				// leaving _actPenWidth/2+1 pixel margins on each sides
				qreal adjustment = ((qreal)_actPenWidth + 1) / 2 + 1;

				qreal margin = !_mods.testFlag(Qt::ControlModifier) && _history->SelectedSize() ?   adjustment : 0;
				QRectF r = _rubberRect.adjusted(-margin, -margin,margin,margin);	// will also resizes _rubberRect

				_lastRectangleItem = DrawableRectangle(r.translated(_topLeft), _history->GetZorder(false), _actPenKind, _actPenWidth, _mods.testFlag(Qt::ShiftModifier));

				QPainter* painter = _GetPainter(_pActCanvas);
				_lastRectangleItem.Draw(painter, _topLeft);
				delete painter;

				_firstPointC = _lastPointC = _lastRectangleItem.GetLastDrawnPoint();

				_rubberRect = r.adjusted(-adjustment, -adjustment, adjustment, adjustment);
				_rubberBand->setGeometry(_rubberRect.toRect());
				 (void)_history->AddDrawableItem(_lastRectangleItem);
				 if(!_erasemode)
					_history->AddToSelection(-1);
			}
			else if (key == Qt::Key_C && !bCopy)   // draw ellipse
			{
				_actPenWidth = _penWidth;
				qreal adjustment = ((qreal)_actPenWidth + 1) / 2 + 1;
				_lastEllipseItem = DrawableEllipse(_rubberRect.translated(_topLeft), _history->GetZorder(false), _actPenKind, _actPenWidth, _mods.testFlag(Qt::ShiftModifier));
				
				QPainter* painter = _GetPainter(_pActCanvas);
				_lastEllipseItem.Draw(painter, _topLeft);
				delete painter;

				_rubberRect = _rubberRect.adjusted(-adjustment, -adjustment, adjustment, adjustment);
				_rubberBand->setGeometry(_rubberRect.toRect());

				_firstPointC = _lastPointC = _lastEllipseItem.GetLastDrawnPoint();


				(void) _history->AddDrawableItem(_lastEllipseItem);
				 if(!_erasemode)
					_history->AddToSelection(-1);
			}
			else if (key == Qt::Key_X && !bCut)   // mark center with cross
			{
				__DrawCross(_rubberRect.translated(_topLeft).center(), 10);
			}
			else if (key == Qt::Key_Period)   // mark center with cross or a period
			{
				_actPenWidth = _penWidth;
				_lastDotItem = DrawableDot(_rubberRect.translated(_topLeft).center(), _history->GetZorder(false), _actPenKind, _actPenWidth);

				QPainter* painter = _GetPainter(_pActCanvas);
				_lastDotItem.Draw(painter, _topLeft);
				delete painter;

				(void) _history->AddDrawableItem(_lastDotItem);
				 if(!_erasemode)
					_history->AddToSelection(-1);
				update();
			}
			else if (bRemove)			   // delete rubberband for any keypress except pure modifiers  or space bar
				HideRubberBand(true);
		}
		else    // no rubberBand
#endif
		{
			int stepSize = _mods.testFlag(Qt::ControlModifier) ? LargeStep : (_mods.testFlag(Qt::ShiftModifier) ? SmallStep : NormalStep);
#ifndef _VIEWER
			if (bPaste)         // paste as sprite
			{
				historyList.PasteFromClipboard();

				if (!historyList.CopiedItems().Size())    // anything to paste?
				{
					_pSprite = _SpriteFromLists();
					_PasteSprite();
					_Redraw();
				}
			}
			else if (key == Qt::Key_X && !_mods.testFlag(Qt::ControlModifier) )
				__DrawCross(_actMousePos, 10);
			else 
#endif
				if (key == Qt::Key_PageUp)
					_PageUp();
				else  if (key == Qt::Key_PageDown)
					_PageDown();
				else  if (key == Qt::Key_Home)
					_Home(_mods.testFlag(Qt::ControlModifier));
				else  if (key == Qt::Key_End)
					_End(_mods.testFlag(Qt::ControlModifier));
			//            else if (key == Qt::Key_Shift)
			//				_shiftKeyDown = true;

				else if (key == Qt::Key_Up)
					_Up(stepSize);
				else if (key == Qt::Key_Down)
					_Down(stepSize);
				else if (key == Qt::Key_Left)
					_Left(stepSize);
				else if (key == Qt::Key_Right)
					_Right(stepSize);
				else if (key == Qt::Key_BracketRight)
					emit IncreaseBrushSize(1);
				else if (key == Qt::Key_BracketLeft)
					emit DecreaseBrushSize(1);
				else if (key == Qt::Key_F4 && _mods.testFlag(Qt::ControlModifier))
					emit CloseTab(_currentHistoryIndex);
				else if ((key == Qt::Key_Tab || key == Qt::Key_Backtab) && _mods.testFlag(Qt::ControlModifier))
					emit TabSwitched(key == Qt::Key_Backtab ? -1 : 1); // Qt: Shit+Tab is Backtab
#ifndef _VIEWER
				else if (key == Qt::Key_1 || key == Qt::Key_2 || key == Qt::Key_3 || key == Qt::Key_4 || key == Qt::Key_5)
					ChangePenColorByKeyboard(key);
#endif
		}
#ifndef _VIEWER
		if (key != Qt::Key_Control && key != Qt::Key_Shift && key != Qt::Key_Alt)
		{
			emit CanUndo(_history->CanUndo());
			emit CanRedo(_history->CanRedo());
		}
#endif
	}
}

void DrawArea::keyReleaseEvent(QKeyEvent* event)
{
	Qt::KeyboardModifiers newmods = event->modifiers();
	if (_mods.testFlag(Qt::ShiftModifier) && !newmods.testFlag(Qt::ShiftModifier))
	{
		_drawStarted = false;
		_firstPointC = _lastPointC;
	}
	_mods = newmods;

	if ((!_spaceBarDown && (!_mods.testFlag(Qt::ShiftModifier) || !event->spontaneous() || event->isAutoRepeat()))) //?
	{
		QWidget::keyReleaseEvent(event);
		return;
	}

	if (event->key() == Qt::Key_Space)
	{
		if (!event->isAutoRepeat())
		{
			_spaceBarDown = false;
			if (_scribbling || _pendown)
			{
				_RestoreCursor();
				QWidget::keyReleaseEvent(event);
				return;
			}
		}
	}
	//if(event->key() == Qt::Key_Shift)
	//    _shiftKeyDown = false;

	_drawStarted = false;
	//_altKeyDown = event->key() == Qt::Key_Alt;
	QWidget::keyReleaseEvent(event);
}

void DrawArea::mousePressEvent(QMouseEvent* event)
{
	MyPointerEvent* pe = new MyPointerEvent(false, event);
	MyButtonPressEvent(pe);
	emit WantFocus();
}

void DrawArea::mouseMoveEvent(QMouseEvent* event)
{
	MyPointerEvent* pe = new MyPointerEvent(false, event);
	MyMoveEvent(pe);
}

void DrawArea::wheelEvent(QWheelEvent* event)   // scroll the screen
{
	if (_pendown || _scribbling)
	{
		event->ignore();
		return;
	}
	_mods = event->modifiers();

	//int y  = _topLeft.y();
	static int dy = 0;
	static int dx = 0;
	static int degv = 0;
	static int degh = 0;

	degh += event->angleDelta().x();
	degv += event->angleDelta().y();
	dx += 2 * event->pixelDelta().x();      // on Win10 64 bit with Wacom tablet from pen
	dy += 2 * event->pixelDelta().y();

	int deg15h = degh / 8,
		deg15v = degv / 8;
	if (!deg15v && dy)
		deg15v = dy;
	if (!deg15h && dx)
		deg15h = dx;

	const constexpr int tolerance = 3;
	if (deg15h > tolerance || deg15h < -tolerance || deg15v > tolerance || deg15v < -tolerance)

	{

		// DEBUG
//        qDebug() << deg15h << ":" << deg15v;
		// /DEBUG
		_ShiftAndDisplayBy(QPointF(deg15h, -deg15v)); // dy < 0 => move viewport down
		degv = degh = 0;
		dx = dy = 0;

	}
	else
		event->ignore();
#ifndef _VIEWER
	_ReshowRubberBand();
#endif
}

void DrawArea::mouseReleaseEvent(QMouseEvent* event)
{
	MyPointerEvent* pe = new MyPointerEvent(false, event);
	MyButtonReleaseEvent(pe);
	emit WantFocus();
}


void DrawArea::tabletEvent(QTabletEvent* event)
{
	if (!_allowPen)
		return;
	//// DEBUG
	//#ifndef _VIEWER
	// #if defined _DEBUG
	//    if(_rubberBand)
	//        qDebug("Tablet Button: rubberband size:%d,%d", _rubberBand->size().width(), _rubberBand->size().height());
	// #endif
	//#endif
	//// /DEBUG
	MyPointerEvent* pe = new MyPointerEvent(true, event);

	/*    QTabletEvent::PointerType pointerT = event->pointerType(); */

	switch (event->type())
	{
		case QEvent::TabletPress:
			MyButtonPressEvent(pe);
			event->accept();
			break;

		case QEvent::TabletMove:
#ifndef Q_OS_IOS
			//        if (event->device() == QTabletEvent::RotationStylus)
			//            updateCursor(event);
#endif
			MyMoveEvent(pe);
			event->accept();
			break;

		case QEvent::TabletRelease:
			MyButtonReleaseEvent(pe);
			emit PointerTypeChange(QTabletEvent::Cursor);
			event->accept();
			break;

		default:
			event->ignore();
			break;
	}
}

/*=============================================================
* TASK:	    Common button press handling for mouse and tablet
* PARAMS:
* GLOBALS:
* RETURNS:
* REMARKS:
*------------------------------------------------------------*/
void DrawArea::MyButtonPressEvent(MyPointerEvent* event)
{
	if ((!_allowPen && event->fromPen) || (!_allowMouse && !event->fromPen))
		return;
	if (event->fromPen)
		_allowMouse = false;
	else
		_allowPen = false;
#ifdef _VIEWER
	_lastPointC = event->pos;     // used for moving the canvas around
#else
	if (event->button == Qt::RightButton ||        // Even for tablets when no pen pressure
		(event->button == Qt::LeftButton && event->mods.testFlag(Qt::ControlModifier)))
	{
		_InitRubberBand(event);
		_lastPointC = event->pos;

	}
	else
#endif
		if (event->pressure != 0.0 && event->button == Qt::LeftButton && !_pendown)  // even when using a pen some mouse messages still appear
		{
			if (_spaceBarDown)
				_SaveCursorAndReplaceItWith(Qt::ClosedHandCursor);

			if (event->fromPen)
			{
				_pendown = true;
				emit PointerTypeChange(event->pointerT);
			}
			else
				_scribbling = true;
#ifndef _VIEWER
			if (_rubberBand)
			{
				if (_history->SelectedSize() && _rubberRect.contains(event->pos))
				{
					if (_CreateSprite(event->pos, _rubberRect, !event->mods.testFlag(Qt::AltModifier)))
					{
						if (event->mods.testFlag(Qt::AltModifier) /*&& _mods.testFlag(Qt::ControlModifier)*/)
							/* do nothing */;
						else
						{
							_history->AddDeleteItems(_pSprite);
							_Redraw();
						}
						//                    QApplication::processEvents();
					}

				}

				HideRubberBand(!_spaceBarDown);   // else move canavs with mouse, and move rubberband with it as well
			}

			if (event->mods.testFlag(Qt::ShiftModifier))    // will draw straight line from last position to actual position
			{
				_firstPointC = _lastPointC;
				_InitiateDrawingIngFromLastPos();       // instead of the event's position
			}
			else
				_InitiateDrawing(event);                // resets _firstPointC and _lastPointC to event_>pos()
#endif
		}
#ifndef _VIEWER
	_ShowCoordinates(event->pos);
#endif
}

/*=============================================================
 * TASK:    move event for mouse and tablet
 * PARAMS:
 * GLOBALS:
 * RETURNS:
 * REMARKS:
 *------------------------------------------------------------*/
void DrawArea::MyMoveEvent(MyPointerEvent* event)
{
#ifndef _VIEWER
	_ShowCoordinates(event->pos);
#endif
	if ((!_allowPen && event->fromPen) || (!_allowMouse && !event->fromPen))
		return;

	constexpr const int REFRESH_LIMIT = 10;
	static int counter = 0; // only refresh screen when this is REFRESH_LIMIT
						// because tablet events frequency is large

#ifndef _VIEWER
	if (_rubberBand)
	{
		QPointF epos = event->pos;
		if (epos.x() < 0)
			epos.setX(0);
		if (epos.y() < 0)
			epos.setY(0);
		
		if (_spaceBarDown && (event->buttons & Qt::LeftButton))
		{
			if (event->fromPen)
			{
				++counter;
				if (counter >= REFRESH_LIMIT)
				{
					counter = 0;
					_Redraw();
					_lastPointC = epos;
				}
			}
			QPointF  dr = (epos - _lastPointC);   // displacement vector
			if (!dr.manhattanLength())
				return;
			_ShiftOrigin(-dr);

			if (!event->fromPen)    // else already redrawn
			{
				_Redraw();
				_lastPointC = epos;
			}

			_ReshowRubberBand();
		}
		else if (((event->buttons & Qt::RightButton) ||
			((event->buttons & Qt::LeftButton) && event->mods.testFlag(Qt::ControlModifier))))
		{
			// modify existing rubber band using either the right button or Ctrl+left button
			// if the shift is held down the shape will be a square
			// if holding down Alt then the rubberband will be centered on its starting poition
			if (event->mods.testFlag(Qt::ShiftModifier))        // constrain rubberband to a square
				epos.setX(_rubberBand->geometry().x() + (epos.y() - _rubberBand->geometry().y()));

			QPointF origin = _rubber_origin;	// we may modify this because of Alt modifier
			QPointF delta;
			if (event->mods.testFlag(Qt::AltModifier))	// then the rubberband goes outward from origin
			{
				delta = epos - _rubber_origin;	// some coordinate may be negative
				origin -= delta;
			}
			// DEBUG
			//qDebug("epos:(%d,%d), delta:(%d, %d), origin:(%d,%d), rubber0: (%d,%d)",
			//	epos.x(), epos.y(), delta.x(), delta.y(), origin.x(), origin.y(),
			//	_rubber_origin.x(), _rubber_origin.y());
			_rubberBand->setGeometry(QRectF(origin, epos).normalized().toRect()); // means: top < bottom, left < right

	// DEBUG
	//#if defined _DEBUG
	//        qDebug("%s: rubberband size:%d,%d", (event->fromPen ? "tablet":"mouse:"),_rubberBand->size().width(), _rubberBand->size().height());
	//#endif
	// /DEBUG
		}
	}
	else
		// no rubber band
#endif
				// mouse  or pen                                     pen
		if (((event->buttons & Qt::LeftButton) && _scribbling) || _pendown)
		{
			static QPointF lastpos;
			QPointF pos = event->pos;
			if (lastpos == pos)
				return;
			else
				lastpos = pos;

			QPointF  dr = (event->pos - _lastPointC);   // displacement vector
			if (!dr.manhattanLength()
#ifndef _VIEWER
				&& (_AutoScrollDirection(event->pos) != _ScrollDirection::scrollNone)
#endif
				)
				return;

#ifndef _VIEWER
			if (_pSprite)     // move sprite
			{
				if (_AutoScrollDirection(event->pos) == _ScrollDirection::scrollNone)
				{
					_RemoveScrollTimer();
					_MoveSprite(dr);
				}
				else
					_AddScrollTimer();
				_lastPointC = event->pos;
			}
			else
			{
#endif
				if (_spaceBarDown)
				{
					if (event->fromPen)
					{
						++counter;
						if (counter >= REFRESH_LIMIT)
						{
							counter = 0;
							_Redraw();
						}
					}
					_ShiftOrigin(-dr);
					if (!event->fromPen)
						_Redraw();

					_lastPointC = event->pos;
				}
#ifndef _VIEWER
				else
				{	
					QPointF tmp = event->pos;
					if (_CanSavePoint(tmp))
					{
						_CorrectForDirection(tmp);
						tmp = _lastScribbleItem.Add(tmp + _topLeft, true) - _topLeft;	// add and smooth
						_DrawLineTo(tmp);
					}
				}
			}
#endif
		}
}

void DrawArea::MyButtonReleaseEvent(MyPointerEvent* event)
{
#ifndef _VIEWER
	_RemoveScrollTimer();
#endif

	if ((!_allowPen && event->fromPen) || (!_allowMouse && !event->fromPen) || (_allowMouse && _allowPen))
		return;

	if ((event->button == Qt::LeftButton && _scribbling) || _pendown)
	{
#ifndef _VIEWER
		if (_pSprite)
		{
			_PasteSprite();
			_Redraw();
		}
		else
		{
			if (!_spaceBarDown)
			{
				//DEBUG_LOG(QString("Mouse release #1: _lastPoint: (%1,%2)").arg(_lastPointC.x()).arg(_lastPointC.y()))
				if (_DrawFreehandLineTo(event->pos))
					_lastScribbleItem.Add(_lastPointC + _topLeft, false);		// add but do not smooth last point
				DrawableItem* pdrwi = &_lastScribbleItem;

				if (_lastScribbleItem.points.size() == 2)
				{
					if (_lastScribbleItem.points.at(0) == _lastScribbleItem.points.at(1))
					{
						(DrawableItem&)_lastDotItem = (DrawableItem&)_lastScribbleItem;
						_lastDotItem.dtType = DrawableType::dtDot;
						pdrwi = &_lastDotItem;
					}
					else	// DrawableLine
					{
						(DrawableItem&)_lastLineItem = (DrawableItem&)_lastScribbleItem;
						_lastLineItem.dtType = DrawableType::dtLine;
						_lastLineItem.endPoint = _lastScribbleItem.points[1];
						pdrwi = &_lastLineItem;
					}
				}
				_history->AddDrawableItem(*pdrwi);
				update();
// DEBUG
				//std::ofstream alma;
				//alma.open("_lastScribbleItem.csv");
				//for (auto p : _lastScribbleItem.points)
				//	alma << p.x() << "," << p.y() << std::endl;
				//alma.close();
// End DEBUG
				//DEBUG_LOG(QString("Mouse release #2: _lastPoint: (%1,%2)\n__lastDrwanItem point size:%3").arg(_lastPointC.x()).arg(_lastPointC.y()).arg(_lastScribbleItem.points.size()))

				emit CanUndo(_history->CanUndo());
				emit CanRedo(_history->CanRedo());
			}
			else
#endif
				_RestoreCursor();
#ifndef _VIEWER
		}
#endif 
		_scribbling = false;
		_pendown = false;
		_drawStarted = false;
	}
#ifndef _VIEWER
	else if (_rubberBand)		// comes here when Ctrl + MyButtonPressed is followed by MyButtonrelease
	{
		// DEBUG
		//#if defined _DEBUG
		//        qDebug("Button Release: rubberband size:%d,%d", _rubberBand->size().width(), _rubberBand->size().height());
		//#endif
		// /DEBUG
		if (_rubberBand->geometry().width() > 10 && _rubberBand->geometry().height() > 10)
		{
			_rubberRect = _rubberBand->geometry();
			_history->CollectDrawablesInside(_rubberRect.translated(_topLeft));
			if (_history->SelectedSize() && !event->mods.testFlag(Qt::AltModifier))
			{
				_rubberBand->setGeometry(_history->BoundingRect().translated(-_topLeft).toRect());
				_rubberRect = _rubberBand->geometry(); // _history->BoundingRect().translated(-_topLeft);
			}
		}
		else if (event->mods.testFlag(Qt::ControlModifier))     // Ctrl + click: select image or near scribbles if any
		{
			QPointF p = event->pos + _topLeft;
			QRectF r = _history->SelectDrawablesUnder(p, event->mods.testFlag(Qt::ShiftModifier));
			if (r.isNull())
			{
				int i = _history->SelectTopmostImageUnder(p);
				if (i >= 0)
				{
					_rubberBand->setGeometry((*_history)[i]->Area().translated(-_topLeft).toRect());
					_rubberRect = _rubberBand->geometry();  // _history->BoundingRect().translated(-_topLeft);
				}
			}
			else    // scribbles were selected
			{
				_rubberBand->setGeometry(r.translated(-_topLeft).toRect());
				_rubberRect = _rubberBand->geometry();      // _history->BoundingRect().translated(-_topLeft);
			}
		}
		else
			HideRubberBand(true);
		event->accept();
	}
#endif
	_allowMouse = _allowPen = true;
}

void DrawArea::_DrawGrid(QPainter& painter)
{
	if (!_bGridOn)
		return;
	int x, y;
	if (_gridIsFixed)
	{
		x = _nGridSpacingX; y = _nGridSpacingY;
	}
	else
	{
		x = _nGridSpacingX - ((int)_topLeft.x() % _nGridSpacingX);
		y = _nGridSpacingY - ((int)_topLeft.y() % _nGridSpacingY);
	}

	painter.setPen(QPen(_gridColor, 2, Qt::SolidLine));
	for (; y <= height(); y += _nGridSpacingY)
		painter.drawLine(0, y, width(), y);
	for (; x <= width(); x += _nGridSpacingX)
		painter.drawLine(x, 0, x, height());
}

void DrawArea::_DrawPageGuides(QPainter& painter)
{
	if (!_bPageGuidesOn)
		return;

	int nxp = _topLeft.x() / _prdata.screenPageWidth,
		nyp = _topLeft.y() / _prdata.screenPageHeight,
		nx = (_topLeft.x() + width()) / _prdata.screenPageWidth,
		ny = (_topLeft.y() + height()) / _prdata.screenPageHeight,
		x = height(), y;

	painter.setPen(QPen(_pageGuideColor, 2, Qt::DotLine));
	if (nx - nxp > 0)     // x - guide
		for (x = nxp + 1 * _prdata.screenPageWidth - _topLeft.x(); x < width(); x += _prdata.screenPageWidth)
			painter.drawLine(x, 0, x, height());
	if (ny - nyp > 0)    // y - guide
		for (y = nyp + 1 * _prdata.screenPageHeight - _topLeft.y(); y < height(); y += _prdata.screenPageHeight)
			painter.drawLine(0, y, width(), y);
}


/*========================================================
 * TASK:    system paint event. Paints all layers
 * PARAMS:  event
 * GLOBALS:
 * RETURNS:
 * REMARKS: - logical layers:
 *                  top     sprite layer     ARGB
 *                          _pActCanvas      ARGB
 *                          page guides if shown
 *                          screenshots
 *                          grid        if shown
 *                  bottom  background image if any
 *          - paint these in bottom to top order on this widget
 *			- drawables are already painted on in-memory
 *				pixmap (_pActCanvas) and just copied here
 *          - event->rect() is widget relative
 *          - area to paint is clipped to event->rect()
 *              any other clipping is applied on top of this
 *-------------------------------------------------------*/
void DrawArea::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);             // show image on widget
	QRectF dirtyRect = event->rect();
	//    painter.fillRect(dirtyRect, _backgroundColor);             // already set

	// bottom layer: background with possible background image
	if (!_background.isNull())
		painter.drawImage(dirtyRect, _background, dirtyRect);
	if (_bGridOn)
		_DrawGrid(painter);

	if (_history)
	{
		// screenshots layer: images below user drawn lines
		QRectF r = dirtyRect.translated(_topLeft);              // screen -> absolute coord
		DrawableScreenShot* pimg = _history->FirstVisibleScreenShot(r);   // pimg intersects r
		while (pimg)                                            // image layer
		{
			QRectF intersectRect = pimg->AreaOnCanvas(_clippingRect);      // absolute
			pimg->Draw(&painter, _topLeft, intersectRect);
						//painter.drawPixmap(intersectRect.translated(-_topLeft), pimg->Image(), intersectRect.translated(-pimg->startPos));
			pimg = _history->NextVisibleScreenShot();
		}
	}
	// page guides layer:
	_DrawPageGuides(painter);

	// _pActCanvas layer: the scribbles
	painter.setClipRect(dirtyRect);
	painter.drawImage(dirtyRect, *_pActCanvas, dirtyRect);          // canvas layer

// sprite layer
	if (_pSprite && _pSprite->visible)
		painter.drawImage(dirtyRect.translated(_pSprite->topLeft), _pSprite->image, dirtyRect);  // sprite layer: dirtyRect: actual area below sprite 
}

void DrawArea::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);

	// DEBUG
	//    QSize s = geometry().size();
	// /DEBUG
	int h = height();
	int w = width();
	if (_limited && _topLeft.x() + w > _prdata.screenPageWidth)
		_ShiftOrigin(QPointF((_topLeft.x() + w - _prdata.screenPageWidth), 0));

	_SetCanvasAndClippingRect();

	if (width() > _pActCanvas->width() || height() > _pActCanvas->height())
	{
		int newWidth = qMax(w + 128, _pActCanvas->width());
		int newHeight = qMax(h + 128, _pActCanvas->height());
		_ResizeImage(_pActCanvas, QSize(newWidth, newHeight), true);
		_ResizeImage(_pOtherCanvas, QSize(newWidth, newHeight), true);
		if (_history)
			_history->SetClippingRect(QRectF(_topLeft, QSize(newWidth, newHeight)));

	}
	_Redraw();
}
#ifndef _VIEWER
/*=============================================================
 * TASK:    removes rubber band by hiding it and optionally delete
 *          it
 * PARAMS:  del: (default: false) if set the rubber band is deleted
 *           and set to null
 * GLOBALS:
 * RETURNS:
 * REMARKS: - when the rubberBand is hidden and not deleted,
 *              it can be reshown using _ReshowRubberBand()
 *------------------------------------------------------------*/
void DrawArea::HideRubberBand(bool del)
{
	if (_rubberBand && _rubberBand->isVisible())
	{
		_topLeftWhenRubber = _topLeft;
		_rubberBand->hide();
		if (del)
		{
			delete _rubberBand;
			_rubberBand = nullptr;
		}
		emit RubberBandSelection(false);
	}
}

/*=============================================================
 * TASK:    re show hidden rubberBand
 * PARAMS:
 * GLOBALS: _rubber_origin, _topLeftWhenRubber, _topLeft
 * RETURNS:
 * REMARKS: - only called if rubber Band was hidden
 *          - top left of rubberBand is relative to area top left
 *------------------------------------------------------------*/
void DrawArea::_ReshowRubberBand()
{
	if (_rubberBand && !_rubberBand->isVisible())
	{
		QPointF pt = _topLeftWhenRubber - _topLeft;  //  <0: viewport moved down/righ canvas muved up/left
		pt += _rubberBand->pos();                   // move rubber band
		_rubberBand->move(pt.toPoint());
		_topLeftWhenRubber = _topLeft;
		if ((_rubberBand->x() >= 0 || _rubberBand->x() + _rubberBand->width() < width()) &&
			(_rubberBand->y() >= 0 || _rubberBand->y() + _rubberBand->height() < height()))
		{
			_rubberBand->show();
			_rubberRect = _rubberBand->geometry();
		}

	}
}

/*========================================================
 * TASK:    starts drawing at last position
 * PARAMS:  NONE
 * GLOBALS: _spaceBarDown, _eraseMode, _lastScribbleItem,
 *          _actPenWidth, _actpenColor, _lastPointC, _topLeft
 * RETURNS:
 * REMARKS: - for erasers the stroke is added to all items 
 *				under the cursor 
 *-------------------------------------------------------*/
void DrawArea::_InitiateDrawingIngFromLastPos()
{
	if (_spaceBarDown)      // no drawing
		return;

	_lastScribbleItem.Clear();
	_actPenWidth = _penWidth;

	// DEBUG
	_lastScribbleItem.bSmoothDebug = _bSmoothDebug;
	// end DEBUG
	_lastScribbleItem.SetPenKind(_actPenKind);
//	_lastScribbleItem.SetPenColor();
	_lastScribbleItem.penWidth = _actPenWidth;
	_lastScribbleItem.startPos = _lastPointC + _topLeft;
	_lastScribbleItem.zOrder = _history->GetZorder(false);
	if (_lastPointC.x() >= 0)    // else no last point yet
		_lastScribbleItem.Add(_lastPointC + _topLeft, true, true);		// reset then add first new point
}

/*========================================================
 * TASK:    starts drawing at position set by the event
 *          saves first point in _firstPointC, _lastPointC
 *          and _lastScribbleItem
 * PARAMS:  event - mouse or tablet event
 * GLOBALS: _spaceBarDown, _eraseMode, _lastScribbleItem,
 *          _actPenWidth, _actpenColor, _lastPoint, _topLeft
 * RETURNS:
 * REMARKS: -
 *-------------------------------------------------------*/
void DrawArea::_InitiateDrawing(MyPointerEvent* event)
{
	_firstPointC = _lastPointC = event->pos;

	_InitiateDrawingIngFromLastPos();
}


/*=============================================================
 * TASK:    Adds scroll timer and start timing
 * PARAMS:
 * GLOBALS: _pScrollTimer
 * RETURNS:
 * REMARKS:
 *------------------------------------------------------------*/
void DrawArea::_AddScrollTimer()
{
	if (_pScrollTimer)  // already added
		return;

	_pScrollTimer = new QTimer(this);
	connect(_pScrollTimer, &QTimer::timeout, this, &DrawArea::_ScrollTimerSlot);
	_pScrollTimer->start(100ms);
}

/*=============================================================
 * TASK:    remove scroll timer
 * PARAMS:
 * GLOBALS:
 * RETURNS:
 * REMARKS:
 *------------------------------------------------------------*/
void DrawArea::_RemoveScrollTimer()
{
	delete _pScrollTimer;
	_pScrollTimer = nullptr;
}

/*=============================================================
 * TASK:    auto scrolls page in direction given by _scrollDir
 * PARAMS:
 * GLOBALS: _scrollDir
 * RETURNS:
 * REMARKS:
 *------------------------------------------------------------*/
void DrawArea::_ScrollTimerSlot()
{
	QPointF dr;
	for (int i = 0; i < 4; ++i)
	{
		switch (_scrollDir)
		{
		case _ScrollDirection::scrollNone: return;

		case _ScrollDirection::scrollUp:
			dr = { 0, 5 };
			break;
		case _ScrollDirection::scrollDown:
			dr = { 0, -5 };
			break;
		case _ScrollDirection::scrollLeft:
			dr = { 5, 0 };
			break;
		case _ScrollDirection::scrollRight:
			dr = { -5, 0 };
			break;
		}
		_ShiftOrigin(dr);
		_Redraw();
		_lastPointC -= dr;
	}
	//    qDebug("dr=%d:%d, _lastPointC=%d:%d", dr.x(),dr.y(), _lastPointC.x(), _lastPointC.y());
}

/*=============================================================
 * TASK:    sets up scroll direction and timer if the position
 *          is near to a widget edge when there is a sprite
 * PARAMS:  pos: position
 * GLOBALS: _pSprite, _limited, _prdata.screenPageWidth, _screenHeight
 *          _pTimer, _topLeft,_limited
 * RETURNS:
 * REMARKS:
 *------------------------------------------------------------*/
DrawArea::_ScrollDirection DrawArea::_AutoScrollDirection(QPointF pos)
{
	if (!_pSprite)
	{
		_RemoveScrollTimer();
		return _scrollDir = _ScrollDirection::scrollNone;
	}

	constexpr const int limit = 10;  // pixel

	_scrollDir = _ScrollDirection::scrollNone;

	if (pos.y() < limit)
		_scrollDir = _ScrollDirection::scrollDown;
	else if (pos.y() > height() - limit)
		_scrollDir = _ScrollDirection::scrollUp;
	else if (pos.x() < limit && _topLeft.x() > 0)
		_scrollDir = _ScrollDirection::scrollRight;
	else if (pos.x() > width() - limit && (!_limited || (_limited && _topLeft.x() + width() < _prdata.screenPageWidth)))
		_scrollDir = _ScrollDirection::scrollLeft;
	// DEBUG
	//const char *s;
	//switch (_scrollDir)
	//{
	//    case _ScrollDirection::scrollNone:
	//        s = "--";
	//        break;
	//    case _ScrollDirection::scrollUp:
	//        s = "Up";
	//        break;
	//    case _ScrollDirection::scrollDown:
	//        s = "Down";
	//        break;
	//    case _ScrollDirection::scrollLeft:
	//        s = "Left";
	//        break;
	//    case _ScrollDirection::scrollRight:
	//        s = "Right";
	//        break;
	//}
	//qDebug("dir:%s",s);
	// /DEBUG
	return _scrollDir;
}

/*========================================================
 * TASK:    Cretae rubber band and store uts parameter
 * PARAMS:
 * GLOBALS:
 * RETURNS:
 * REMARKS: -
 *-------------------------------------------------------*/
void DrawArea::_InitRubberBand(MyPointerEvent* event)
{
	_rubber_origin = event->pos;
	if (!_rubberBand)
		_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
	_rubberBand->setGeometry(QRectF(_rubber_origin, QSize()).toRect());
	_rubberBand->show();
	emit RubberBandSelection(true);
}



/*========================================================
 * TASK:    confines drawing in either horizontal or
 *              vertical afte drawing started
 *            changes in coordinate in one direction
 * PARAMS:  qpC - actual coordinate canvas relative
 * GLOBALS: _startsSet, _isHorizontal
 * RETURNS:
 * REMARKS: - if _startsSet and not horizontal then
 *              vertical
 *-------------------------------------------------------*/
void DrawArea::_ModifyIfSpecialDirection(QPointF& qpC)
{
	if (_drawStarted)
	{
		if (_isHorizontal)
			qpC.setY(_firstPointC.y());
		else
			qpC.setX(_firstPointC.x());
	}
}

void DrawArea::_SetLastPointPosition()
{
	HistoryItem* phi = _history->LastScribble();
	if (phi)
	{
		DrawableItem* pscri = phi->GetDrawable(true);
		if (pscri)
			_lastPointC = pscri->GetLastDrawnPoint();
	}
	else
		_lastPointC = _topLeft;
}

/*========================================================
 * TASK:    Determines whether movement should be
 *          constrained to be horizontal or vertical
 *          and if so does not allow drawing until
 *          the next point is at last 5 points from the first one
 *          allows drawing in any other case
 * PARAMS:  newEndPoint is canvas relative
 * GLOBALS:
 * RETURNS: true : if is allowed to save 'newEndPointC'
 *          false: constrain requested but we do not know
 *              yet in which direction
 * REMARKS: - Expects _lastScribbleItem to contain at least
 *              two points
 *          - without constraint does not modify newEndPoint
 *              and always returns true
 *-------------------------------------------------------*/
bool DrawArea::_CanSavePoint(QPointF& newEndPointC)   // endPoint relative to canvas, not _topLeft
{
	//DEBUG_LOG(QString("CanSavePoint #1: firstPointC:(%1,%2) newEndPointC=(%3,%4)").arg(_firstPointC.x()).arg(_firstPointC.y()).arg(newEndPointC.x()).arg(newEndPointC.y()))
	if (!_drawStarted && (_mods.testFlag(Qt::ShiftModifier) && (_pendown || _scribbling)))
	{
		int x0 = _firstPointC.x(),    // relative to canvas
			y0 = _firstPointC.y(),
			x = newEndPointC.x(),
			y = newEndPointC.y(),
			dx = abs(x - x0),
			dy = abs(y0 - y);

		//DEBUG_LOG(QString("CanSavePoint #2: (dx, dy) = (%1,%2)").arg(dx).arg(dy))
		if (dx < 4 && dy < 4)
			return false;

		if (dx < 10 && dy < 10)
		{
			if (dx > dy)
				_isHorizontal = true;
			else if (dy > dx)
				_isHorizontal = false;
			_drawStarted = true;
		// DEBUG
		// qDebug("_drawStarted? %s, shift? %s, horiz? %s dx:%d,dy:%d", _drawStarted ?  "yes":"no",_mods.testFlag(Qt::ShiftModifier) ? "yes":"no", _isHorizontal ? "yes":"no",dx,dy);
		// /DEBUG
			// replace most points keeping only the first, and the last

			_ModifyIfSpecialDirection(_lastPointC);
			_ModifyIfSpecialDirection(newEndPointC);
		}
		//DEBUG_LOG(QString("CanSavePOint #3: lastPointC:(%1,%2) endPointC=(%3,%4)").arg(_lastPointC.x()).arg(_lastPointC.y()).arg(newEndPointC.x()).arg(newEndPointC.y()))
	}
	// DEBUG
	//qDebug("last: (%d, %d), new:(%d,%d)", _lastPointC.x(), _lastPointC.y(), newEndPointC.x(), newEndPointC.y());
	// /DEBUG            
	return true;
}

QPointF DrawArea::_CorrectForDirection(QPointF& newpC)     // newpC canvas relative
{
	if (_drawStarted)
	{
		if (_isHorizontal)
			newpC.setY(_firstPointC.y());
		else
			newpC.setX(_firstPointC.x());
	}

	return newpC;
}
#endif
void DrawArea::_SetTopLeftFromItem(HistoryItem* phi)
{
	int l = _topLeft.x(),
		t = _topLeft.y();
	if (phi)
	{
		QRectF rect = phi->Area();
		if (!rect.isNull() && rect.isValid() && !_canvasRect.intersects(rect)) // try to put at the middle of the screen
		{
			if (rect.x() < l || rect.x() > l + _canvasRect.width())
				l = rect.x() - (_canvasRect.width() - rect.width()) / 2;
			if (rect.y() < t || rect.y() > t + _canvasRect.height())
				t = rect.y() - (_canvasRect.height() - rect.height()) / 2;

			if (l != _topLeft.x() || t != _topLeft.y())
			{
				_SetOrigin(QPointF(l, t));
			}
		}
	}
	else
		_SetOrigin(QPointF(0, 0));

}

int DrawArea::CollectDrawables(IntVector& hv)
{
	return _history->GetDrawablesInside(_clippingRect, hv);

	//if (_history->SetFirstItemToDraw() < 0)  // returns index of first visible item after the last clear screen
	//    return hv;                          // so when no such iteme exists we're done with empty list

	//HistoryItem* phi;
	//while ((phi = _history->GetOneStep()))   // add next not hidden item to vector in y order
	//    hv.push_back(phi);                  
	//                                        // but draw them in z-order
	//std::sort(hv.begin(), hv.end(), [](HistoryItem* pl, HistoryItem* pr) {return pl->ZOrder() < pr->ZOrder(); });
	//return hv;
}

/*========================================================
 * TASK:    Draw line from '_lastPointC' to 'endPointC'
 *          using _DrawLineTo
 * PARAMS:  endpointC : canvas relative coordinate
 * GLOBALS:
 * RETURNS: true if the line was drawn and you must save _lastPointC
 *          false otherwise
 * REMARKS: - save _lastPointC after calling this function
 *              during user drawing
 *          - sets the new actual canvas relative position
 *            in '_lastPointC
 *          - does modify _endPointC if the shift key
 *              is pressed when drawing
 *          - when _shiftKeyDown
 *              does not draw the point until a direction
 *                  was established
  *-------------------------------------------------------*/
bool DrawArea::_DrawFreehandLineTo(QPointF endPointC)
{
	bool result = true;
#ifndef _VIEWER
	if ((result = _CanSavePoint(endPointC)))     // i.e. must save point
	{
		_CorrectForDirection(endPointC); // when _drawStarted leaves only one coord moving
		_DrawLineTo(endPointC);
	}
#endif
	return result;
}

/*========================================================
 * TASK:    Draw line from '_lastPointC' to 'endPointC'
 *			onto _pActCanvas
 * PARAMS:  endpointC : canvas relative coordinate
 * GLOBALS:	_pActCanvas
 * RETURNS: if the line was drawn and you must save _lastPointC
 *          false otherwise
 * REMARKS: - save _lastPointC after calling this function
 *              during user drawing
 *          - sets the new actual canvas relative position
 *            in '_lastPointC
 *          - does modify _endPointC if the shift key
 *              is pressed when drawing
 *          - when _shiftKeyDown
 *              does not draw the point until a direction
 *                  was established
 *			- changes will apear on the screen only in drawEvent
 *-------------------------------------------------------*/
void DrawArea::_DrawLineTo(QPointF endPointC)     // 'endPointC' canvas relative 
{
	// DEBUG
	// qDebug("_DrawLineTo: last:(%4.1f, %4.1f) new:(%4.1f, %4.1f)"), _lastPointC.x(), _lastPointC.y(), endPointC.x(), endPointC.y();
	// end DEBUG

	QPainter painter(_pActCanvas);
	QPen pen = QPen(_PenColor(), (_pencilmode ? 1 : _actPenWidth), Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin);
	painter.setPen(pen);
	if (_erasemode && !_debugmode)
		painter.setCompositionMode(QPainter::CompositionMode_Clear);
	//else			Default mode is SourceOver
	//	painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
	painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);

	QPointF ep = endPointC,
		lp = _lastPointC;

	if (ep == lp)
		painter.drawPoint(lp);
	else if (_lastPointC.x() >= 0)
		painter.drawLine(lp, ep);    //? better?
	//painter.drawLine(_lastPointC, endPointC);
	int rad = (_actPenWidth / 2) + 2;
	update(QRectF(_lastPointC, endPointC + (endPointC == _lastPointC ? QPointF(1, 1) : QPointF(0, 0))).normalized()
		.adjusted(-rad, -rad, +rad, +rad).toRect());

	_lastPointC = endPointC;
}


/*========================================================
 * TASK:    draws the polyline stored in drawnable on
 *          '_pActCanvas'
 * PARAMS:  pscrbl - valid pointer to a ScribbleAble item
 * GLOBALS: _pActCanvas,_actPenKind, _actPenWidth, _erasemode
 *          _clippingRect, _lastPointC, _topLeft
 * RETURNS:
 * REMARKS: - no errro checking on pscrbl
 *-------------------------------------------------------*/
void DrawArea::_DrawAllPoints(DrawableItem* pscrbl)
{
	QPainter* painter = _GetPainter(_pActCanvas);

	pscrbl->Draw(painter, _topLeft);

	_lastPointC = pscrbl->GetLastDrawnPoint();
}

void DrawArea::_ResizeImage(QImage* image, const QSize& newSize, bool isTransparent)
{
	if (image->size() == newSize)
		return;

	QImage newImage(newSize, QImage::Format_ARGB32);

	QColor color = isTransparent ? Qt::transparent : _backgroundColor;
	newImage.fill(color);
	if (!image->isNull())
	{
		QPainter painter(&newImage);
		painter.setCompositionMode(QPainter::CompositionMode_Source);
		painter.drawImage(QPointF(0, 0), *image);
	}
	*image = newImage;
}

void DrawArea::ClearHistory()
{
	_history->ClearUndo();

	emit CanUndo(_history->CanUndo());
	emit CanRedo(_history->CanRedo());
}

/*========================================================
 * TASK:    called when primary screen changed
 * PARAMS:  new screen
 * GLOBALS:
 * RETURNS:
 * REMARKS: ???
 *-------------------------------------------------------*/
void DrawArea::SlotForPrimaryScreenChanged(QScreen* ps)
{
	// _history->SetBandHeight(ps->geometry().height());
}

void DrawArea::SlotForGridSpacingChanged(int spacing)
{
	_nGridSpacingX = _nGridSpacingY = spacing;
	_Redraw();
}

bool DrawArea::PageSetup(PageSetupDialog::WhatToDo what)      // public slot
{
	PageSetupDialog* pageDlg = new PageSetupDialog(this, _prdata.printerName, what);
	bool res = false;
	bool forPdf = what == PageSetupDialog::wtdExportPdf;
	int oldwidth = _prdata.screenPageWidth;
	if (pageDlg->exec())
	{
		if (!forPdf)
		{
			_prdata.printerName = pageDlg->actPrinterName;
			_bPageSetupUsed = !_prdata.printerName.isEmpty();
		}
		else
			_openPDFInViewerAfterPrint = pageDlg->flags & pfOpenPDFInViewer;
#define SQUARE(a)  (a*a)

		if (!pageDlg->useResInd)
			_prdata.screenPageWidth = pageDlg->horizPixels;
		if (_screenHeight > 0)           // -1 if no predefined resolution
		{
			static float fact[] = { 1.0, 1.0 / 2.54, 1.0 / 25.4 };   // inch, cm, mm
			float cosine = (float)_prdata.screenPageWidth / (float)std::sqrt(SQUARE(_prdata.screenPageWidth) + SQUARE(_screenHeight));
			_ppi = (float)_prdata.screenPageWidth / (pageDlg->screenDiagonal * fact[pageDlg->unitIndex] * cosine);
		}
		else
			_ppi = 96;

#undef SQUARE

		QSize ss;
		_prdata.screenPageWidth = pageDlg->GetScreenSize(ss);

		_prdata.bExportPdf = forPdf;
		_prdata.flags = pageDlg->flags;

		qreal w, h;

		if (_prdata.flags & pfLandscape)
		{
			w = pageDlg->pdfHeight;
			h = pageDlg->pdfWidth;
		}
		else
		{
			w = pageDlg->pdfWidth;
			h = pageDlg->pdfHeight;
		}

		_prdata.paperId = PageId(pageDlg->pdfIndex);	// in pagesetup.cpp

		_prdata.SetPrintArea(QRectF(0, 0, w, h), false); 	// do not re-calculate screen area for page
		_prdata.SetDpi(resos[pageDlg->pdfDpi], false);
		_prdata.SetMargins(pageDlg->hMargin, pageDlg->vMargin, pageDlg->gutterMargin, true); // and re-calculate screen area

		_bPageSetupValid = true;

		res = true;
	}
	_bPageSetupValid = res;
	delete pageDlg;

	if (oldwidth != _prdata.screenPageWidth)
		_Redraw();
	return res;
}


bool DrawArea::_NoPrintProblems()
{
	bool res = false;
	switch (_printer->Status())
	{
	case MyPrinter::rsAllocationError:
		QMessageBox::critical(this, tr("FalconBoard - Error"), tr("Can't Allocate Resources\nNot enough memory?"));
		break;
	case MyPrinter::rsInvalidPrinter:
		QMessageBox::critical(this, tr("FalconBoard - Error"), tr("Can't find printer"));
		break;
	case MyPrinter::rsPrintError:
		QMessageBox::warning(this, tr("FalconBoard - Warning"), tr("Print error"));
		break;
	case MyPrinter::rsCancelled:
		QMessageBox::warning(this, tr("FalconBoard - Warning"), tr("Print cancelled"));
		break;
	default:
		res = true;
	}
	return res;
}


/*========================================================
 * TASK:    Print data to printer or to PDF
 * PARAMS:  name: file name for PDF
 *          pdir: pointer to
 * GLOBALS:
 * RETURNS:
 * REMARKS: - if pdir then name does not contain a directory
 *              and *pdir will be set to last used directory
 *-------------------------------------------------------*/
void DrawArea::Print(QString name, QString* pdir)
{
	if (pdir)
	{
		_prdata.pdir = pdir;
		_prdata.directory = *pdir;
	}
	else
		pdir = &_prdata.directory;
	_prdata.docName = name;
	int pos = name.lastIndexOf('/');
	if (pos >= 0)
	{
		_prdata.directory = name.left(pos + 1);
		_prdata.docName = name.mid(pos + 1);
	}
	else
		_prdata.docName = name;

	if (!_prdata.bExportPdf && !_bPageSetupUsed)
		PageSetup(PageSetupDialog::wtdPageSetup);

	if (_bPageSetupValid)
	{
		_prdata.topLeftActPage = _topLeft;
		_prdata.backgroundColor = _backgroundColor;
		_prdata.gridColor = (_prdata.flags & pfWhiteBackground) ? "#d0d0d0" : _gridColor;
		_prdata.pBackgroundImage = &_background;
		_prdata.nGridSpacingX = _nGridSpacingX;
		_prdata.nGridSpacingY = _nGridSpacingY;
		_prdata.gridIsFixed = _gridIsFixed;
		_prdata.openPDFInViewerAfterPrint = _openPDFInViewerAfterPrint;

		_printer = new MyPrinter(this, _history, _prdata);     // _prdata may be modified!
		if (_NoPrintProblems())   // only when not Ok
		{
			_printer->Print();
			if (_NoPrintProblems() && _openPDFInViewerAfterPrint)		// prints errors if any
			{
				QUrl url = QUrl::fromLocalFile(_prdata.fileName);
				QDesktopServices::openUrl(url);
			}
		}
	}
	_prdata.bExportPdf = false;
}

void DrawArea::ExportPdf(QString fileName, QString& directory)
{
	if (PageSetup(PageSetupDialog::wtdExportPdf))                // else cancelled
		Print(fileName, &directory);
}

void DrawArea::_Redraw(bool clear)
{

	if (!_mustRedrawArea)
	{
		_redrawPending = true;
		return;
	}
	if (!_history)
	{
		_ClearCanvas();
		return;
	}

	_redrawPending = false;
	int savewidth = _penWidth;
	FalconPenKind savekind = _actPenKind;
	bool saveEraseMode = _erasemode;

	IntVector /*HistoryItemVector*/ forPage;
	CollectDrawables(forPage);  // in clipping area
	if (clear)
		_ClearCanvas();
	for (auto phi : forPage)
		_ReplotDrawableItem(_history->Drawable(phi));

	_actPenKind = savekind;
	_penWidth = savewidth;
	_erasemode = saveEraseMode;
}

QColor DrawArea::_PenColor()

{
	static FalconPenKind _prevKind = penNone;
	static QColor color;
	if (_actPenKind == _prevKind)
	{
		if (_actPenKind != penBlack)
			return color;
		return _darkMode ? QColor(Qt::white) : QColor(Qt::black);
	}

	_prevKind = _actPenKind;

	switch (_actPenKind)
	{
	case penBlack: return  color = _darkMode ? QColor(Qt::white) : QColor(Qt::black);
	default:
		return color = drawColors[_actPenKind];
	}
}

void DrawArea::_SaveCursorAndReplaceItWith(QCursor newCursor)
{
	if (!_cursorSaved)
	{
		_cursorSaved = true;
		_savedCursor = cursor();
		setCursor(newCursor);
	}
}

void DrawArea::_RestoreCursor()
{
	if (_cursorSaved)
	{
		setCursor(_savedCursor);
		_cursorSaved = false;
	}
}

QPainter *DrawArea::_GetPainter(QImage *pCanvas)
{
	QPainter *painter = new QPainter(pCanvas);
	QPen pen = QPen(_PenColor(), (_pencilmode ? 1 : _actPenWidth), Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin);
	painter->setPen(pen);
	if (_erasemode && !_debugmode)
		painter->setCompositionMode(QPainter::CompositionMode_Clear);
	else
		painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
	painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);

	QRectF rect = _clippingRect.translated(-_topLeft); // _pActCanvas relative
	painter->setClipRect(rect);
	return painter;
}

/*========================================================
 * TASK:    plots visible 'Drawable's onto _pActCanvas
 *              if it intersects _clippingRect
 * PARAMS:  phi - possibly null pointer to a HistoryItem
 * GLOBALS: _clippingRect
 * RETURNS:
 * REMARKS: - reason: double buffering: draw on one canvas display the other
 *			- images are not painted here, not even when they
 *             are part of a pasted stack of items
 *				They are painted in paintEvent
 *-------------------------------------------------------*/
bool DrawArea::_ReplotDrawableItem(DrawableItem* pdrwi)
{
	if (!pdrwi || pdrwi->IsImage() || !pdrwi->IsVisible() ||!pdrwi->Area().intersects(_clippingRect))
		return false;

	QPainter painter(_pActCanvas);
	pdrwi->Draw(&painter, _topLeft, _clippingRect);

	return true;
}

void DrawArea::_SetCanvasAndClippingRect()
{
	_clippingRect = _canvasRect = QRectF(0, 0, width(), height()).translated(_topLeft);     // 0,0 relative rectangle
}

#ifndef _VIEWER

void DrawArea::Undo()               // must draw again all underlying drawables
{
	if (_history->CanUndo())
	{
		HideRubberBand(true);

		HistoryItem* phi = _history->Undo();
		if (_history->CanUndo())
			_SetTopLeftFromItem(phi);

		_Redraw();	// _Canvas is cleared inside _Redraw
		_clippingRect = _canvasRect;
		_SetLastPointPosition();

		emit CanRedo(_history->CanRedo());
	}
	emit CanUndo(_history->CanUndo());
}

void DrawArea::Redo()       // need only to draw undone items, need not redraw everything
{                           // unless top left or items color changed
	HistoryItem* phi = _history->Redo();
	if (!phi)
		return;

	HideRubberBand(true);

	_SetTopLeftFromItem(phi);
	// ??    _clippingRect = phi->Area();

	_Redraw();

	_clippingRect = _canvasRect;

	_SetLastPointPosition();

	emit CanUndo(_history->CanUndo());
	emit CanRedo(_history->CanRedo());
}
void DrawArea::ChangePenColorSlot(int key)
{
	ChangePenColorByKeyboard(key);
}

#endif
void DrawArea::SetCursor(DrawCursorShape cs)
{
	_erasemode = false;
	if (_spaceBarDown && (_scribbling || _pendown)) // do not set the cursor when space bar is pressed
		return;

	switch (cs)
	{
	case csArrow: setCursor(Qt::ArrowCursor); break;
	case csOHand: setCursor(Qt::OpenHandCursor); break;
	case csCHand: setCursor(Qt::ClosedHandCursor); break;
	case csPen:   setCursor(penCursors[_actPenKind]); break;
#ifndef _VIEWER
	case csEraser: setCursor(penCursors[penEraser]); 
		_erasemode = true; 
		HideRubberBand(true); 
		break;
#endif
	default:break;
	}
}


void DrawArea::SetGridOn(bool on, bool fixed)
{
	_gridIsFixed = fixed;
	_bGridOn = on;
	_Redraw();
}

void DrawArea::SetPageGuidesOn(bool on)
{
	_bPageGuidesOn = on;
	_Redraw();
}

void DrawArea::_SetOrigin(QPointF o)
{
#ifndef _VIEWER
	HideRubberBand();  // and store _topLeft into _topLeftWhenRubber
#endif
	_topLeft = o;
	_canvasRect.moveTo(_topLeft);
	_clippingRect = _canvasRect;
	_history->SetClippingRect(_canvasRect);

#ifndef _VIEWER
	_ShowCoordinates(QPointF());
#endif
}


/*========================================================
 * TASK:    set new top left coordinate of current viewport
 *          by shifting it with 'delta' pixels
 * PARAMS:  delta: pixels to change _topLeft
 * GLOBALS: _topLeft
 * RETURNS:
 * REMARKS: - delta.x() > 0 - move viewport right
 *          - delta.y() > 0 - move viewport down
 *-------------------------------------------------------*/
void DrawArea::_ShiftOrigin(QPointF delta)
{
	QPointF o = _topLeft;       // origin of screen top left relative to "paper"

	o += delta;                // calculate new screen origin

	if (o.x() < 0)
		o.setX(0);

	if (delta.x() > 0 && _limited && o.x() + width() >= _prdata.screenPageWidth)
		o.setX(_topLeft.x());

	if (o.y() < 0)
		o.setY(0);

	_SetOrigin(o);
}
// 

/*=============================================================
 * TASK:    shifts rectangular part of _pActCanvas to
 *          canvas _pOtherCanvas then exchanges canvases
 * PARAMS:  delta value in pixels
 * GLOBALS: _pActCanvas, _pOtherCanvas
 * RETURNS: nothing The two clipping areas will be filled
 * REMARKS: - both canvases have same size and other attributes
 *          - using memcpy, so the horizontal pixel position
 *            uses units of bytes/pixel (pixelSizeInBytes)
 *------------------------------------------------------------*/
void DrawArea::_ShiftRectangle(QPointF delta, QRectF& clip1, QRectF& clip2)
{
	_pOtherCanvas->fill(Qt::transparent);

	// column and row values on canvases in QImage's 'data' buffer
	// column values are multiplied with the correct bitsPerPixel units
	int srcSRow,     // source start on _pActCanvas at this row
		srcSCol,     // and in this column
		dstRow,      // destination start on _pOtherCanvas
		dstCol;

	int pixelSizeInBytes = _canvas1.bytesPerLine() / _canvas1.width();

	int dx = qAbs(delta.x()), dy = qAbs(delta.y());

	int w = width(),
		h = height();
	int blockWidth = (w - dx) * pixelSizeInBytes,
		blockHeight = (h - dy);

	clip2 = clip1 = QRectF();

	if (dx || dy)
	{
		if ((delta.y() > 0 && delta.x() >= 0) || (delta.x() > 0 && delta.y() >= 0))
		{
			srcSRow = dy;
			srcSCol = dx;
			dstRow = 0;
			dstCol = 0;
			clip1 = QRectF(0, h - dy, w - dx, dy);
			clip2 = QRectF(w - dx, 0, dx, h - dy);
		}
		else if ((delta.y() < 0 && delta.x() <= 0) || (delta.x() < 0 && delta.y() <= 0))
		{
			srcSRow = 0;
			srcSCol = 0;
			dstRow = dy;
			dstCol = dx;
			clip1 = QRectF(0, 0, w, dy);
			clip2 = QRectF(0, dy, dx, h - dy);
		}
		else if (delta.x() > 0 && delta.y() < 0)
		{
			srcSRow = 0;
			srcSCol = dx;
			dstRow = dy;
			dstCol = 0;
			clip1 = QRectF(0, 0, w, dy);
			clip2 = QRectF(w - dx, dy, dx, h - dy);
		}
		else // if (delta.x() < 0 && delta.y() > 0)
		{
			srcSRow = dy;
			srcSCol = 0;
			dstRow = 0;
			dstCol = dx;
			clip1 = QRectF(dx, h - dy, w, dy);
			clip2 = QRectF(0, 0, dx, h - dy);
		}
		srcSCol *= pixelSizeInBytes;
		dstCol *= pixelSizeInBytes;

		for (int i = 0; i < blockHeight; ++srcSRow, ++dstRow, ++i)
			memcpy(_pOtherCanvas->scanLine(dstRow) + dstCol, _pActCanvas->scanLine(srcSRow) + srcSCol, blockWidth);
	}
	std::swap(_pActCanvas, _pOtherCanvas);
	clip1.translate(_topLeft);
	clip2.translate(_topLeft);
}

/*========================================================
 * TASK:    scrolls visible area in a given direction
 * PARAMS:  delta: vector to move the area
 *          smooth: smoth scrolling by only drawing the
 *               changed bits
 * GLOBALS: _limited, _topLeft, _pActCanvas
 * RETURNS:
 * REMARKS: - smooth == true must only used when one of
 *              delta.x() or delta.y() is zero
 *          - delta.x() > 0 - moves viewport right (diplayed left)
 *            delta.y() > 0 - moves viewport down (displayed up)
 *-------------------------------------------------------*/
void DrawArea::_ShiftAndDisplayBy(QPointF delta, bool smooth)    // delta changes _topLeft, delta.x < 0 scroll right, delta.y < 0 scroll 
{
	if (_topLeft.y() + delta.y() < 0)
		delta.setY(-_topLeft.y());
	if (_topLeft.x() + delta.x() < 0)
		delta.setX(-_topLeft.x());
	if (delta.x() > 0 && _limited && delta.x() + width() >= _prdata.screenPageWidth)
		delta.setX(_prdata.screenPageWidth - width());

	if (delta.isNull())
		return;      // nothing to do

#if 1
	int dx = qAbs(delta.x()), dy = qAbs(delta.y());
	if (/*smooth && */ (dx <= _prdata.screenPageWidth / 10) && (dy <= _screenHeight / 10))
	{                      // use smooth transform only for up/down/left/right never for pgUp, etc

		QRectF clip1, clip2;

		_ShiftOrigin(delta);
		_ShiftRectangle(delta, clip1, clip2);           // shift original content
														// then draw to actual canvas
		if (clip1.isValid())
		{
			_clippingRect = clip1;
			_Redraw(false);
		}
		if (clip2.isValid())
		{
			_clippingRect = clip2;
			_Redraw(false);
		}
		_clippingRect = _canvasRect;
		update();

	}
#else 
	QPointF pt0 = _topLeft;
	_ShiftOrigin(delta);
	delta = _topLeft - pt0;
	if (smooth)
	{
		std::swap(_pActCanvas, _pOtherCanvas);
		_pActCanvas->fill(Qt::transparent);     // transparent
		QPainter painter(_pActCanvas);
		QRectF rectSrc, rectDst,                    // canvas relatice coordinates
			rectRe;                              // _topLeft relative  
		int w = width(),
			h = height();
		rectSrc = _canvasRect.translated(-_topLeft - delta);  // relative to viewport
		rectSrc.adjust(0, 0, -qAbs(delta.x()), -qAbs(delta.y()));
		rectDst = rectSrc;
		rectDst.moveTo(0, 0);

		painter.drawImage(rectDst, *_pOtherCanvas, rectSrc);   // move area
		// then plot newly visible areas


		// then plot newly visible areas
		if (delta.x() == 0.0)
		{
			if (delta.y() > 0)      // viewport up
				rectRe = QRectF(0, 0, w, delta.y());
			else                    // viewport down
				rectRe = QRectF(0, h + delta.y(), w, -delta.y());
		}
		else    // delta.y() is 0.0
		{
			if (delta.x() > 0)      // viewport left
				rectRe = QRectF(w - delta.x(), 0, delta.x(), h);
			else                    // viewport right
				rectRe = QRectF(0, 0, -delta.x(), h);
		}

		//        _ShiftOrigin(delta);

		rectRe.translate(_topLeft);
		_clippingRect = rectRe;
		_history->SetClippingRect(rectRe);
		_Redraw(false);
		update();
		_clippingRect = _canvasRect;
		_history->SetClippingRect(_canvasRect);
	}
#endif
	else
	{
		_ShiftOrigin(delta);
		_Redraw();
	}
}
void DrawArea::_PageUp()
{
	QPointF pt(0, -geometry().height() / 2);
	_ShiftAndDisplayBy(pt);
}
void DrawArea::_PageDown()
{
	QPointF pt(0, geometry().height() / 2);
	_ShiftAndDisplayBy(pt);
}
void DrawArea::_Home(bool toTop)
{
	QPointF pt = -_topLeft;

	if (!toTop)
		pt.setY(0);   // do not move in y direction
	_ShiftAndDisplayBy(pt);
}
void DrawArea::_End(bool toBottom)
{
	if (toBottom)
	{
		QSize siz = geometry().size();
		_topLeft = _history->BottomRightLimit(siz);
	}
	else  // just go end of rightmost scribble in actual viewport
	{
		int x = _history->RightMostInBand(_canvasRect) - width();
		if (x < 0)
			x = 0;
		x += 20;	// leave empty space to the right
		_topLeft.setX(x);
		_Redraw();
	}

	_SetOrigin(_topLeft);
	_Redraw();
}
void DrawArea::_Up(int amount)
{
	QPointF pt(0, -amount);
	_ShiftAndDisplayBy(pt, false);
}
void DrawArea::_Down(int amount)
{
	QPointF pt(0, amount);
	_ShiftAndDisplayBy(pt, false);
}
void DrawArea::_Left(int amount)
{
	QPointF pt(-amount, 0);
	_ShiftAndDisplayBy(pt, false);
}
void DrawArea::_Right(int amount)
{
	QPointF pt(amount, 0);
	_ShiftAndDisplayBy(pt, false);
}

#ifndef _VIEWER
void DrawArea::_ShowCoordinates(const QPointF& qp)
{
	static QPointF prevPoint = QPointF(0, 0);
	QPointF qpt;
	if (qp.isNull())
		qpt = prevPoint;
	else
		qpt = prevPoint = qp;

	_lastCursorPos = qpt;

	qpt += _topLeft;

	_actMousePos = qpt;

	QString qs;
	int pg = int((_topLeft.y() + qp.y()) / _prdata.screenPageHeight) + 1;
	if (_rubberBand)
	{
		QRectF r = _rubberBand->geometry();
		qs = QString(tr("   Page:%1, Left:%2, Top:%3 | Pen: x:%4, y:%5 | selection x:%6 y: %7, width: %8, height: %9")).
			arg(pg).arg(_topLeft.x()).arg(_topLeft.y()).arg(qpt.x()).arg(qpt.y()).
			arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
	}
	else
		qs = QString(tr("   Page:%1, Left:%2, Top:%3 | Pen: x:%4, y:%5 ")).arg(pg).arg(_topLeft.x()).arg(_topLeft.y()).arg(qpt.x()).arg(qpt.y());
	emit TextToToolbar(qs);
}

// ****************************** Sprite ******************************

/*========================================================
 * TASK:    Create a new sprite using lists of selected
 *          items and images
 * PARAMS:  pos: event position relative to _topLeft
 *          rectOnScreen: area on screen (from _rubberRect) relative to _topLeft
 *          deleted: was items deleted at original position?
 *                  false: no, just copied to sprite
 *          setVisible: should we display the sprite? (default: true)
 * GLOBALS: _topLeft, _pSprite
 * RETURNS: _pSprite
 * REMARKS: - _nSelectedList is used to create the other lists
 *            which leaves previously copied items intact
 *              so paste operation still pastes the non sprite
 *              data
 *			- sprite's _items have already rectOnScreen top left
 *				relative coordinates
 *-------------------------------------------------------*/
Sprite* DrawArea::_CreateSprite(QPointF pos, QRectF& rectOnScreen, bool deleted, bool setVisible)
{
	_pSprite = new Sprite(_history);        // this copies the selected items into lists
	return _PrepareSprite(_pSprite, pos, rectOnScreen, deleted, setVisible);
}

Sprite* DrawArea::_PrepareSprite(Sprite* pSprite, QPointF cursorPos, QRectF rectOnScreen, bool deleted, bool setVisible)
{
	// DEBUG
	// qDebug("rect = (x:%d,y:%d,w:%d,h:%d)", rect.x(), rect.y(), rect.width(), rect.height());
	// /DEBUG
	pSprite->visible = setVisible;
	pSprite->itemsDeleted = deleted;       // signal if element(s) was(were) deleted before moving
	pSprite->topLeft = rectOnScreen.topLeft() - QPoint(pSprite->margin, pSprite->margin);     // sprite top left
	pSprite->dp = cursorPos - pSprite->topLeft; // cursor position rel. to sprite
	pSprite->rect.adjust(0, 0, 2*pSprite->margin, 2*pSprite->margin); // increase size to make space for dashed border
	pSprite->image = QImage(pSprite->rect.width(), pSprite->rect.height(), QImage::Format_ARGB32);
	pSprite->image.fill(Qt::transparent);     // transparent

	// save color and line width
	FalconPenKind pk = _actPenKind;
	int pw = _actPenWidth;
	bool em = _erasemode;
	_erasemode = false;
	QPainter *painter = _GetPainter(&pSprite->image);

	int ix, siz = pSprite->drawables.Size();
	DrawableItem* pdrwi;
	QRectF sr, tr;   // source & target
	for (ix = 0; ix < siz; ++ix )	// first draw the screenshots items are in zorder already
	{
		pdrwi = pSprite->drawables[ix];
		if (pdrwi->dtType != DrawableType::dtScreenShot)   // screenshots are below others
			break;

		pdrwi = pSprite->drawables[ix];
		DrawableScreenShot* pSs = (DrawableScreenShot*)pdrwi;
		pSs->Draw(painter, QPointF(0, 0), pSprite->rect);
	}
	for (; ix < siz; ++ix )	// then the other Drawables
	{
		pdrwi = pSprite->drawables[ix];
		_actPenKind = pdrwi->PenKind();
		_actPenWidth = pdrwi->penWidth;
		pdrwi->Draw(painter, QPointF(0,0), pSprite->rect);
	}

	// create border to see the rectangle
	painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
	painter->setPen(QPen((_darkMode ? Qt::white : Qt::black), 2, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
	painter->drawRect(QRectF(QPoint(pSprite->margin/2.0, pSprite->margin / 2.0), pSprite->rect.size()) );
	//painter->drawLine(0, 0, pSprite->rect.width(), 0);
	//painter->drawLine(pSprite->rect.width(), 0, pSprite->rect.width(), pSprite->rect.height());
	//painter->drawLine(pSprite->rect.width(), pSprite->rect.height(), 0, pSprite->rect.height());
	//painter->drawLine(0, pSprite->rect.height(), 0, 0);

	delete painter;

	// restore data
	_actPenKind = pk;
	_actPenWidth = pw;
	_erasemode = em;

	return pSprite;
}

Sprite* DrawArea::_SpriteFromLists()
{
	Sprite* pSprite = new Sprite(_history, historyList.CopiedRect(), _history->SelectedItemsList() );
	return _PrepareSprite(pSprite, _lastCursorPos, historyList.CopiedRect().translated(_lastCursorPos), false, true);
}

void DrawArea::_MoveSprite(QPointF dr)
{
	//        QRectF updateRect = _pSprite->rect.translated(_pSprite->topLeft);      // original rectangle
	_pSprite->topLeft += dr;
	if (_pSprite->topLeft.y() < 0)
		_pSprite->topLeft.setY(0);
	if (_pSprite->topLeft.x() < 0)
		_pSprite->topLeft.setX(0);
	//        updateRect = updateRect.united(_pSprite->rect.translated(_pSprite->topLeft)); // + new rectangle
//            update(updateRect);
	update();
	//            QApplication::processEvents();
}

/*========================================================
 * TASK:    pastes images and scribbles on screen
 * PARAMS:  NONE
 * GLOBALS: _history, _pSprite
 * RETURNS: none
 * REMARKS: - in this case on top of history list _items is a
 *            HistorydeleteItem, which is moved above
 *              the HistoryPasteItemBottom mark item
 *-------------------------------------------------------*/
void DrawArea::_PasteSprite()
{
	Sprite* ps = _pSprite;
	_pSprite = nullptr;             // so the next paint event finds no sprite
	_history->AddPastedItems(ps->topLeft + _topLeft, ps);    // add at window relative position: top left = (0,0)
	QRectF updateRect = ps->rect.translated(ps->topLeft).toRect();    // original rectangle
	update(updateRect.toRect());
	_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
	int m = ps->margin;
	_rubberRect = updateRect.adjusted(m,m,-m,-m);
	_rubberBand->setGeometry(_rubberRect.toRect());
	_rubberBand->show();
	delete ps;

	if (_history)
		_history->CollectPasted(_rubberRect.translated(_topLeft));

	emit CanUndo(_history->CanUndo());
	emit CanRedo(_history->CanRedo());
	emit RubberBandSelection(true);
}
#endif
