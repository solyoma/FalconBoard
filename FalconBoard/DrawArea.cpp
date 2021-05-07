#include "DrawArea.h"
#include "DrawArea.h"
#include "DrawArea.h"

#include <QApplication>
#include <QMouseEvent>

#include <QFileDialog>
#include <QPainter>

#include <QMessageBox>

#if defined(QT_PRINTSUPPORT_LIB)
    #include <QtPrintSupport/qtprintsupportglobal.h>
    #if QT_CONFIG(printdialog)
        #include <QPrinter>
        #include <QPrintDialog>
        #include <QPrinterInfo>
    #endif
#endif

#include <math.h>

#include "common.h"

#include "myprinter.h"

#define DEBUG_LOG(qs) \
{							 \
	QFile f("debug.log");		 \
	f.open(QIODevice::Append);	 \
	QTextStream ofsdbg(&f);		 \
	ofsdbg << qs << "\n";			 \
}

// !!!
DrawColors drawColors;      // global used here and for print, declared in common.h

//----------------------------- DrawArea ---------------------
DrawArea::DrawArea(QWidget* parent)    : QWidget(parent)
{
    setAttribute(Qt::WA_StaticContents);
    setAttribute(Qt::WA_TabletTracking);
    setCursor(Qt::CrossCursor);
    setMouseTracking(true);
    _historyList.reserve(10);                  // max number of TABs possible is 10, must be checked
    //_historyList.push_back(new History());     // this way no reallocations, so _history remains valid
    //_history = _historyList[0];
}

void DrawArea::SetPrinterData(const MyPrinterData& prdata)
{
     _prdata.screenPageWidth = prdata.screenPageWidth; 
     _prdata.flags = prdata.flags;
     _prdata.printerName = prdata.printerName;
}

void DrawArea::ClearRoll()
{
#ifndef _VIEWER
    _RemoveRubberBand();
#endif
    _history->AddClearRoll();
    _ClearCanvas();
}

void DrawArea::ClearVisibleScreen()
{
#ifndef _VIEWER
    _RemoveRubberBand();
#endif
    _history->AddClearVisibleScreen();
    _ClearCanvas();
}

void DrawArea::ClearDown()
{
#ifndef _VIEWER
    _RemoveRubberBand();
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
 *          if a name is given loads the history as well
 * PARAMS:  name: name for new history. String may be empty
 *                  in which case a new unsaved page is added
 *          indexAt - insert at this position. 
 *                  if > capacity then at the end
 *          loadIt - if name is not empty also load data
 * GLOBALS: _history
 * RETURNS: index of added history or 
 *              -1 if capacity reached or
 *              -2 if file name given but could not load it
 *          but file did not exist
 * REMARKS: - adding history is unsucessfull if it is full
 *          - capacity is always smaller than 100000!
 *-------------------------------------------------------*/
int DrawArea::AddHistory(const QString name, bool loadIt, int indexAt )
{
    if(_historyList.capacity() == HistoryListSize())
        return -1;

    if (_history)
        _history->topLeft = _topLeft;

    DHistory* ph = new DHistory();
    ph->SetCopiedLists(&_copiedImages, &_copiedItems, &_copiedRect);
    if (!name.isEmpty())
        ph->SetName(name);

    if (indexAt > _historyList.capacity())
    {
        _historyList.push_back(ph);
        indexAt = HistoryListSize() - 1;
    }
    else
        _historyList.insert(_historyList.begin() + indexAt, ph);

    bool b = _currentHistoryIndex == indexAt;
    _currentHistoryIndex = indexAt;
    _history = _historyList[indexAt];
    if (!name.isEmpty() && loadIt)
        if (!_history->Load())
            return -2;

    _topLeft = _history->topLeft;

    if (!b)
        _Redraw(true);
    return indexAt;
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
        _RemoveRubberBand();
#endif
        if (index >= 0)     // store last viewport into previously shown history
        {
            if(_history)    // there is a history
                _history->topLeft = _topLeft;
            _currentHistoryIndex = index;
            _history = _historyList[index];
            _topLeft = _history->topLeft;
        }
        else
            index = _currentHistoryIndex;
    }
    int res = 1;
    if (redraw)
    {
        res = _history->Load();   // only when it wasn't loaded before
        _SetCanvasRect();
        _Redraw(true);
#ifndef _VIEWER
        _ShowCoordinates(QPoint());
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
    if(index < 0 || index > HistoryListSize())
        return -1;

    delete _historyList[index];
    _historyList.erase(_historyList.begin() + index);
    if (index == _currentHistoryIndex)
    {
        _currentHistoryIndex = -1;
        _history = nullptr;
    }
    else if (index < _currentHistoryIndex)
        --_currentHistoryIndex;
    return HistoryListSize();;
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
    DHistory* pdh = _historyList[to];
    _historyList[to] = _historyList[from];
    _historyList[from] = pdh;
    _currentHistoryIndex = from;
}

int DrawArea::Load()
{
//    _history->ScreenShotList().clear();
    int res = _history->Load(); // only loads if names diferent

    QString qs = _history->Name(); //  .mid(_history->Name().lastIndexOf('/') + 1);
    if (!res)
        QMessageBox::about(this, tr(WindowTitle), QString( tr("'%1'\nInvalid file")).arg(qs));
    else if(res == -1)
        QMessageBox::about(this, tr(WindowTitle), QString(tr("File\n'%1'\n not found")).arg(qs));
    else if(res < 0)    // i.e. < -1
        QMessageBox::about(this, tr(WindowTitle), QString( tr("File read problem. %1 records read. Please save the file to correct this error")).arg(-res-2));
    if(res && res != -1)    // TODO send message if read error
    {
        _topLeft = QPoint(0, 0);
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
    QImage visibleImage = grab().toImage();
    if (visibleImage.save(fileName, fileFormat)) 
    {
        return true;
    }
    return false;
}
#endif

void DrawArea::SetMode(bool darkMode, QString color, QString sGridColor, QString sPageGuideColor)
{
    _backgroundColor = color;
    _gridColor = sGridColor;
    _pageGuideColor = sPageGuideColor;
     drawColors.SetDarkMode( _darkMode = darkMode);
    _Redraw();                  // because pen color changed!
}

void DrawArea::SetPenKind(MyPenKind newKind, int width)
{
    _myPenKind = newKind;
    if(width > 0)
        _penWidth = width;
}

void DrawArea::AddScreenShotImage(QImage& image)
{
    ScreenShotImage bimg;
    bimg.image = image;
    int x = (geometry().width() -  image.width()) / 2,
        y = (geometry().height() - image.height()) / 2;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    bimg.topLeft = QPoint(x, y) + _topLeft;
    _history->AddScreenShot(bimg);
    emit CanUndo(_history->CanUndo());
    emit CanRedo(_history->CanRedo());
}


/*========================================================
 * TASK:    check history if it is modified
 * PARAMS:  any - if true check all histories until the
 *              first modified is found
 * GLOBALS:
 * RETURNS: if any=false: current history index + 1 if modified or 0
 *              true:  the index of the first modified history + 1
 * REMARKS: - only need to set 'any' to true when closing the app.
 *-------------------------------------------------------*/
int DrawArea::IsModified(bool any) const
{
    if (!any)
        return _historyList[_currentHistoryIndex]->IsModified() ? _currentHistoryIndex + 1 : 0;
    // else 
    for (int i = 0; i < HistoryListSize(); ++i)
        if (_historyList[i]->IsModified())
            return  i + 1;
    return 0;
}

int DrawArea::IsModified(int index) const
{
    return _historyList[index < 0 ? _currentHistoryIndex : index]->IsModified();
}

#ifndef _VIEWER
void DrawArea::InsertVertSpace()
{
    _RemoveRubberBand();
    _history->AddInsertVertSpace(_rubberRect.y() + _topLeft.y(), _rubberRect.height());
    _Redraw();
}
MyPenKind DrawArea::PenKindFromKey(int key)
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
    if(!_history->SelectedSize())
        _history->CollectItemsInside(_rubberRect.translated(_topLeft));

    MyPenKind pk = PenKindFromKey(key);
    HistoryItem* phi = _history->AddRecolor(pk);
//    _RemoveRubberBand();
    if (phi)
        _Redraw();
    return true;
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
        _canvas.fill(qRgba(255, 255, 255, 0));     // transparent
        _clippingRect = _canvasRect;
    }
    else
    {
        QPainter painter(&_canvas);
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
    MyPenKind pk = PenKindFromKey(key);
    emit PenKindChange(pk);
}
#endif

void DrawArea::keyPressEvent(QKeyEvent* event)
{
    _mods = event->modifiers();
    int key = event->key();

#if !defined _VIEWER && defined _DEBUG
    if (key == Qt::Key_D && (_mods.testFlag(Qt::ControlModifier) && _mods.testFlag(Qt::AltModifier)))
    {                           // toggle debug mode
        _debugmode = !_debugmode;
        if (_mods.testFlag(Qt::ShiftModifier))
            _pencilmode = !_pencilmode;
        _Redraw();
    }
#endif

    if (!_scribbling && !_pendown && key == Qt::Key_Space)
    {
        _spaceBarDown = true;
        QWidget::keyPressEvent(event);
    }
    else if (event->spontaneous())
    {
#ifndef _VIEWER
        bool bPaste = _itemsCopied &&
            ((key == Qt::Key_Insert && _mods.testFlag(Qt::ShiftModifier)) ||
                (key == Qt::Key_V && _mods.testFlag(Qt::ControlModifier))
                );

        if (_rubberBand)    // delete rubberband for any keypress except pure modifiers
        {
            bool bDelete = key == Qt::Key_Delete || key == Qt::Key_Backspace,
                 bCut    = ((key == Qt::Key_X) && _mods.testFlag(Qt::ControlModifier)) ||
                           ((key == Qt::Key_Insert) && _mods.testFlag(Qt::ShiftModifier)),
                 bCopy   = (key == Qt::Key_Insert || key == Qt::Key_C || key == Qt::Key_X) &&
                                        _mods.testFlag(Qt::ControlModifier),
                 bRemove = (bDelete | bCopy | bCut | bPaste) ||
                           (key != Qt::Key_Control && key != Qt::Key_Shift && key != Qt::Key_Alt && key != Qt::Key_R && key != Qt::Key_C),
                 bCollected = false,
                 bRecolor = (key == Qt::Key_1 || key == Qt::Key_2 || key == Qt::Key_3 || key == Qt::Key_4 || key == Qt::Key_5),

                 bRotate  = (key == Qt::Key_0 ||  // rotate right by 90 degrees
                             key == Qt::Key_8 ||  // rotate by 180 degrees
                             key == Qt::Key_9 ||  // rotate left by 90 degrees
                             key == Qt::Key_H ||  // flip horizontally
                             (key == Qt::Key_V && !_mods)       // flip vertically when no modifier keys pressed
                            );

            if (bDelete || bCopy || bRecolor || bRotate)
            {
                if ((bCollected = _history->SelectedSize()) && !bDelete && !bRotate)
                {
                    _history->CopySelected();
                    _itemsCopied = true;        // never remove selected list
                }
            }
                // if !bCollected then history's _selectedList is empty, but the rubberRect is set into _selectionRect
            if ((bCut || bDelete) && bCollected)
            {
                _history->AddDeleteItems();
                _RemoveRubberBand();
                _Redraw();
            }
            else if (bDelete && !bCollected)     // delete empty area
            {
                if (_history->AddRemoveSpaceItem(_rubberRect))     // there was something (not delete below the last item)
                {
                    _RemoveRubberBand();
                    _Redraw();
                }
            }
			else if (bPaste)
			{           // _history's copied item list is valid, each item is canvas relative
                        // get offset to top left of encompassing rect of copied items relative to '_topLeft'
				QPoint dr = _rubberRect.translated(_topLeft).topLeft(); 

                HistoryItem* phi = _history->AddPastedItems(dr);
                if (phi)
                {
                    _Redraw(); // update();
                    _rubberRect = QRect(_rubberRect.topLeft(), _copiedRect.size());
                    _rubberBand->setGeometry(_rubberRect );
                    _history->CollectPasted(_rubberRect.translated(_topLeft));
                }
			}
            else if (bRecolor)
            {
                ChangePenColorByKeyboard(key);
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
            else if (key == Qt::Key_R)    // draw rectangle with margin or w,o, margin with Shift
            {
                _actPenWidth = _penWidth;
                _lastScribbleItem.clear();
                _lastScribbleItem.type = heScribble;
                            // when any draweables is selected draw the rectangle with 1 pixel margins on all sides
                int margin = /* !_mods.testFlag(Qt::ShiftModifier)*/ _history->SelectedSize() ? _actPenWidth/2+1 : 0;

                int x1, y1, x2, y2, x3, y3, x4, y4;
                x1 = _rubberRect.x()-margin;                y1 = _rubberRect.y() - margin;
                x2 = x1 + _rubberRect.width() + 2 * margin; y2 = y1;
                x3 = x2;                                    y3 = y1 + _rubberRect.height() + 2 * margin;
                x4 = x1;                                    y4 = y3;

                _firstPointC = _lastPointC = QPoint(x1, y1);
                _lastScribbleItem.add(QPoint(x1,y1) + _topLeft);
                _DrawLineTo(QPoint(x2, y2));
                _lastScribbleItem.add(QPoint(x2, y2) + _topLeft);
                _DrawLineTo(QPoint(x3, y3));
                _lastScribbleItem.add(QPoint(x3, y3) + _topLeft);
                _DrawLineTo(QPoint(x4, y4));
                _lastScribbleItem.add(QPoint(x4, y4) + _topLeft);
                _DrawLineTo(QPoint(x1, y1));
                _lastScribbleItem.add(QPoint(x1, y1) + _topLeft);

                _lastScribbleItem.penKind = _myPenKind;
                _lastScribbleItem.penWidth = _actPenWidth;

                _rubberRect.adjust(-_actPenWidth / 2.0, -_actPenWidth / 2.0, _actPenWidth / 2.0, _actPenWidth / 2.0);
                _rubberBand->setGeometry( _rubberRect );
                HistoryItem *pscrbl =_history->AddScribbleItem(_lastScribbleItem);
                _history->AddToSelection();
            }
            else if (key == Qt::Key_C && !bCopy)   // draw ellipse
            {
                _actPenWidth = _penWidth;
                _lastScribbleItem.clear();
                _lastScribbleItem.type = heScribble;

                QPainterPath myPath;
                QRect rf = _rubberRect;
                myPath.addEllipse(rf);
                QList<QPolygonF> polList = myPath.toSubpathPolygons();
                QPoint pt;
                if (polList.size())
                {
                    _lastPointC = QPoint( polList[0][0].x(), polList[0][0].y());
                    _lastScribbleItem.add(_lastPointC + _topLeft);
                    for (auto p : polList)
                    {
                        for (auto ptf : p)
                        {
                            pt = QPoint( (int)ptf.x(),(int)ptf.y() );
                            _DrawLineTo(pt);
                            _lastPointC = pt;
                            _lastScribbleItem.add(_lastPointC + _topLeft);
                        }
                    }
                    _rubberRect.adjust(-_actPenWidth / 2.0, -_actPenWidth / 2.0, _actPenWidth / 2.0, _actPenWidth / 2.0);
                    _rubberBand->setGeometry( _rubberRect );
                    HistoryItem *pscrbl =_history->AddScribbleItem(_lastScribbleItem);
                    pscrbl->GetScribble()->bndRect.adjust(-_actPenWidth/2.0, -_actPenWidth / 2.0,_actPenWidth / 2.0, _actPenWidth / 2.0);
                    _history->AddToSelection();
                }
            }
            else if (bRemove)
                _RemoveRubberBand();

        }
        else    // no rubberBand
#endif
        {
#ifndef _VIEWER
            if (bPaste)         // paste as sprite
            {
                if (_history->SelectedSize())    // anything to paste?
                {
                    _pSprite = _SpriteFromLists();
                    _PasteSprite();
                    _Redraw();
                }
            }
			else 
#endif
                if (key == Qt::Key_PageUp)
				_PageUp();
			else  if (key == Qt::Key_PageDown)
				_PageDown();
			else  if (key == Qt::Key_Home)
				_Home(_mods.testFlag(Qt::ControlModifier) );
            else  if (key == Qt::Key_End)
                _End();
//            else if (key == Qt::Key_Shift)
//				_shiftKeyDown = true;

            else if (key == Qt::Key_Up)
                _Up(_mods.testFlag(Qt::ControlModifier) ? 100 : 10);
            else if (key == Qt::Key_Down)
                _Down(_mods.testFlag(Qt::ControlModifier) ? 100 : 10);
            else if (key == Qt::Key_Left)
                _Left(_mods.testFlag(Qt::ControlModifier) ? 100 : 10);
            else if (key == Qt::Key_Right)
                _Right(_mods.testFlag(Qt::ControlModifier) ? 100 : 10);
            else if(key == Qt::Key_BracketRight )
                emit IncreaseBrushSize(1);
            else if(key == Qt::Key_BracketLeft )
                emit DecreaseBrushSize(1);
            else if(key == Qt::Key_F4 && _mods .testFlag(Qt::ControlModifier) )
                emit CloseTab(_currentHistoryIndex);
            else if ( (key == Qt::Key_Tab || key == Qt::Key_Backtab) && _mods.testFlag(Qt::ControlModifier))
                emit TabSwitched(key == Qt::Key_Backtab ? -1:1); // Qt: Shit+Tab is Backtab
#ifndef _VIEWER
            else if (key == Qt::Key_1 || key == Qt::Key_2 || key == Qt::Key_3 || key == Qt::Key_4 || key == Qt::Key_5)
                ChangePenColorByKeyboard(key);
#endif
		}
#ifndef _VIEWER
        emit CanUndo(_history->CanUndo());
        emit CanRedo(_history->CanRedo());
#endif
    }
}

void DrawArea::keyReleaseEvent(QKeyEvent* event)
{
    _mods = event->modifiers();

    if ( (!_spaceBarDown && !_mods.testFlag(Qt::ShiftModifier) || !event->spontaneous() || event->isAutoRepeat()))
    {
        QWidget::keyReleaseEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Space)
    {
        _spaceBarDown = false;
        if (_scribbling || _pendown)
        {
            _RestoreCursor();
            QWidget::keyReleaseEvent(event);
            return;
        }
    }
    //if(event->key() == Qt::Key_Shift)
    //    _shiftKeyDown = false;

    _startSet = false;
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
    dy += event->pixelDelta().y();      // this did not do anything for me on Win10 64 bit
    dy += event->pixelDelta().y();

    if (!dy)
        dy = degv / 8;      // 15 degree
    if (!dx)
        dx = degh / 8;      // 15 degree

    const constexpr int tolerance = 3;
    if ( dy > tolerance || dy < -tolerance || dx > tolerance || dx < -tolerance)
    {
        _ShiftAndDisplayBy(QPoint(dx, dy));

        degv = degh = 0;
        dx = dy = 0;

#ifndef _VIEWER
        if (_rubberBand)
            _RemoveRubberBand();
#endif
    }
    else
        event->ignore();
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

    QTabletEvent::PointerType pointerT = event->pointerType();

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
#ifndef _VIEWER
    
    if ( event->button == Qt::RightButton ||        // Even for tablets when no pen pressure
        (event->button == Qt::LeftButton && event->mods.testFlag(Qt::ControlModifier)))
    {
        _InitRubberBand(event);
#else
    _lastPointC = event->pos;     // used for moving the canvas around
#endif

#ifndef _VIEWER
    }
    else
#endif
    if (event->pressure != 0.0 && event->button == Qt::LeftButton && !_pendown)  // even when using a pen some mouse messages still appear
    {
        if (_spaceBarDown)
            _SaveCursorAndReplaceItWith(Qt::ClosedHandCursor);

        if(event->fromPen)
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

            _RemoveRubberBand();
        }

//        _altKeyDown = event->modifiers().testFlag(Qt::AltModifier);
//DEBUG_LOG(QString("mousePress #1: first:(%1,%2) last=(%3,%4)").arg(_firstPointC.x()).arg(_firstPointC.y()).arg(_lastPointC.x()).arg(_lastPointC.y()))
        if (event->mods.testFlag(Qt::ShiftModifier))
        {
//            _mods.setFlag(Qt::ShiftModifier, false);
            _firstPointC = _lastPointC;
            _InitiateDrawingIngFromLastPos();
//            _DrawLineTo(event->pos );
//DEBUG_LOG(QString("mousePress #2:  drawn: => last=(%1,%2)").arg(_lastPointC.x()).arg(_lastPointC.y()))

//            if (_DrawFreehandLineTo(event->pos))
//                _lastScribbleItem.add(_lastPointC + _topLeft);
//            event->mods.setFlag(Qt::ShiftModifier, true);
        }
        else
            _InitiateDrawing(event);        // resets _lastPointC to event_>pos()
#endif
    }
#ifndef _VIEWER
	_ShowCoordinates(event->pos);
#endif
}

void DrawArea::MyMoveEvent(MyPointerEvent* event)
{
#ifndef _VIEWER
    _ShowCoordinates(event->pos);
#endif
    if ((!_allowPen && event->fromPen) || (!_allowMouse && !event->fromPen))
        return;
#ifndef _VIEWER

    if ((event->buttons & Qt::RightButton) ||
        (event->buttons & Qt::LeftButton && event->mods.testFlag(Qt::ControlModifier)) && _rubberBand)
    {
        QPoint epos = event->pos;
        if (event->mods.testFlag(Qt::ShiftModifier))        // constrain rubberband to a square
            epos.setX(_rubberBand->geometry().x() + (epos.y() - _rubberBand->geometry().y()));
        _rubberBand->setGeometry(QRect(_rubber_origin, epos).normalized()); // means: top < bottom, left < right

// DEBUG
//#if defined _DEBUG
//        qDebug("%s: rubberband size:%d,%d", (event->fromPen ? "tablet":"mouse:"),_rubberBand->size().width(), _rubberBand->size().height());
//#endif
// /DEBUG
    }
    else
#endif
                // mouse                                             pen
        if ( ((event->buttons & Qt::LeftButton) && _scribbling) || _pendown)
        {
            static QPoint lastpos;
            static int counter; // only refresh screen when this is REFRESH_LIMIT
                                // because tablet events frequency is large
            const int REFRESH_LIMIT = 10;
            QPoint pos = event->pos;
            if (lastpos == pos)
                return;
            else
                lastpos = pos;

            QPoint  dr = (event->pos - _lastPointC);   // displacement vector
            if (!dr.manhattanLength() )
                return;

#ifndef _VIEWER
            if (_pSprite)     // move sprite
            {
                _MoveSprite(dr);
                _lastPointC = event->pos;
            }
            else
            {
#endif
                if (_spaceBarDown)
                {
                    if(event->fromPen)
                    {
                        ++counter;
                        if (counter >= REFRESH_LIMIT)
                        {
                            counter = 0;
                            _Redraw();
                        }
                    }
                    _ShiftOrigin(dr);
                    if(!event->fromPen)
                        _Redraw();

                    _lastPointC = event->pos;
                }
#ifndef _VIEWER
                else
                {
                    if (_DrawFreehandLineTo(event->pos) )
                        _lastScribbleItem.add(_lastPointC + _topLeft);
                }
            }
#endif
        }
}

void DrawArea::MyButtonReleaseEvent(MyPointerEvent* event)
{
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
#endif
            if (!_spaceBarDown)
            {
#ifndef _VIEWER
//DEBUG_LOG(QString("Mouse release #1: _lastPoint: (%1,%2)").arg(_lastPointC.x()).arg(_lastPointC.y()))
                if (_DrawFreehandLineTo(event->pos))
                    _lastScribbleItem.add(_lastPointC + _topLeft);

//DEBUG_LOG(QString("Mouse release #2: _lastPoint: (%1,%2)\n__lastDrwanItem point size:%3").arg(_lastPointC.x()).arg(_lastPointC.y()).arg(_lastScribbleItem.points.size()))
                _history->AddScribbleItem(_lastScribbleItem);

                emit CanUndo(_history->CanUndo());
                emit CanRedo(_history->CanRedo());
#endif
            }
            else
                _RestoreCursor();
#ifndef _VIEWER
        }
#endif 
        _scribbling = false;
        _pendown = false;
        _startSet = false;
    }
#ifndef _VIEWER
    else if (_rubberBand)
    {
// DEBUG
//#if defined _DEBUG
//        qDebug("Button Release: rubberband size:%d,%d", _rubberBand->size().width(), _rubberBand->size().height());
//#endif
// /DEBUG
        if (_rubberBand->geometry().width() > 10 && _rubberBand->geometry().height() > 10)
        {
            _rubberRect = _rubberBand->geometry();
            _history->CollectItemsInside(_rubberRect.translated(_topLeft));
            if (_history->SelectedSize())
            {
                _rubberBand->setGeometry(_history->BoundingRect().translated(-_topLeft));
                _rubberRect = _rubberBand->geometry(); // _history->BoundingRect().translated(-_topLeft);
            }
        }
        else if (event->mods.testFlag(Qt::ControlModifier))     // Ctrl + click: select image if any
        {
            QPoint p = event->pos + _topLeft;
            int i = _history->SelectTopmostImageFor(p);
            if (i >= 0)
            {
                _rubberBand->setGeometry(_history->Image(i).Area().translated(-_topLeft));
                _rubberRect = _rubberBand->geometry(); // _history->BoundingRect().translated(-_topLeft);
            }
        }
        else
            _RemoveRubberBand();
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

    int nxp = _topLeft.x() / _screenWidth,
        nyp = _topLeft.y() / _prdata.screenPageHeight,
        nx  = (_topLeft.x() + width())/_screenWidth, 
        ny  = (_topLeft.y() + height())/_prdata.screenPageHeight,
        x=height(), y;

    painter.setPen(QPen(_pageGuideColor, 2, Qt::DotLine));
    if (nx - nxp > 0)     // x - guide
        for(x = nxp + 1 * _screenWidth - _topLeft.x(); x < width(); x += _screenWidth )
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
 * REMARKS: - layers:
 *                  top     sprite layer     ARGB
 *                          _canvas          ARGB
 *                          page guides if shown
 *                          screenshots
 *                          grid        if shown
 *                  bottom  background image if any
 *          - paint these in bottom to top order on this widget 
 *          - event->rect() is widget relative
 *          - area to paint is clipped to event->rect()
 *              any other clipping is applied on top of this
 *-------------------------------------------------------*/
void DrawArea::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);             // show image on widget
    QRect dirtyRect = event->rect();
    painter.fillRect(dirtyRect, _backgroundColor);             // draw on here

// bottom layer: background with possible background image
    if(!_background.isNull()) 
        painter.drawImage(dirtyRect, _background, dirtyRect);
    if (_bGridOn)
        _DrawGrid(painter);

    if (_history)
    {
        // screenshots layer: images below user drawn lines
        QRect r = dirtyRect.translated(_topLeft);              // screen -> absolute coord
        ScreenShotImage* pimg = _history->ScreenShotList().FirstVisible(r);   // pimg intersects r
        while (pimg)                                            // image layer
        {
            QRect intersectRect = pimg->AreaOnCanvas(_clippingRect);      // absolute
            painter.drawImage(intersectRect.translated(-_topLeft), pimg->image, intersectRect.translated(-pimg->topLeft));
            pimg = _history->ScreenShotList().NextVisible();
        }
    }
// _canvas layer: the scribbles
    painter.drawImage(dirtyRect, _canvas, dirtyRect);          // canvas layer

// page guides layer:
    if (_bPageGuidesOn)
        _DrawPageGuides(painter);
// sprite layer
    if(_pSprite && _pSprite->visible)
        painter.drawImage(dirtyRect.translated(_pSprite->topLeft), _pSprite->image, dirtyRect);  // sprite layer: dirtyRect: actual area below sprite 
}

void DrawArea::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

// DEBUG
    QSize s = geometry().size();
// /DEBUG
    int h = height();
    int w = width();
    if (_limited && _topLeft.x() + w > _screenWidth)
        _ShiftOrigin(QPoint( (_topLeft.x() + w - _screenWidth),0));

    _SetCanvasRect();
    _clippingRect = _canvasRect;

    if (width() > _canvas.width() || height() > _canvas.height()) 
    {
        int newWidth =  qMax(w + 128,  _canvas.width());
        int newHeight = qMax(h + 128, _canvas.height());
        _ResizeImage(&_canvas, QSize(newWidth, newHeight), true);
        if(_history)
            _history->SetClippingRect(QRect(_topLeft, QSize(newWidth, newHeight)));

        _Redraw();
    }

}
#ifndef _VIEWER
void DrawArea::_RemoveRubberBand()
{
    if (_rubberBand)
    {
        _rubberBand->hide();
        delete _rubberBand;
        _rubberBand = nullptr;
        emit RubberBandSelection(false);
    }
}

/*========================================================
 * TASK:    starts drawing at position _lastPosition
 * PARAMS:  NONE
 * GLOBALS: _spaceBarDown, _eraseMode, _lastScribbleItem,
 *          _actPenWidth, _actpenColor, _lastPoint, _topLeft
 * RETURNS:
 * REMARKS: -
 *-------------------------------------------------------*/
void DrawArea::_InitiateDrawingIngFromLastPos()
{
    if (_spaceBarDown)      // no drawing
        return;

    _lastScribbleItem.clear();
    if (_erasemode)
        _lastScribbleItem.type = heEraser;
    else
        _lastScribbleItem.type = heScribble;
    _actPenWidth = _penWidth;

    _lastScribbleItem.penKind = _myPenKind;
    _lastScribbleItem.penWidth = _actPenWidth;
    _lastScribbleItem.add(_lastPointC + _topLeft);
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
    _rubberBand->setGeometry(QRect(_rubber_origin, QSize()));
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
void DrawArea::_ModifyIfSpecialDirection(QPoint& qpC)
{
    if (_startSet)
    {
        if (_isHorizontal)
            qpC.setY(_firstPointC.y());
        else
            qpC.setX(_firstPointC.x());
    }
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
 *          - without contraint does not modify newEndPoint
 *              and always returns true
 *-------------------------------------------------------*/
bool DrawArea::_CanSavePoint(QPoint& newEndPointC)   // endPoint relative to canvas, not _topLeft
{
//DEBUG_LOG(QString("CanSavePoint #1: firstPointC:(%1,%2) newEndPointC=(%3,%4)").arg(_firstPointC.x()).arg(_firstPointC.y()).arg(newEndPointC.x()).arg(newEndPointC.y()))
     if (!_startSet && (_mods.testFlag(Qt::ShiftModifier) && (_pendown || _scribbling)))
     {
        int x0 = _firstPointC.x(),    // relative to canvas
            y0 = _firstPointC.y(),
            x  = newEndPointC.x(),
            y  = newEndPointC.y(),
            dx = abs(x - x0),
            dy = abs(y0 - y);

//DEBUG_LOG(QString("CanSavePoint #2: (dx, dy) = (%1,%2)").arg(dx).arg(dy))
        if (dx < 4 && dy < 4)
            return false;

        if(dx < 10 && dy < 10)
        {
            if (dx > dy)
                _isHorizontal = true;
            else if (dy > dx)
                _isHorizontal = false;
            _startSet = true;
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

QPoint DrawArea::_CorrectForDirection(QPoint &newpC)     // newpC canvas relative
{
    if (_startSet)  
    {
        if (_isHorizontal)
            newpC.setY(_firstPointC.y());
        else
            newpC.setX(_firstPointC.x());
    }

    return newpC;
}
#endif
void DrawArea::_MoveToActualPosition(QRect rect)
{
    int l = _topLeft.x(),
        t = _topLeft.y();

    if (!rect.isNull() && rect.isValid() && !_canvasRect.intersects(rect)) // try to put at the middle of the screen
    {
        if (rect.x() < l || rect.x() > l + _canvasRect.width())
            l = rect.x() - (_canvasRect.width() - rect.width()) / 2;
        if (rect.y() < t || rect.y() > t + _canvasRect.height())
            t = rect.y() - (_canvasRect.height() - rect.height()) / 2;

        if (l != _topLeft.x() || t != _topLeft.y())
        {
            _SetOrigin(QPoint(l, t));
            _Redraw();
        }
    }

}

 int DrawArea::_CollectScribbles(HistoryItemVector &hv)
{
    return _history->GetScribblesInside(_clippingRect, hv);
    
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
  *-------------------------------------------------------*/
bool DrawArea::_DrawFreehandLineTo(QPoint endPointC)
{
    bool result = true;
#ifndef _VIEWER
        if ((result = _CanSavePoint(endPointC)))     // i.e. must save point
        {
            _CorrectForDirection(endPointC); // when _startSet leaves only one coord moving
            _DrawLineTo(endPointC);
        }
#endif
    return result;
}

/*========================================================
 * TASK:    Draw line from '_lastPointC' to 'endPointC'
 * PARAMS:  endpointC : canvas relative coordinate
 * GLOBALS:
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
 *-------------------------------------------------------*/
void DrawArea::_DrawLineTo(QPoint endPointC)     // 'endPointC' canvas relative 
{
    QPainter painter(&_canvas);
    QPen pen = QPen(_PenColor(), (_pencilmode ? 1 : _actPenWidth), Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin);
/* 
                            THIS DOES NOT WORK, nothing gets painted:
    if (_actPenWidth > 2)
    {
        qreal cxy = _actPenWidth / 2.0;
        QRadialGradient gradient(cxy, cxy, cxy);
        gradient.setColorAt(0, _PenColor());
        gradient.setColorAt(0.9, _PenColor());
        gradient.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0));

        QBrush brush(gradient);
        pen.setBrush(brush);
    }
*/
    painter.setPen(pen);
    if (_erasemode && !_debugmode)
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
    else
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setRenderHints(QPainter::HighQualityAntialiasing | QPainter::SmoothPixmapTransform, true);

    QPointF ep = endPointC,
            lp = _lastPointC;

    painter.drawLine(lp, ep);    //? better?
    //painter.drawLine(_lastPointC, endPointC);
    int rad = (_actPenWidth / 2) + 2;
    update(QRect(_lastPointC, endPointC).normalized()
        .adjusted(-rad, -rad, +rad, +rad));

    _lastPointC = endPointC;
}


/*========================================================
 * TASK:    draws the polyline stored in drawnable on
 *          '_canvas'
 * PARAMS:  pscrbl - valid pointer to a ScribbleAble item
 * GLOBALS: _canvas,_myPenKind, _actPenWidth, _erasemode
 *          _clippingRect, _lastPointC, _topLeft
 * RETURNS:
 * REMARKS: - no errro checking on pscrbl
 *-------------------------------------------------------*/
void DrawArea::_DrawAllPoints(ScribbleItem* pscrbl)
{
    _myPenKind = pscrbl->penKind;
    _actPenWidth = pscrbl->penWidth;
    _erasemode = pscrbl->type == heEraser ? true : false;

    
    QPainter painter(&_canvas);
    QPen pen = QPen(_PenColor(), (_pencilmode ? 1 : _actPenWidth), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    painter.setPen(pen);
    if (_erasemode && !_debugmode)
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
    else
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setRenderHint(QPainter::HighQualityAntialiasing);

    QRect rect = _clippingRect.translated(-_topLeft); // _canvas relative
    painter.setClipRect(rect );

    _lastPointC = pscrbl->points[0] - _topLeft;
    QPoint pt;
    if(pscrbl->points.size() > 1)
        pt = pscrbl->points[1] - _topLeft;
    else
        pt = pscrbl->points[0] - _topLeft + QPoint(1,1);

    for (int i = 1; i < pscrbl->points.size()-1; ++i)
    {
        painter.drawLine(_lastPointC, pt);
        _lastPointC = pt;
        pt = pscrbl->points[i+1] - _topLeft;
    }
    painter.drawLine(_lastPointC, pt);

    int rad = (_actPenWidth / 2) + 2;
    rect = rect.intersected(pscrbl->bndRect.translated(-_topLeft)).normalized();
   
    if (!rect.isNull())
    {
        rect.adjust(-rad, -rad, +rad, +rad);
        update(rect);
    }
    _lastPointC = pt; 
}

void DrawArea::_ResizeImage(QImage* image, const QSize& newSize, bool isTransparent)
{
    if (image->size() == newSize)
        return;

    QImage newImage(newSize, QImage::Format_ARGB32);

    QColor color = isTransparent ? Qt::transparent : _backgroundColor;
    newImage.fill(color);
    QPainter painter(&newImage);
    painter.setCompositionMode(QPainter::CompositionMode_Source); //??
    painter.drawImage(QPoint(0, 0), *image);
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
 * REMARKS: - must be used to set bandHeight for _history
 *-------------------------------------------------------*/
void DrawArea::SlotForPrimaryScreenChanged(QScreen* ps)
{
    _history->SetBandHeight(ps->geometry().height());
}

void DrawArea::SlotForGridSpacingChanged(int spacing)
{
    _nGridSpacingX = _nGridSpacingY = spacing;
    _Redraw();
}

void DrawArea::PageSetup()      // public slot
{
    static float fact[] = {1.0, 1.0/2.54, 1.0/25.4};   // inch, cm, mm

    PageSetupDialog* ps = new PageSetupDialog(this, _prdata.printerName);
    _prdata.bExportPdf = false;

    if (ps->exec())
    {
        QSize wh;
        _screenWidth = ps->GetScreenSize(wh);

        _prdata.screenPageWidth = _screenWidth;
        _screenHeight = wh.height();

#define SQUARE(a)  (a*a)

        if (_screenHeight > 0)           // -1 if no predefined resolution
        {
            float cosine = (float)_screenWidth / (float)std::sqrt(SQUARE(_screenWidth) + SQUARE(_screenHeight));
            _ppi = (float)_screenWidth / (ps->screenDiagonal * fact[ps->unitIndex] * cosine) ;
        }
        else
            _ppi = 96;

#undef SQUARE
        _prdata.printerName = ps->_actPrinterName;
        _prdata.flags = ps->flags;
        _bPageSetupUsed = !_prdata.printerName.isEmpty();
        _bPageSetupValid = true;
    }
    else
        _bPageSetupValid = false;
    delete ps;
}

bool DrawArea::_PdfPageSetup()
{
    PdfSetupDialog* pdfDlg = new PdfSetupDialog(this);
    bool res = false;
    if (pdfDlg->exec())
    {
        _prdata.bExportPdf = true;
        _prdata.flags = pdfDlg->flags;

        static int resos[] = { 300, 600, 1200 };
        int dpi = resos[pdfDlg->pdfDpi];

        float   w = (pdfDlg->pdfWidth - 2 * pdfDlg->pdfMarginLR) * dpi,
                h = (pdfDlg->pdfHeight - 2 * pdfDlg->pdfMarginTB) * dpi,
                mlr = pdfDlg->pdfMarginLR * dpi,
                mtb = pdfDlg->pdfMarginTB * dpi;
        if (_prdata.flags & pfLandscape)
        {
            _prdata.printArea = QRect(mtb, mlr, h, w);
            _prdata.magn = h / _prdata.screenPageWidth;
            _prdata.pdfMarginLR = mtb;  
            _prdata.pdfMarginTB = mlr;
        }
        else
        {
            _prdata.printArea = QRect(mlr, mtb, w, h);
            _prdata.magn = w / _prdata.screenPageWidth;
            _prdata.pdfMarginLR = mlr;
            _prdata.pdfMarginTB = mtb;
        }
        _bPageSetupValid = true;
        res = true;
    }
    delete pdfDlg;

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
 * TASK:
 * PARAMS:
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
        PageSetup();

	if (_bPageSetupValid)
	{
		_prdata.topLeftActPage = _topLeft;
		_prdata.backgroundColor = _backgroundColor;
		_prdata.gridColor = (_prdata.flags & pfWhiteBackground) ? "#d0d0d0" : _gridColor;
		_prdata.pBackgroundImage = &_background;
		_prdata.nGridSpacingX = _nGridSpacingX;
		_prdata.nGridSpacingY = _nGridSpacingY;
		_prdata.gridIsFixed = _gridIsFixed;
        _printer = new MyPrinter(this, _history, _prdata);     // _prdata may be modified!
        if (_NoPrintProblems())   // only when not Ok
        {
            _printer->Print();
            _NoPrintProblems();
        }
	}
    _prdata.bExportPdf = false;
}

void DrawArea::ExportPdf(QString fileName, QString& directory)
{
    if (_PdfPageSetup())                // else cancelled
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
    MyPenKind savekind = _myPenKind;
    bool saveEraseMode = _erasemode;

    HistoryItemVector forPage;
    _CollectScribbles(forPage);  // using _yxOrder and _items sorted in ascending Z-number
    if(clear)
        _ClearCanvas();
    for (auto phi : forPage)
        _ReplotScribbleItem(phi);
    
    _myPenKind = savekind;
    _penWidth = savewidth;
    _erasemode = saveEraseMode;
}

QColor DrawArea::_PenColor()

{
    static MyPenKind _prevKind = penNone;
    static QColor color;
    if (_myPenKind == _prevKind)
    {
        if (_myPenKind != penBlack)
            return color;
        return _darkMode ? QColor(Qt::white) : QColor(Qt::black);
    }

    _prevKind = _myPenKind;

    switch (_myPenKind)
    {
        case penBlack: return  color = _darkMode ? QColor(Qt::white) : QColor(Qt::black);
        default:
            return color = drawColors[_myPenKind];
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


/*========================================================
 * TASK:    plots visible 'Scribbleable's onto _canvas
 *              if it intersects _clippingRect
 * PARAMS:  phi - possibly nullpointer to a HistoryItem
 * GLOBALS: _clippingRect
 * RETURNS:
 * REMARKS: - images noot plotted here, not even when they 
 *             are part of a pasted stack of items
 *-------------------------------------------------------*/
bool DrawArea::_ReplotScribbleItem(HistoryItem* phi)
{
    if (!phi || phi->IsImage() || !phi->Area().intersects(_clippingRect))
        return false;

    ScribbleItem* pscrbl;     // used when we must draw something onto the screen

    auto plot = [&]()   // lambda to plot point pointed by pscrbl
                {
        if (!pscrbl->isVisible || !pscrbl->intersects(_clippingRect))   // if the clipping rectangle has no intersection with         
            return;                              // the scribble

        // DEBUG
        // draw rectange around item to see if item intersects the rectangle
        //{
        //    QPainter painter(&_canvas);
        //    QRect rect = pscrbl->rect.adjusted(-1,-1,0,0);   // 1px pen left and top:inside rectangle, put pen outside rectangle
        //    painter.setPen(QPen(QColor(qRgb(132,123,45)), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        //    painter.drawRect(rect);
        //}
        // /DEBUG            
        _DrawAllPoints(pscrbl);
    };

    switch(phi->type)
    {
        case heScribble:
        case heEraser:    
            if( (pscrbl = phi->GetVisibleScribble(0)))
                plot();
            break;
        case heItemsDeleted:    // nothing to do
            break;
        case heItemsPastedTop:
            pscrbl = phi->GetVisibleScribble();
            for (int i = 1; pscrbl; ++i)
            {
                plot();
                pscrbl = phi->GetVisibleScribble(i);
            }
            break;
        default:
            break;
    }
                                    
    return true;
}

void DrawArea::_SetCanvasRect()
{
    _clippingRect = _canvasRect = QRect(0, 0, width(), height()).translated(_topLeft);     // 0,0 relative rectangle
}

#ifndef _VIEWER
void DrawArea::Undo()               // must draw again all underlying scribbles
{
    if (_history->CanUndo())
    {
        _RemoveRubberBand();

        QRect rect = _history->Undo();
        _MoveToActualPosition(rect); 
//        _clippingRect = rect;
        _ClearCanvas();

        _Redraw();
        _clippingRect = _canvasRect;
        emit CanRedo(_history->CanRedo());
    }
    emit CanUndo(_history->CanUndo());
}

void DrawArea::Redo()       // need only to draw undone items, need not redraw everything
{                           // unless top left or items color changed
    HistoryItem* phi = _history->Redo();
    if (!phi)
        return;

    _RemoveRubberBand();

    _MoveToActualPosition(phi->Area());
// ??    _clippingRect = phi->Area();

    _Redraw();

    _clippingRect = _canvasRect;

    emit CanUndo(_history->CanUndo() );
    emit CanRedo(_history->CanRedo());
}
void DrawArea::ChangePenColorSlot(int key)
{

    ChangePenColorByKeyboard(key);
}
#endif
void DrawArea::SetCursor(CursorShape cs, QIcon* icon)
{
    _erasemode = false;
    if (_spaceBarDown && (_scribbling || _pendown)) // do not set the cursor when space bar is pressed
        return;

    switch (cs)
    {
        case csArrow: setCursor(Qt::ArrowCursor); break;
        case csOHand: setCursor(Qt::OpenHandCursor); break;
        case csCHand: setCursor(Qt::ClosedHandCursor); break;
        case csPen:   setCursor(Qt::CrossCursor); break;
        case csEraser: SetEraserCursor(icon);  break;
        default:break;
    }
}

void DrawArea::SetEraserCursor(QIcon *icon)
{
    if (icon)
    {
        QPixmap pxm = icon->pixmap(64, 64);
        _eraserCursor = QCursor(pxm);
    }
    setCursor(_eraserCursor);
    _erasemode = true;
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

void DrawArea::_SetOrigin(QPoint o)
{
    _topLeft = o;
#ifndef _VIEWER
    _RemoveRubberBand();
#endif
    _canvasRect.moveTo(_topLeft);
    _clippingRect = _canvasRect;
    _history->SetClippingRect(_canvasRect);

#ifndef _VIEWER
    _ShowCoordinates(QPoint());
#endif
}


/*========================================================
 * TASK:    set new top left coordinate of current viewport
 *          by shiftting it with -delta
 * PARAMS:  delta: pixels to change _topLeft
 * GLOBALS: _topLeft
 * RETURNS:
 * REMARKS: - delta.x() < 0 - move viewport right
 *          - delta.y() < 0 - move viewport down
 *-------------------------------------------------------*/
void DrawArea::_ShiftOrigin(QPoint delta)    
{
    QPoint o = _topLeft;       // origin of screen top left relative to "paper"

    o -= delta;                // calculate new screen origin

    if (o.x() < 0)
        o.setX(0);

    if (delta.x() < 0 && _limited && o.x() + width() >= _screenWidth)
        o.setX(_topLeft.x());

    if (o.y() < 0)
        o.setY(0);

    _SetOrigin(o);
}
// 

/*========================================================
 * TASK:    scrolls visible area in a given direction
 * PARAMS:  delta: vector to move the area 
 *              if delta.x() > 0 moves left, 
 *              if delta.y() > 0 moves up, 
 *          smooth: smoth scrolling by only drawing the 
                changed bits
 * GLOBALS: _limited, _topLeft, _canvas
 * RETURNS:
 * REMARKS: - smooth == true mus only used when one of 
 *              delta.x() or delta.y() is zero
 *          - delta.x() < 0 - moves viewport right
 *            delta.y() < 0 - moves viewport down
 *-------------------------------------------------------*/
void DrawArea::_ShiftAndDisplayBy(QPoint delta, bool smooth)    // delta changes _topLeft, delta.x < 0 scroll right, delta.y < 0 scroll 
{
    if (_topLeft.y() - delta.y() < 0.0)
        delta.setY(_topLeft.y());
    if (_topLeft.x() - delta.x() < 0.0)
        delta.setX(_topLeft.x());
    if (delta.x() < 0 && _limited && _topLeft.x() - delta.x() + width() >= _screenWidth)
        delta.setX(_topLeft.x() + width() - _screenWidth);

    if(delta.isNull() )
       return;      // nothing to do


    if (smooth)     // use only for up/down/left/right never for diagonal
    {
        QImage origImg = _canvas;
        _canvas.fill(qRgba(255, 255, 255, 0));     // transparent
        QPainter painter(&_canvas);
        QRect rectSrc, rectDst,                    // canvas relatice coordinates
              rectRe;                              // _topLeft relative  
        int w = width(),
            h = height();
        rectSrc = _canvasRect.translated(-_topLeft-delta);  // relative to viewport
        rectSrc.adjust(0, 0, -qAbs(delta.x()), -qAbs(delta.y()));
        rectDst = rectSrc;
        rectDst.moveTo(0, 0);

        painter.drawImage(rectDst, origImg, rectSrc);   // move area
        // then plot newly visible areas
        if (delta.x() == 0.0)
        {
            if (delta.y() > 0)      // viewport up
                rectRe = QRect(0, 0, w, delta.y());
            else                    // viewport down
                rectRe = QRect(0, h + delta.y(), w, -delta.y());
        }
        else    // delta.y() is 0.0
        {
            if (delta.x() > 0)      // viewport left
                rectRe = QRect(w - delta.x(), 0, delta.x(), h);
            else                    // viewport right
                rectRe = QRect(0, 0, -delta.x(), h);
        }

        _ShiftOrigin(delta);

        rectRe.translate(_topLeft);
        _clippingRect = rectRe;
        _history->SetClippingRect(rectRe);
        _Redraw(false);
        update();
        _clippingRect = _canvasRect;
        _history->SetClippingRect(_canvasRect);
    }
    else
    {
        _ShiftOrigin(delta);
        _Redraw();
    }
}
void DrawArea::_PageUp()
{
    QPoint pt(0, geometry().height()/3*2);
    _ShiftAndDisplayBy(pt);
}
void DrawArea::_PageDown()
{
    QPoint pt(0, -geometry().height() / 3 * 2);
    _ShiftAndDisplayBy(pt);
}
void DrawArea::_Home(bool toTop)
{
    QPoint pt = _topLeft;

    if(!toTop)
        pt.setY(0);   // do not move in y direction
    _ShiftAndDisplayBy(pt);
}
void DrawArea::_End()
{
    _topLeft = _history->BottomRightVisible(geometry().size() );
    _SetOrigin(_topLeft);
    _Redraw();
}

void DrawArea::_Up(int amount)
{
    QPoint pt(0, amount);
    _ShiftAndDisplayBy(pt, false);
}
void DrawArea::_Down(int amount)
{
    QPoint pt(0, -amount);
    _ShiftAndDisplayBy(pt, false);
}
void DrawArea::_Left(int amount)
{
    QPoint pt(amount,0);
    _ShiftAndDisplayBy(pt, false);
}
void DrawArea::_Right(int amount)
{
    QPoint pt(-amount,0);
    _ShiftAndDisplayBy(pt, false);
}

#ifndef _VIEWER
void DrawArea::_ShowCoordinates(const QPoint& qp)
{
    static QPoint prevPoint = QPoint(0, 0),
                    qpt;
    if (qp.isNull())
        qpt = prevPoint;
    else
        qpt = prevPoint = qp;

    _lastCursorPos = qpt;

    qpt += _topLeft;

    emit TextToToolbar(QString(tr("   Left:%1, Top:%2 | Cursor: x:%3, y:%4 ")).arg(_topLeft.x()).arg(_topLeft.y()).arg(qpt.x()).arg(qpt.y()));
}

// ****************************** Sprite ******************************

/*========================================================
 * TASK:    Create a new sprite using lists of selected 
 *          items and images
 * PARAMS:  pos: event position relative to _topLeft
 *          rect: from _rubberRect relative to _topLeft
 *          deleted: was items deleted at original position?
 *                  false: no copied to sprite
 *          setVisible: should we display the sprite? (default: true)
 * GLOBALS: _topLeft, _pSprite
 * RETURNS: _pSprite
 * REMARKS: - _nSelectedList is used to create the other lists
 *            which leaves previously copied items intact
 *              so paste operation still pastes the non sprite
 *              data
 *-------------------------------------------------------*/
Sprite* DrawArea::_CreateSprite(QPoint pos, QRect& rect, bool deleted, bool setVisible)
{
    _pSprite = new Sprite(_history);        // this copies the selected items into lists
    return _PrepareSprite(_pSprite, pos, rect, deleted, setVisible);
}

Sprite* DrawArea::_PrepareSprite(Sprite* pSprite, QPoint cursorPos, QRect & rect, bool deleted, bool setVisible)
{
    pSprite->visible = setVisible;
    pSprite->itemsDeleted = deleted;       // signal if element(s) was(were) deleted before moving
    pSprite->topLeft = rect.topLeft();     // sprite top left
    pSprite->dp = cursorPos - pSprite->topLeft; // cursor position rel. to sprite
    pSprite->image = QImage(rect.width(), rect.height(), QImage::Format_ARGB32);
    pSprite->image.fill(qRgba(255, 255, 255, 0));     // transparent

    QPainter painter(&pSprite->image);
    QRect sr, tr;   // source & target
    for (ScreenShotImage& si : pSprite->images)
    {
        tr = QRect(si.topLeft, pSprite->rect.size() );
        sr = si.image.rect();       // -"-

        if (sr.width() > tr.width())
            sr.setWidth(tr.width());
        if (sr.height() > tr.height())
            sr.setHeight(tr.height());
        if (sr.width() < tr.width())
            tr.setWidth(sr.width());
        if (tr.height() < sr.height())
            tr.setHeight(sr.height());

        painter.drawImage(tr, si.image, sr);
    }
        // save color and line width
    MyPenKind pk = _myPenKind;
    int pw = _actPenWidth;
    bool em = _erasemode;

    for (auto& di : pSprite->items)
    {
        _myPenKind = di.penKind;
        _actPenWidth = di.penWidth;
        _erasemode = di.type == heEraser ? true : false;

        if(_erasemode)
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
        else 
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        painter.setPen(QPen(_PenColor(), _actPenWidth, Qt::SolidLine, Qt::RoundCap,
                            Qt::RoundJoin));

        QPoint p0 = di.points[0];        // sprite relative coordinates
        for (auto &p : di.points)
        {
            painter.drawLine(p0, p);
            p0 = p;
        }
    }
    // create border to see the rectangle
    if (_erasemode && !_debugmode)
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setPen(QPen((_darkMode ? Qt::white : Qt::black), 2, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin)) ;
    painter.drawLine(0, 0, pSprite->rect.width(), 0);
    painter.drawLine(pSprite->rect.width(), 0, pSprite->rect.width(), pSprite->rect.height());
    painter.drawLine(pSprite->rect.width(), pSprite->rect.height(), 0, pSprite->rect.height());
    painter.drawLine(0, pSprite->rect.height(), 0, 0);

    // restore data
    _myPenKind = pk;
    _actPenWidth = pw;
    _erasemode = em;

    return pSprite;
}

Sprite* DrawArea::_SpriteFromLists()
{
    Sprite *pSprite = new Sprite(_history, _history->SelectedItemsList(), _copiedRect, &_copiedImages, &_copiedItems);
    return _PrepareSprite(pSprite, _lastCursorPos, _copiedRect.translated(_lastCursorPos), false, true);
}

void DrawArea::_MoveSprite(QPoint dr)
{
//        QRect updateRect = _pSprite->rect.translated(_pSprite->topLeft);      // original rectangle
        _pSprite->topLeft += dr;
        if( _pSprite->topLeft.y() < 0)
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
    QRect updateRect = ps->rect.translated(ps->topLeft);    // original rectangle
    update(updateRect);
    _rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
    _rubberRect = updateRect;
    _rubberBand->setGeometry(updateRect);
    _rubberBand->show();
    delete ps;

    if (_history)
        _history->CollectPasted(_rubberRect.translated(_topLeft));

    emit CanUndo(_history->CanUndo());
    emit CanRedo(_history->CanRedo());
}
#endif
