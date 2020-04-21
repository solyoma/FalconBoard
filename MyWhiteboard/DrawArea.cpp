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
        if (!event->isAutoRepeat())
        {
            if (event->key() == Qt::Key_PageUp)
                _PageUp();
            else  if (event->key() == Qt::Key_PageDown)
                _PageDown();
            else  if (event->key() == Qt::Key_Home)
                _Home();
            else if (event->key() == Qt::Key_Shift)
                _shiftKeyDown = true;
        }
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
    emit WantFocus();
}

void DrawArea::mouseMoveEvent(QMouseEvent* event)
{
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
            _DrawLineTo(event->pos());
            if (_startSet)
                _lastDrawnItem.points[1] = _lastPointC - _topLeft;
            else
                _lastDrawnItem.add(_lastPointC - _topLeft);
        }
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

// DEBUG
qDebug("ended: count (%d)", _lastDrawnItem.points.size());
for(int i = 0; i < _lastDrawnItem.points.size(); ++i)
    qDebug(" %d - (%d,%d)", i, _lastDrawnItem.points[i].x(), _lastDrawnItem.points[i].y() );

            emit CanUndo(true);
            emit CanRedo(false);
        }
        else
            _RestoreCursor();

        _scribbling = false;
        _startSet = false;
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
                    _DrawLineTo(event->pos());
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

    // DEBUG
    qDebug("Initiate drawing. First point: (%d, %d)", _firstPointC.x(), _firstPointC.y());

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
        qDebug("_ModifyIfSpecialDirection  qp: (%d, %d) %s", qpC.x(), qpC.y(), (_isHorizontal ? "Horiz" : "Vert"));
        if (_isHorizontal)
            qpC.setY(_firstPointC.y());
        else
            qpC.setX(_firstPointC.x());
        qDebug("             modified:     qp': (%d, %d)", qpC.x(), qpC.y());
    }
}


/*========================================================
 * TASK:    When movement is constrained to be horizontal 
 *          or vertical 
 * PARAMS:  newEndPoint is canvas relative
 * GLOBALS:
 * RETURNS: true : if is allowed to save 'newEndPointC'
 *          false: constrain requested but we do not know 
 *              yet in which direction
 * REMARKS: - Expects _lastDrawnItem to contain at least 
 *              two points
 *          - when movement is constrained leave only
 *              two points in _lastDrawnItem
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

        if (dx > dy && dy < 5)
            _isHorizontal = true;
        else if (dy > dx && dx < 5)
            _isHorizontal = false;
        _startSet = true;
        // replace most points keeping only the first, and the last

        _ModifyIfSpecialDirection(_lastPointC);
        _ModifyIfSpecialDirection(newEndPointC);
// DEBUG
        qDebug("_CanSavePoint");
        for (int i = 0; i < _lastDrawnItem.points.size(); ++i)
            qDebug(" %d - (%d,%d)", i, _lastDrawnItem.points[i].x(), _lastDrawnItem.points[i].y());
// /DEBUG
        _lastDrawnItem.points.clear();
        _lastDrawnItem.points.push_back(_firstPointC - _topLeft);              // no other points are necessary
        _lastDrawnItem.points.push_back(newEndPointC - _topLeft);    // no other points are necessary
    }
// DEBUG
    if(!_startSet)
        qDebug("_CanSavePoint allows drawing");
// /DEBUG
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

// DEBUG
        if (_lastDrawnItem.points.size() < 2)
            qDebug("Error: _lastDrawnItem size is 1");
else
// /DEBUG            
            // there are only two elements in _lastDrawnItem
        _lastDrawnItem.points[1] = _lastPointC - _topLeft;    // no other points are necessary
    }
    return newpC;
}

/*========================================================
 * TASK:    Draw line from '_lastPointC' to 'endPointC'
 * PARAMS:  endpointC : clanvas relative coordinate
 * GLOBALS:
 * RETURNS: nothing
 * REMARKS: - save _lastPointC before calling this function
 *              during user drawing
 *          - sets the new actual canvas relative position
 *            in '_lastPointC
 *          - does modify _endPointC if the shift key 
 *              is pressed when drawing
 *          - when _shiftKeyDown 
 *              does not draw the point until a direction 
 *                  was established
 *              if direction is established modifies 
 *                  _lastDrawnItem.points[1]
 *-------------------------------------------------------*/
void DrawArea::_DrawLineTo(QPoint endPointC)     // 'endPointC' canvas relative 
{
    if (_CanSavePoint(endPointC))     // i.e. must save point
    {
        _CorrectForDirection(endPointC); // when _startSet leaves only one coord moving
                // draw the line
        QPainter painter(&_canvas);
        painter.setPen(QPen(_PenColor(), _actPenWidth, Qt::SolidLine, Qt::RoundCap,
            Qt::RoundJoin));
        if (_erasemode)
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.drawLine(_lastPointC, endPointC);
// DEBUG
        qDebug("DrawLine: (%d, %d) => (%d, %d)", _lastPointC.x(), _lastPointC.y(), endPointC.x(), endPointC.y());
// /DEBUG
        int rad = (_actPenWidth / 2) + 2;
        update(QRect(_lastPointC, endPointC).normalized()
            .adjusted(-rad, -rad, +rad, +rad));
    }
    _lastPointC = endPointC;
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

bool DrawArea::_ReplotItem(const DrawnItem* pdrni)
{
    if (!pdrni)
        return false;

    QRect r;
    switch(pdrni->histEvent)
    {
        case heScribble:
        case heEraser:    // else clear screen, move, etc
            r = geometry();
            r = r.translated(-_topLeft).marginsAdded(QMargins(r.width(), r.height(), r.width(),r.height()));
            if (!pdrni->intersects(r))    // the canvas rectangle has no intersection with 
                break;                    // the scribble

            _lastPointC = pdrni->points[0] + _topLeft; 
            _myPenKind = pdrni->penKind;
            _actPenWidth = pdrni->penWidth;
            _erasemode = pdrni->histEvent == heEraser ? true : false;
            for (int i = 1; i < pdrni->points.size(); ++i)
                _DrawLineTo(pdrni->points[i] + _topLeft);
            break;
        case heVisibleCleared:
            _ClearCanvas();
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
        _history.BeginUndo();
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
