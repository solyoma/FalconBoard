#include "DrawArea.h"
#include "DrawArea.h"

#include <QMouseEvent>

#include <QPainter>
#include <QMessageBox>

#if defined(QT_PRINTSUPPORT_LIB)
#include <QtPrintSupport/qtprintsupportglobal.h>
#if QT_CONFIG(printdialog)
#include <QPrinter>
#include <QPrintDialog>
#endif
#endif

//----------------------------- DrawColors -------------------
int DrawColors::_penColorIndex(MyPenKind pk)
{
	for (int i = 0; i < _COLOR_COUNT; ++i)
		if (_colors[i].kind == pk)
			return i;
	return -1;
}

DrawColors::DrawColors()
{
	_colors[0].kind = penBlack;
	_colors[1].kind = penRed;
	_colors[2].kind = penGreen;
	_colors[3].kind = penBlue;
	_colors[4].kind = penYellow;
}
void DrawColors::SetDarkMode(bool dark)
{
	_dark = dark;
}
QColor& DrawColors::operator[](MyPenKind pk)
{
	int i = _penColorIndex(pk);
	return   (i < 0 ? _invalid : (_dark ? _colors[i].darkColor : _colors[i].lightColor));
}

//----------------------------- DrawArea ---------------------
DrawArea::DrawArea(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StaticContents);
    setAttribute(Qt::WA_TabletTracking);
    setCursor(Qt::CrossCursor);
    SetPenColors();
}

void DrawArea::SetPenColors()
{
    drawColors.SetDarkMode(false);         // dark color  on light mode screen
    drawColors[penBlack] = Qt::black;
    drawColors[penRed] =   Qt::red;
    drawColors[penGreen] = "#007d1a";
    drawColors[penBlue] =  Qt::blue;
    drawColors[penYellow] = "#b704be";
    
    drawColors.SetDarkMode(true);          // use on dark mode screens
	drawColors[penBlack] = Qt::white;
	drawColors[penRed] = Qt::red;
	drawColors[penGreen] = Qt::green;
	drawColors[penBlue] = Qt::blue;
	drawColors[penYellow] = Qt::yellow;
}

void DrawArea::ClearCanvas()
{
#ifndef _VIEWER
    _RemoveRubberBand();
#endif
    _history.addClearCanvas();
    _ClearCanvas();
}

void DrawArea::ClearBackground()
{
    _background.fill(_backgroundColor);
    _isBackgroundSet = false;
    update();
}

int DrawArea::Load(QString name)
{
    int res = _history.Load(name, _tlMax);
    if (!res)
        QMessageBox::about(this, tr(WindowTitle), tr("Invalid file"));
    else if(res == -1)
        QMessageBox::about(this, tr(WindowTitle), tr("File not found"));
    else if(res < 0)
        QMessageBox::about(this, tr(WindowTitle), QString( tr("File read problem. %1 records read. Please save the file to correct this error")).arg(-res-1));
    if(res && res != -1)    // TODO send message if read error
    {
        _topLeft = QPoint(0, 0);
        if (_tlMax.x() < width())
            _tlMax.setX(0);
        else
            _tlMax.rx() = width()/2;
        _tlMax.ry() -= (height() - 100);

        _tlMax = -_tlMax;
        _ClearCanvas();
        _Redraw();
    }
    emit CanUndo(true);
    emit CanRedo(false);
    return res;
}

#ifndef _VIEWER
bool DrawArea::OpenBackgroundImage(const QString& fileName)
{
    if (!_loadedImage.load(fileName))
        return false;

    SetBackgroundImage(_loadedImage);

    return true;
}

bool DrawArea::SaveVisibleImage(const QString& fileName, const char* fileFormat)
{
    QImage visibleImage = _background;           
    _ResizeImage(&visibleImage, size(), false);

    QPainter painter(&visibleImage);
    painter.drawImage(QPoint(0,0), _canvas);


    if (visibleImage.save(fileName, fileFormat)) 
    {
        return true;
    }
    return false;
}

void DrawArea::SetBackgroundImage(QImage& loadedImage)
{
    QSize newSize = loadedImage.size().expandedTo(size());
    _ResizeImage(&loadedImage, newSize, false); // background not transparent
    _background = loadedImage;
    if (!_background.isNull())
    {
        _isBackgroundSet = true;
        _Redraw();
    }
    update();
}
#endif
void DrawArea::SetMode(bool darkMode, QString color)
{
    _backgroundColor = color;
    _background.fill(_backgroundColor);
#ifndef _VIEWER
    if (_isBackgroundSet)
            SetBackgroundImage(_loadedImage);
#endif
    drawColors.SetDarkMode( _darkMode = darkMode);
    _Redraw();                  // because pen color changed!
}

void DrawArea::SetPenKind(MyPenKind newKind)
{
    _myPenKind = newKind;
}

void DrawArea::SetPenWidth(int newWidth)
{
    _penWidth = newWidth;
}

void DrawArea::SetEraserWidth(int newWidth)
{
    _eraserWidth = newWidth;
}

#ifndef _VIEWER
void DrawArea::InsertVertSpace(int heightInPixels)
{
    int y = _rubberRect.y();
    _RemoveRubberBand();
    _history.addInsertVertSpace(y, heightInPixels);
    _ClearCanvas();
    _Redraw();
}
#endif

void DrawArea::NewData()
{
    ClearHistory();
    ClearBackground();
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

        painter.fillRect(_clippingRect.translated(-_topLeft), qRgba(255, 255, 255, 0));
    }
    update();
}

void DrawArea::keyPressEvent(QKeyEvent* event)
{
    if (!_scribbling && !_pendown && event->key() == Qt::Key_Space)
    {
        _spaceBarDown = true;
        QWidget::keyPressEvent(event);
    }
    else if (event->spontaneous())
    {
        int key = event->key();
        Qt::KeyboardModifiers mods = event->modifiers();

#ifndef _VIEWER
        if (_rubberBand)    // delete rubberband for any keypress except pure modifiers
        {
            bool bDelete = key == Qt::Key_Delete || key == Qt::Key_Backspace,
                 bCut    = ((key == Qt::Key_X) && mods.testFlag(Qt::ControlModifier)) ||
                           ((key == Qt::Key_Insert) && mods.testFlag(Qt::ShiftModifier)),
                 bCopy   = (key == Qt::Key_Insert || key == Qt::Key_C || key == Qt::Key_X) &&
                                        mods.testFlag(Qt::ControlModifier),
                 bPaste  =  _scribblesCopied && 
                            ( (key == Qt::Key_Insert && mods.testFlag(Qt::ShiftModifier)) ||
                              (key == Qt::Key_V && mods.testFlag(Qt::ControlModifier))
                            ),
                 bRemove = (bDelete | bCopy | bCut | bPaste) ||
                           (key != Qt::Key_Control && key != Qt::Key_Shift && key != Qt::Key_Alt),
                 bCollected = false,
                 bRecolor = (key == Qt::Key_1 || key == Qt::Key_2 || key == Qt::Key_3 || key == Qt::Key_4) &&
                            (mods.testFlag(Qt::ControlModifier) && mods.testFlag(Qt::AltModifier));

            if (bRemove)
                _RemoveRubberBand();

            if (bCut || bDelete || bCopy || bRecolor)
            {
                if ((bCollected = _history.CollectItemsInside(_rubberRect.translated(-_topLeft))))
                {
                    _history.CopySelected();
                    _scribblesCopied = true;        // never remove selected list
                }
            }

            if ((bCut || bDelete) && bCollected)
            {
                _history.addDeleteItems();
                _ClearCanvas();
                _Redraw();
            }

			if (bPaste)
			{           // _history's copied item list is valid, each item is canvas relative
				
                        // get offset to top left of encompassing rect of copied items relative to '_topLeft'
				QPoint dr = _rubberRect.translated(-_topLeft).topLeft(); 

                HistoryItem* phi = _history.addPastedItems(dr);
                if(phi)
				    _ReplotItem(phi);

                emit CanUndo(true);
                emit CanRedo(false);
			}
            if (bRecolor)
            {
                MyPenKind pk;
                switch (key)
                {
                    case Qt::Key_1:pk = penBlack;  break;
                    case Qt::Key_2:pk = penRed;  break;
                    case Qt::Key_3:pk = penGreen;  break;
                    case Qt::Key_4:pk = penBlue;  break;
                    case Qt::Key_5:pk = penYellow;  break;
                }
                HistoryItem* phi =_history.addRecolor(pk);
                if(phi)
                    _Redraw();
            }

            if (key == Qt::Key_R)    // draw rectangle
            {
                QRect r = _rubberRect;
                _firstPointC = _lastPointC = r.topLeft();
                _lastDrawnItem.clear();
                _lastDrawnItem.type = heScribble;
                _actPenWidth = _penWidth;
                _lastDrawnItem.add(_lastPointC - _topLeft);
                if (_DrawLineTo(r.topRight()) )
                    _lastDrawnItem.add(_lastPointC - _topLeft);
                if (_DrawLineTo(r.bottomRight()))
                    _lastDrawnItem.add(_lastPointC - _topLeft);
                if (_DrawLineTo(r.bottomLeft()))
                    _lastDrawnItem.add(_lastPointC - _topLeft);
                if (_DrawLineTo(r.topLeft()))
                    _lastDrawnItem.add(_lastPointC - _topLeft);

                _lastDrawnItem.penKind = _myPenKind;
                _lastDrawnItem.penWidth = _actPenWidth;
//                _lastDrawnItem.add(_lastPointC - _topLeft);

                _history.addDrawnItem(_lastDrawnItem);
                emit CanUndo(true);
                emit CanRedo(false);
            }
        }
        else
#endif
        {
			if (key == Qt::Key_PageUp)
				_PageUp();
			else  if (key == Qt::Key_PageDown)
				_PageDown();
			else  if (key == Qt::Key_Home)
				_Home(mods.testFlag(Qt::ControlModifier) );
            else  if (key == Qt::Key_End)
                _End();
            else if (key == Qt::Key_Shift)
				_shiftKeyDown = true;

            else if (key == Qt::Key_Up)
                _Up(mods.testFlag(Qt::ControlModifier) ? 100 : 10);
            else if (key == Qt::Key_Down)
                _Down(mods.testFlag(Qt::ControlModifier) ? 100 : 10);
            else if (key == Qt::Key_Left)
                _Left(mods.testFlag(Qt::ControlModifier) ? 100 : 10);
            else if (key == Qt::Key_Right)
                _Right(mods.testFlag(Qt::ControlModifier) ? 100 : 10);
            else if(key == Qt::Key_BracketRight )
                emit IncreaseBrushSize(1);
            else if(key == Qt::Key_BracketLeft )
                emit DecreaseBrushSize(1);
		}
    }
}

void DrawArea::keyReleaseEvent(QKeyEvent* event)
{
    if ( (!_spaceBarDown && !_shiftKeyDown) || !event->spontaneous() || event->isAutoRepeat())
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
    _shiftKeyDown = false;
    _startSet = false;
    QWidget::keyReleaseEvent(event);
}

void DrawArea::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !_pendown)  // even when using a pen some mouse message still appear
    {
        if (_spaceBarDown)
            _SaveCursorAndReplaceItWith(Qt::ClosedHandCursor);

        _scribbling = true;
#ifndef _VIEWER
        if (_rubberBand)
            _RemoveRubberBand();

        _InitiateDrawing(event);
    }
    else if (event->button() == Qt::RightButton)
    {
        _rubber_origin = event->pos();
        if (!_rubberBand)
            _rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
        _rubberBand->setGeometry(QRect(_rubber_origin, QSize()));
        _rubberBand->show();
        emit RubberBandSelection(true);
#endif
    }

    emit WantFocus();
}

void DrawArea::mouseMoveEvent(QMouseEvent* event)
{
#ifndef _VIEWER
    ShowCoordinates(event->pos());
#endif
    if ((event->buttons() & Qt::LeftButton) && _scribbling)
    {
        QPoint  dr = (event->pos() - _lastPointC);   // displacement vector
        if (!dr.manhattanLength())
            return;

        if (_spaceBarDown)
        {
            _ShiftOrigin(dr);
            _ClearCanvas();
            _Redraw();

            _lastPointC = event->pos();
        }
#ifndef _VIEWER
        else
        {
            if(_DrawLineTo(event->pos()) )
                _lastDrawnItem.add(_lastPointC - _topLeft);
        }
#endif
    }
#ifndef _VIEWER
    else if ((event->buttons() & Qt::RightButton) && _rubberBand)
    {
        QPoint pos = event->pos();
        _rubberBand->setGeometry(QRect(_rubber_origin, pos).normalized()); // means: top < bottom, left < right
    }
#endif
}

void DrawArea::wheelEvent(QWheelEvent* event)   // scroll the screen
{
    if (_pendown || _scribbling)
    {
        event->ignore();
        return;
    }

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
    if (event->button() == Qt::LeftButton && _scribbling) 
    {
        if (!_spaceBarDown)
        {
#ifndef _VIEWER
            _DrawLineTo(event->pos());

            _history.addDrawnItem(_lastDrawnItem);

            emit CanUndo(true);
            emit CanRedo(false);
#endif
        }
        else
            _RestoreCursor();

        _scribbling = false;
        _startSet = false;
    }
#ifndef _VIEWER
    else if (_rubberBand)
    {
        if (_rubberBand->geometry().width() > 10 && _rubberBand->geometry().height() > 10)
            _rubberRect = _rubberBand->geometry();
        else
            _RemoveRubberBand();
        event->accept();
    }
#endif
}

void DrawArea::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);             // show image on widget
    QRect dirtyRect = event->rect();
    painter.drawImage(dirtyRect, _background, dirtyRect);
    painter.drawImage(dirtyRect, _canvas, dirtyRect);
}

void DrawArea::resizeEvent(QResizeEvent* event)
{
    if (width() > _canvas.width() || height() > _canvas.height()) 
    {
        int newWidth =  qMax(width() + 128,  _canvas.width());
        int newHeight = qMax(height() + 128, _canvas.height());
        _ResizeImage(&_canvas, QSize(newWidth, newHeight), true);
        _ResizeImage(&_background, QSize(newWidth, newHeight), false);
        update();
    }
    QWidget::resizeEvent(event);

    _canvasRect = QRect(0, 0, geometry().width(), geometry().height() ).translated(-_topLeft);     // 0,0 relative rectangle
    _clippingRect = _canvasRect;
}

void DrawArea::tabletEvent(QTabletEvent* event)
{
    QTabletEvent::PointerType pointerT = event->pointerType();

    auto mb = event->buttons();

    switch (event->type()) 
    {
        case QEvent::TabletPress:
            if (event->pressure())   // message comes here for pen buttons even without the pen touching the tablet
            {
                if (!_pendown)
                {
                    if (_spaceBarDown)
                        _SaveCursorAndReplaceItWith(Qt::ClosedHandCursor);
                    _pendown = true;
                    emit PointerTypeChange(pointerT);
#ifndef _VIEWER
                    _InitiateDrawing(event);
#endif
                }
#ifndef _VIEWER
                if (_rubberBand)
                    _RemoveRubberBand();
#endif
            }
            event->accept();
            break;
    case QEvent::TabletMove:
#ifndef Q_OS_IOS
//        if (event->device() == QTabletEvent::RotationStylus)
//            updateCursor(event);
#endif
        if (_pendown)
        {
            static QPoint lastpos;
            static int counter; // only refresh screen when this is REFRESH_LIMIT
                                // because tablet events frequency is large
            const int REFRESH_LIMIT = 10;
            QPoint pos = event->pos();
            if (lastpos == pos)
            {
                event->accept();
                break;
            }
            else
            {
                lastpos = pos;
            }

            QPoint  dr = (pos - _lastPointC);   // displacement vector
            if (dr.manhattanLength())
            {
                if (_spaceBarDown)
                {
                    ++counter;
                    if (counter >= REFRESH_LIMIT)
                    {
                        counter = 0;
                        _ClearCanvas();
                        _Redraw();
                    }
                    _ShiftOrigin(dr);
                    _lastPointC = event->pos();
                }
#ifndef _VIEWER
                else
                {
                    if(_DrawLineTo(event->pos()) )
                        _lastDrawnItem.add(_lastPointC - _topLeft);
                }
#endif
            }
        }
        event->accept();
        break;
    case QEvent::TabletRelease:
        if (_pendown)// && event->buttons() == Qt::NoButton)
        {
            if (!_spaceBarDown)
            {
                _DrawLineTo(event->pos());

                _history.addDrawnItem(_lastDrawnItem);

                emit CanUndo(true);
                emit CanRedo(false);
            }
            else
                _RestoreCursor();
            _pendown = false;
            _startSet = false;
        }
        event->accept();
        break;
    default:
        event->ignore();
        break;
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
 * TASK:    starts drawing at position set by the event
 *          saves first point in _firstPointC, _lastPointC
 *          and _lastDrawnItem
 * PARAMS:  event - mouse or tablet event
 * GLOBALS: _spaceBarDown, _eraseMode, _lastDrawnItem,
 *          _actPenWidth, _actpenColor, _lastPoint, _topLeft
 * RETURNS:
 * REMARKS: -
 *-------------------------------------------------------*/
void DrawArea::_InitiateDrawing(QEvent* event)
{
    _firstPointC = _lastPointC = _pendown ? ((QTabletEvent*)event)->pos() : ((QMouseEvent*)event)->pos();

    if (_spaceBarDown)      // no drawing
        return;

    _lastDrawnItem.clear();
    if (_erasemode)
    {
        _lastDrawnItem.type = heEraser;
        _actPenWidth = _eraserWidth;
    }
    else
    {
        _lastDrawnItem.type = heScribble;
        _actPenWidth = _penWidth;
    }

    _lastDrawnItem.penKind = _myPenKind;
    _lastDrawnItem.penWidth = _actPenWidth;
    _lastDrawnItem.add(_lastPointC - _topLeft);
}


void DrawArea::_ModifyIfSpecialDirection(QPoint& qpC)
{       // when '_horizontal' is valid only keep the changes in coordinate in one direction
        // qpC canvas relative
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
 *          the next point is at list 5 points from the first one
 *          allows drawing in any other case
 * PARAMS:  newEndPoint is canvas relative
 * GLOBALS:
 * RETURNS: true : if is allowed to save 'newEndPointC'
 *          false: constrain requested but we do not know 
 *              yet in which direction
 * REMARKS: - Expects _lastDrawnItem to contain at least 
 *              two points
 *          - without contraint does not modify newEndPoint
 *              and always returns true
 *-------------------------------------------------------*/
bool DrawArea::_CanSavePoint(QPoint& newEndPointC)   // endPoint relative to canvas, not _topLeft
{
    if (!_startSet && _shiftKeyDown && (_pendown || _scribbling))
    {
        int x0 = _firstPointC.x(),    // relative to canvas
            y0 = _firstPointC.y(),
            x  = newEndPointC.x(),
            y  = newEndPointC.y(),
            dx = abs(x - x0),
            dy = abs(y0 - y);

        if (dx < 5 && dy < 5)
            return false;

        if (dx > dy)
            _isHorizontal = true;
        else if (dy > dx)
            _isHorizontal = false;
        _startSet = true;
        // replace most points keeping only the first, and the last

        _ModifyIfSpecialDirection(_lastPointC);
        _ModifyIfSpecialDirection(newEndPointC);
    }
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
void DrawArea::MoveToActualPosition(QRect rect)
{
    int l = -_topLeft.x(),
        t = -_topLeft.y();

    if (!rect.isNull() && rect.isValid() && !_canvasRect.intersects(rect)) // try to put at the middle of the screen
    {
        if (rect.x() < l || rect.x() > l + _canvasRect.width())
            l = rect.x() - (_canvasRect.width() - rect.width()) / 2;
        if (rect.y() < t || rect.y() > t + _canvasRect.height())
            t = rect.y() - (_canvasRect.height() - rect.height()) / 2;

        if (-l != _topLeft.x() || -t != _topLeft.y())
        {
            _SetOrigin(QPoint(-l, -t));
            _ClearCanvas();
            _Redraw();
        }
    }

}

/*========================================================
 * TASK:    Draw line from '_lastPointC' to 'endPointC'
 * PARAMS:  endpointC : clanvas relative coordinate
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
bool DrawArea::_DrawLineTo(QPoint endPointC)     // 'endPointC' canvas relative 
{
    bool result = true;
#ifndef _VIEWER
    if ( (result=_CanSavePoint(endPointC)) )     // i.e. must save point
    {
        _CorrectForDirection(endPointC); // when _startSet leaves only one coord moving
#endif
                // draw the line
        QPainter painter(&_canvas);
        painter.setPen(QPen(_PenColor(), _actPenWidth, Qt::SolidLine, Qt::RoundCap,
            Qt::RoundJoin));
        if (_erasemode)
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.drawLine(_lastPointC, endPointC);
        int rad = (_actPenWidth / 2) + 2;
        update(QRect(_lastPointC, endPointC).normalized()
            .adjusted(-rad, -rad, +rad, +rad));

        if (_topLeft.x()< _tlMax.x() || _topLeft.y() < _tlMax.y())     // last page of drawing
            _tlMax = _topLeft;
        // this should be in an other thread to work, else everything stops
//        if (!_pendown && !_scribbling && _delayedPlayback)
//            SleepFor(5ms);
#ifndef _VIEWER
    }
#endif
    _lastPointC = endPointC;
    return result;
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
    _history.clear();
    _ClearCanvas();
    _SetOrigin(QPoint() );  // new _topLeft and _canvasRect
    _tlMax = QPoint();

    emit CanUndo(false);
    emit CanRedo(false);
}

#ifndef _VIEWER
void DrawArea::Print()
{
#if QT_CONFIG(printdialog)
    QPrinter printer(QPrinter::HighResolution);

    QPrintDialog printDialog(&printer, this);
    //! [21] //! [22]
    if (printDialog.exec() == QDialog::Accepted) {
        QPainter painter(&printer);
        QRect rect = painter.viewport();
        QSize size = _canvas.size();
        size.scale(rect.size(), Qt::KeepAspectRatio);
        painter.setViewport(rect.x(), rect.y(), size.width(), size.height());
        painter.setWindow(_canvas.rect());
        painter.drawImage(0, 0, _canvas);
    }
#endif // QT_CONFIG(printdialog)
}
#endif
void DrawArea::_Redraw()
{
    int savewidth = _penWidth;
    MyPenKind savekind = _myPenKind;
    bool saveEraseMode = _erasemode;

    _history.SetFirstItemToDraw();
    while (_ReplotItem(_history.GetOneStep()))
        ;

    SetPenWidth(savewidth);
    SetPenKind(savekind);
    saveEraseMode = _erasemode;

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

bool DrawArea::_ReplotItem(HistoryItem* phi)
{
    if (!phi)
        return false;

    DrawnItem* pdrni;     // used when we must draw something onto the screen

    auto plot = [&]()   // lambda to plot point pointed by pdrni
                {
        if (pdrni->isDeleted)
            return;          
        if (_tlMax.x() > _topLeft.x() || _tlMax.y() > _topLeft.y())
            _tlMax = _topLeft;

        if (!pdrni->intersects(_clippingRect))   // if the clipping rectangle has no intersection with         
            return;                              // the scribble

        // DEBUG
// draw rectange around item to see if item intersects the rectangle
        //{
        //    QPainter painter(&_canvas);
        //    QRect rect = pdrni->rect.adjusted(-1,-1,0,0);   // 1px pen left and top:inside rectangle, put pen outside rectangle
        //    painter.setPen(QPen(QColor(qRgb(132,123,45)), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        //    painter.drawRect(rect);
        //}
        // /DEBUG            

        _lastPointC = pdrni->points[0] + _topLeft;
        _myPenKind = pdrni->penKind;
        _actPenWidth = pdrni->penWidth;
        _erasemode = pdrni->type == heEraser ? true : false;
        for (int i = 1; i < pdrni->points.size(); ++i)
            _DrawLineTo(pdrni->points[i] + _topLeft);

    };

    switch(phi->type)
    {
        case heScribble:
        case heEraser:    
            if( (pdrni = phi->GetDrawable(0)))
                plot();
            break;
        case heVisibleCleared:
            _ClearCanvas();
            break;
        case heItemsDeleted:    // nothing to do
            break;
        case heItemsPasted:
            pdrni = phi->GetDrawable();
            for (int i = 1; pdrni; ++i)
            {
                plot();
                pdrni = phi->GetDrawable(i);
            }
            break;
        default:
            break;
    }
                                    
    return true;
}

#ifndef _VIEWER
void DrawArea::Undo()               // must draw again all underlying scribbles
{
    if (_history.CanUndo())
    {
        QRect rect = _history.Undo();
        MoveToActualPosition(rect); 
//        _clippingRect = rect;
        _ClearCanvas();
        
        while(_ReplotItem(_history.GetOneStep()) )
            ;
        _clippingRect = _canvasRect;
        emit CanRedo(true);
    }
    emit CanUndo(_history.CanUndo());
}

void DrawArea::Redo()       // need only to draw undone items, need not redraw everything
{                           // unless top left or items color changed
    HistoryItem* phi = _history.Redo();
    if (!phi)
        return;

    MoveToActualPosition(phi->Area());
// ??    _clippingRect = phi->Area();

    if (phi->type == heRecolor)
        _Redraw();
    else if (phi->type == heItemsDeleted || heVertSpace)
    {
        _ClearCanvas();
        _Redraw();
    }
    else    // heScribble, heEraser
        _ReplotItem(phi);

    _clippingRect = _canvasRect;

    emit CanUndo(_history.CanUndo() );
    emit CanRedo(_history.CanRedo());
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

void DrawArea::_SetOrigin(QPoint o)
{
    _topLeft = o;
#ifndef _VIEWER
    _RemoveRubberBand();
#endif
    _canvasRect.moveTo(-_topLeft);
    _clippingRect = _canvasRect;

#ifndef _VIEWER
    ShowCoordinates(_topLeft);
#endif
}


void DrawArea::_ShiftOrigin(QPoint delta)    // delta changes _topLeft, negative delta.x: scroll right
{
    QPoint o = _topLeft;           // origin

    o += delta;                // calculate new origin
    if (o.x() > 0)
        o.setX(0);
    if (o.y() > 0)
        o.setY(0);

    _SetOrigin(o);
}
void DrawArea::_ShiftAndDisplayBy(QPoint delta)    // delta changes _topLeft, negative delta.x: scroll right
{
    _ShiftOrigin(delta);
    _ClearCanvas();
    _Redraw();
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
    QPoint pt = -_topLeft;

    if(!toTop)
        pt.setY(0);   // do not move in y direction
    _ShiftAndDisplayBy(pt);
}
void DrawArea::_End()
{
    _topLeft = _tlMax;
    _SetOrigin(_topLeft);
    _ClearCanvas();
    _Redraw();
}

void DrawArea::_Up(int amount)
{
    QPoint pt(0, amount);
    _ShiftAndDisplayBy(pt);
}
void DrawArea::_Down(int amount)
{
    QPoint pt(0, -amount);
    _ShiftAndDisplayBy(pt);
}
void DrawArea::_Left(int amount)
{
    QPoint pt(amount, 0);
    _ShiftAndDisplayBy(pt);
}
void DrawArea::_Right(int amount)
{
    QPoint pt(-amount, 0);
    _ShiftAndDisplayBy(pt);
}

#ifndef _VIEWER
void DrawArea::ShowCoordinates(const QPoint& qp)
{
    emit TextToToolbar(QString("x:%1, y:%2").arg(qp.x()).arg(qp.y()));
}
#endif
