#include "DrawArea.h"
#include "DrawArea.h"

#include <QMouseEvent>
#include <QPainter>

#if defined(QT_PRINTSUPPORT_LIB)
#include <QtPrintSupport/qtprintsupportglobal.h>
#if QT_CONFIG(printdialog)
#include <QPrinter>
#include <QPrintDialog>
#endif
#endif

DrawArea::DrawArea(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StaticContents);
    setAttribute(Qt::WA_TabletTracking);
    setCursor(Qt::CrossCursor);
}

void DrawArea::ClearCanvas()
{
    _history.push_back(DrawnItem(heVisibleCleared));
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
    int res = _history.Load(name);
    if (res >= 0)    // TODO send message if read error
    {
        _ClearCanvas();
        _Redraw();
    }
    emit CanUndo(true);
    emit CanRedo(false);
    return res;
}

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

void DrawArea::SetMode(bool darkMode, QString color)
{
    _backgroundColor = color;
    _background.fill(_backgroundColor);
    if (_isBackgroundSet)
            SetBackgroundImage(_loadedImage);
    _darkMode = darkMode;
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

void DrawArea::NewData()
{
    ClearHistory();
    ClearBackground();
}

void DrawArea::_ClearCanvas()
{
    _canvas.fill(qRgba(255,255,255, 0));     // transparent
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
        if (_rubberBand)    // felet rubberband for any keypress
        {
            if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
            {
                _RemoveRubberBand();
                if (_history.CollectItemsInside(_rubberRect.translated(-_topLeft)))
                {
                    _history.HideDeletedItems();
                    _ClearCanvas();
                    _Redraw();
                }
            }
            else
                _RemoveRubberBand();
        }
        else
        {
			if (event->key() == Qt::Key_PageUp)
				_PageUp();
			else  if (event->key() == Qt::Key_PageDown)
				_PageDown();
			else  if (event->key() == Qt::Key_Home)
				_Home();
			else if (event->key() == Qt::Key_Shift)
				_shiftKeyDown = true;
			if (event->key() == Qt::Key_Up)
				_Up();
			else if (event->key() == Qt::Key_Down)
				_Down();
			else if (event->key() == Qt::Key_Left)
				_Left();
			else if (event->key() == Qt::Key_Right)
				_Right();
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
            _SaveCursor(Qt::ClosedHandCursor);

        _scribbling = true;
        _InitiateDrawing(event);
    }
    else if (event->button() == Qt::RightButton)
    {
        _rubber_origin = event->pos();
        if (!_rubberBand)
            _rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
        _rubberBand->setGeometry(QRect(_rubber_origin, QSize()));
        _rubberBand->show();
    }

    emit WantFocus();
}

void DrawArea::mouseMoveEvent(QMouseEvent* event)
{
    ShowCoordinates(event->pos());
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
        else
        {
            if(_DrawLineTo(event->pos()) )
                _lastDrawnItem.add(_lastPointC - _topLeft);
        }
    }
    else if ((event->buttons() & Qt::RightButton) && _rubberBand)
    {
        QPoint pos = event->pos();
        _rubberBand->setGeometry(QRect(_rubber_origin, pos).normalized()); // means: top < bottom, left < right
    }
}

void DrawArea::wheelEvent(QWheelEvent* event)   // scroll the screen
{
    int y  = _topLeft.y();
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

    if ( dy > 10 || dy < -10 || dx > 10 || dx < -10)
    {
        _ShiftAndDisplay(QPoint(dx, dy));

        degv = degh = 0;
        dx = dy = 0;
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
            _DrawLineTo(event->pos());

            _history.push_back(_lastDrawnItem);

            emit CanUndo(true);
            emit CanRedo(false);
        }
        else
            _RestoreCursor();

        _scribbling = false;
        _startSet = false;
    }
    else if (_rubberBand)
    {
        if (_rubberBand->geometry().width() > 10 && _rubberBand->geometry().height() > 10)
            _rubberRect = _rubberBand->geometry();
        else
            _RemoveRubberBand();
        event->accept();
    }
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
}

void DrawArea::tabletEvent(QTabletEvent* event)
{
    QTabletEvent::PointerType pointerT = event->pointerType();
    switch (event->type()) 
    {
        case QEvent::TabletPress:
            if (!_pendown) 
            {
                if (_spaceBarDown)
                    _SaveCursor(Qt::ClosedHandCursor);
                _pendown = true;
                emit PointerTypeChange(pointerT);
                _InitiateDrawing(event);
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
                else
                {
                    if(_DrawLineTo(event->pos()) )
                        _lastDrawnItem.add(_lastPointC - _topLeft);
                }
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

                _history.push_back(_lastDrawnItem);

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

void DrawArea::_RemoveRubberBand()
{
    if (_rubberBand)
    {
        _rubberBand->hide();
        delete _rubberBand;
        _rubberBand = nullptr;
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
        _lastDrawnItem.histEvent = heEraser;
        _actPenWidth = _eraserWidth;
    }
    else
    {
        _lastDrawnItem.histEvent = heScribble;
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
    if (!_startSet && _shiftKeyDown)
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
    if (_startSet)  // then _lastDrawnItem only contains 2 points!
    {
        if (_isHorizontal)
            newpC.setY(_firstPointC.y());
        else
            newpC.setX(_firstPointC.x());
    }

    return newpC;
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
    bool result;
    if ( (result=_CanSavePoint(endPointC)) )     // i.e. must save point
    {
        _CorrectForDirection(endPointC); // when _startSet leaves only one coord moving
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
    }
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
    _topLeft = QPoint();
    emit CanUndo(false);
    emit CanRedo(false);
}

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

void DrawArea::_Redraw()
{
    _history.SetFirstItemToDraw();
    while (_ReplotItem(_history.GetOneStep()))
        ;
}

QColor DrawArea::_PenColor() const

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
    default:
    case penBlack: return  color = _darkMode ? QColor(Qt::white) : QColor(Qt::black);
    case penRed: return    color = QColor(Qt::red);
    case penGreen: return  color = QColor(Qt::green);
    case penBlue: return   color = QColor(Qt::blue);
    case penEraser: return color = QColor(Qt::white);
    }
}

void DrawArea::_SaveCursor(QCursor newCursor)
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

    static int lastItemDrawn = -1;  // used with undelete: print items until this index

    DrawnItem* pdrni;     // used when we must draw something onto the screen
    QRect r;                    // but only if it intersects with this

    auto plot = [&]()   // lambda to plot point pointed by pdrni
                {
        if (pdrni->isDeleted || !pdrni->intersects(r))    // if the canvas rectangle has no intersection with 
            return;                   // the scribble

        _lastPointC = pdrni->points[0] + _topLeft;
        _myPenKind = pdrni->penKind;
        _actPenWidth = pdrni->penWidth;
        _erasemode = pdrni->histEvent == heEraser ? true : false;
        for (int i = 1; i < pdrni->points.size(); ++i)
            _DrawLineTo(pdrni->points[i] + _topLeft);
    };

    switch(phi->histEvent)
    {
        case heScribble:
        case heEraser:    // else clear screen, move, etc
            r = QRect(0,0, geometry().width(), geometry().height() );
            r = r.translated(-_topLeft);
// DEBUG
//            r = r.marginsAdded(QMargins(200,200,200,200)); // ????
// /DEBUG            

            lastItemDrawn = phi->drawnIndex;    // saved for plotting undeleted items

            pdrni = _history.DrawableFor(*phi);

            plot();

            break;
        case heVisibleCleared:
            _ClearCanvas();
            break;
        case heItemsDeleted:    // delete and redraw all visible scribbles from the beginning without going through the HistoryItem List
            if (!phi->processed)
            {
                _history.HideDeletedItems(phi);
                _ClearCanvas();
                for (int i = _history.SetFirstItemToDraw(); i <= lastItemDrawn; ++i)
                {
                    pdrni = _history.DrawnItemAt(i);
                    plot();
                }
            }
            break;
        default:
            break;
    }

    return true;
}

void DrawArea::Undo()
{
    if (_history.CanUndo())
    {
        _ClearCanvas();
        _history.BeginUndo();  // undelete items deleted before the last item
        while(_ReplotItem(_history.GetOneStep()) )
            ;

        emit CanRedo(true);
    }
    emit CanUndo(_history.CanUndo());
}

void DrawArea::Redo()
{
    _ReplotItem (_history.Redo());

    emit CanUndo(_history.CanUndo() );
    emit CanRedo(_history.CanRedo());
}

void DrawArea::SetCursor(CursorShape cs)
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
        case csEraser: 
            {
                    QBitmap pbm(QString(":/MyWhiteboard/Resources/eraserpen.png"));
                    setCursor(QCursor(pbm, pbm));
                    _erasemode = true;
            }
            break;
        default:break;
    }
}

void DrawArea::_ShiftOrigin(QPoint delta)    // delta changes _topLeft, negative delta.x: scroll right
{
    QPoint o = _topLeft;           // origin

    o += delta;                // calculate new origin
    if (o.x() > 0)
        o.setX(0);
    if (o.y() > 0)
        o.setY(0);

    _topLeft = o;

    emit TextToToolbar(QString("top left: (%1, %2)").arg(_topLeft.x()).arg(_topLeft.y()));
}
void DrawArea::_ShiftAndDisplay(QPoint delta)    // delta changes _topLeft, negative delta.x: scroll right
{
    _ShiftOrigin(delta);
    _ClearCanvas();
    _Redraw();
}
void DrawArea::_PageUp()
{
    QPoint pt(0, geometry().height()/3*2);
    _ShiftAndDisplay(pt);
}
void DrawArea::_PageDown()
{
    QPoint pt(0, -geometry().height() / 3 * 2);
    _ShiftAndDisplay(pt);
}
void DrawArea::_Home()
{
    _ShiftAndDisplay(-_topLeft);
}
void DrawArea::_End()
{
    _ClearCanvas();
    _Redraw();
}

void DrawArea::_Up()
{
    QPoint pt(0, 10);
    _ShiftAndDisplay(pt);
}
void DrawArea::_Down()
{
    QPoint pt(0, -10);
    _ShiftAndDisplay(pt);
}
void DrawArea::_Left()
{
    QPoint pt(10, 0);
    _ShiftAndDisplay(pt);
}
void DrawArea::_Right()
{
    QPoint pt(-10, 0);
    _ShiftAndDisplay(pt);
}

void DrawArea::ShowCoordinates(QPoint& qp)
{
    emit TextToToolbar(QString("(%1, %2)").arg(qp.x()).arg(qp.y()));
}
