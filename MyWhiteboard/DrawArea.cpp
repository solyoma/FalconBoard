#include "DrawArea.h"
#include "DrawArea.h"
#include "DrawArea.h"

#include <QApplication>
#include <QMouseEvent>

#include <QPainter>
#include <QPrinter>

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
    _history.pImages = &_belowImages;
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
	drawColors[penBlue] = "#82dbfc";
	drawColors[penYellow] = Qt::yellow;
}

void DrawArea::ClearRoll()
{
#ifndef _VIEWER
    _RemoveRubberBand();
#endif
    _history.addClearRoll();
    _ClearCanvas();
}

void DrawArea::ClearVisibleScreen()
{
#ifndef _VIEWER
    _RemoveRubberBand();
#endif
    _history.addClearVisibleScreen();
    _ClearCanvas();
}

void DrawArea::ClearDown()
{
#ifndef _VIEWER
    _RemoveRubberBand();
#endif
    _history.addClearDown();
    _ClearCanvas();
}

void DrawArea::ClearBackground()
{
    _background = QImage();
    _isBackgroundSet = false;
    update();
}

int DrawArea::Load(QString name)
{
    _belowImages.clear();

    int res = _history.Load(name);
    QString qs = name.mid(name.lastIndexOf('/')+1);
    if (!res)
        QMessageBox::about(this, tr(WindowTitle), QString( tr("'%1'\nInvalid file")).arg(qs));
    else if(res == -1)
        QMessageBox::about(this, tr(WindowTitle), QString(tr("File\n'%1'\n not found")).arg(qs));
    else if(res < 0)
        QMessageBox::about(this, tr(WindowTitle), QString( tr("File read problem. %1 records read. Please save the file to correct this error")).arg(-res-1));
    if(res && res != -1)    // TODO send message if read error
    {
        _topLeft = QPoint(0, 0);
        _Redraw();
    }
    emit CanUndo(true);
    emit CanRedo(false);
    update();
    return res;
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

void DrawArea::AddScreenShotImage(QImage& image)
{
    ScreenShotImage bimg;
    bimg.image = image;
    int x = (geometry().width() -  image.width()) / 2,
        y = (geometry().height() - image.height()) / 2;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    bimg.topLeft = QPoint(x, y) + _topLeft;
    _belowImages.push_back(bimg);
    _history.addScreenShot(_belowImages.size()-1);
    emit CanUndo(true);
    emit CanRedo(false);
}

#ifndef _VIEWER
void DrawArea::InsertVertSpace()
{
    _RemoveRubberBand();
    _history.addInsertVertSpace(_rubberRect.y() + _topLeft.y(), _rubberRect.height());
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
    if(!_history.SelectedSize())
        _history.CollectItemsInside(_rubberRect.translated(_topLeft));

    MyPenKind pk = PenKindFromKey(key);
    HistoryItem* phi = _history.addRecolor(pk);
//    _RemoveRubberBand();
    if (phi)
        _Redraw();
    return true;
}
#endif

void DrawArea::NewData()
{
    ClearHistory();
    ClearBackground();
    _erasemode = false; // previous last operation mightr be an erease
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
    SetPenKind(pk);
}
#endif

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
                 bRecolor = (key == Qt::Key_1 || key == Qt::Key_2 || key == Qt::Key_3 || key == Qt::Key_4 || key == Qt::Key_5),

                 bRotate  = (key == Qt::Key_0 ||  // rotate right by 90 degrees
                             key == Qt::Key_8 ||  // rotate by 180 degrees
                             key == Qt::Key_9 ||  // rotate left by 90 degrees
                             key == Qt::Key_H ||  // flip horizontally
                             (key == Qt::Key_V && !mods)       // flip vertically when no modifier keys pressed
                            );

            if (bDelete || bCopy || bRecolor || bRotate)
            {
                if ((bCollected = _history.SelectedSize()) && !bDelete && !bRotate)
                {
                    _history.CopySelected();
                    _scribblesCopied = true;        // never remove selected list
                }
            }
                // if !bCollected then history's _selectedList is empty, but the rubberRect is set into _selectionRect
            if ((bCut || bDelete) && bCollected)
            {
                _history.addDeleteItems();
                _Redraw();
            }
            else if (bDelete && !bCollected)     // delete on empty area
            {
                if (_history.addRemoveSpaceItem(_rubberRect))     // there was something (not delete below the last item
                {
                    _Redraw();
                }
            }

			if (bPaste)
			{           // _history's copied item list is valid, each item is canvas relative
                        // get offset to top left of encompassing rect of copied items relative to '_topLeft'
				QPoint dr = _rubberRect.translated(_topLeft).topLeft(); 

                HistoryItem* phi = _history.addPastedItems(dr);
                if(phi)
				    _ReplotItem(phi);

                emit CanUndo(true);
                emit CanRedo(false);
			}
            if (bRecolor)
            {
                ChangePenColorByKeyboard(key);
                RecolorSelected(key);
            }

            if (bRotate && bCollected)
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
                    _history.addRotationItem(rot);
                    _Redraw();
					emit CanUndo(true);
					emit CanRedo(false);
                }
            }

            if (key == Qt::Key_R)    // draw rectangle with margin or w,o, margin with Shift
            {
                _actPenWidth = _penWidth;
                _lastDrawnItem.clear();
                _lastDrawnItem.type = heScribble;

                int margin = /* !mods.testFlag(Qt::ShiftModifier)*/ _history.SelectedSize() ? 3*_actPenWidth : 0;

                int x1, y1, x2, y2, x3, y3, x4, y4;
                x1 = _rubberRect.x()-margin;                y1 = _rubberRect.y() - margin;
                x2 = x1 + _rubberRect.width() + 2 * margin; y2 = y1;
                x3 = x2;                                    y3 = y1 + _rubberRect.height() + 2 * margin;
                x4 = x1;                                    y4 = y3;

                _firstPointC = _lastPointC = QPoint(x1, y1);
                _lastDrawnItem.add(QPoint(x1,y1) + _topLeft);
                _DrawLineTo(QPoint(x2, y2));
                _lastDrawnItem.add(QPoint(x2, y2) + _topLeft);
                _DrawLineTo(QPoint(x3, y3));
                _lastDrawnItem.add(QPoint(x3, y3) + _topLeft);
                _DrawLineTo(QPoint(x4, y4));
                _lastDrawnItem.add(QPoint(x4, y4) + _topLeft);
                _DrawLineTo(QPoint(x1, y1));
                _lastDrawnItem.add(QPoint(x1, y1) + _topLeft);

                _lastDrawnItem.penKind = _myPenKind;
                _lastDrawnItem.penWidth = _actPenWidth;

                _history.addDrawnItem(_lastDrawnItem);

                emit CanUndo(true);
                emit CanRedo(false);
            }
            if (bRemove)
                _RemoveRubberBand();

        }
        else
#endif
        {
            _altKeyDown = event->modifiers().testFlag(Qt::AltModifier);

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
                _Up(mods.testFlag(Qt::ControlModifier) ? 100 : 20);
            else if (key == Qt::Key_Down)
                _Down(mods.testFlag(Qt::ControlModifier) ? 100 : 20);
            else if (key == Qt::Key_Left)
                _Left(mods.testFlag(Qt::ControlModifier) ? 100 : 20);
            else if (key == Qt::Key_Right)
                _Right(mods.testFlag(Qt::ControlModifier) ? 100 : 20);
            else if(key == Qt::Key_BracketRight )
                emit IncreaseBrushSize(1);
            else if(key == Qt::Key_BracketLeft )
                emit DecreaseBrushSize(1);
#ifndef _VIEWER
            else if (key == Qt::Key_1 || key == Qt::Key_2 || key == Qt::Key_3 || key == Qt::Key_4 || key == Qt::Key_5)
                ChangePenColorByKeyboard(key);
#endif
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
    if(event->key() == Qt::Key_Shift)
        _shiftKeyDown = false;

    _startSet = false;
    _altKeyDown = event->key() == Qt::Key_Alt;
    QWidget::keyReleaseEvent(event);
}

void DrawArea::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !_pendown)  // even when using a pen some mouse messages still appear
    {
        if (_spaceBarDown)
            _SaveCursorAndReplaceItWith(Qt::ClosedHandCursor);

        _scribbling = true;
#ifndef _VIEWER
        if (_rubberBand )
        {
            if (_history.SelectedSize() &&  _rubberRect.contains(event->pos()))         
            {
                if (_CreateSprite(event->pos(), _rubberRect))
                {
                    _history.addDeleteItems(_pSprite);
                    _Redraw();
//                    QApplication::processEvents();
                }
                
            }

            _RemoveRubberBand();
        }

        _altKeyDown = event->modifiers().testFlag(Qt::AltModifier);

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
#else
    _lastPointC = event->pos();     // used for moving the canvas around
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

#ifndef _VIEWER
        if (_pSprite)     // move sprite
        {
            MoveSprite(dr);
            _lastPointC = event->pos();
        }
        else
        {
#endif
            if (_spaceBarDown)
            {
                _ShiftOrigin(dr);
                _Redraw();

                _lastPointC = event->pos();
            }
#ifndef _VIEWER
            else
            {
                if (_DrawFreehandLineTo(event->pos()))
                    _lastDrawnItem.add(_lastPointC + _topLeft);
            }
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
                _DrawFreehandLineTo(event->pos());

                _history.addDrawnItem(_lastDrawnItem);

                emit CanUndo(true);
                emit CanRedo(false);
#endif
            }
            else
                _RestoreCursor();
#ifndef _VIEWER
        }
#endif
        _scribbling = false;
        _startSet = false;
    }
#ifndef _VIEWER
    else if (_rubberBand)
    {
        if (_rubberBand->geometry().width() > 10 && _rubberBand->geometry().height() > 10)
        {
            _rubberRect = _rubberBand->geometry();
            bool b = _history.CollectItemsInside(_rubberRect.translated(_topLeft));
            if (_history.SelectedSize())
            {
                _rubberBand->setGeometry(_history.BoundingRect().translated(-_topLeft));
                _rubberRect = _rubberBand->geometry(); // _history.BoundingRect().translated(-_topLeft);
            }
        }
        else
            _RemoveRubberBand();
        event->accept();
    }
#endif
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
#ifndef _VIEWER
                if (_rubberBand)
                {
                    if (_history.SelectedSize() && _rubberRect.contains(event->pos()))
                    {
                        if (_CreateSprite(event->pos(), _rubberRect))
                        {
                            _history.addDeleteItems(_pSprite);
                            _Redraw();
        //                    QApplication::processEvents();
                        }

                    }
                }
                _RemoveRubberBand();
#endif
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

#ifndef _VIEWER
                    if (_pSprite)     // move sprite
                    {
                        MoveSprite(dr);
                        _lastPointC = event->pos();
                    }
                    else
                    {
#endif
                        if (_spaceBarDown)
                        {
                            ++counter;
                            if (counter >= REFRESH_LIMIT)
                            {
                                counter = 0;
                                _Redraw();
                            }
                            _ShiftOrigin(dr);
                            _lastPointC = event->pos();
                        }
#ifndef _VIEWER
                        else
                        {
                            if (_DrawFreehandLineTo(event->pos()))
                                _lastDrawnItem.add(_lastPointC + _topLeft);
                        }
                    }
#endif
                }
            }
            event->accept();
            break;
        case QEvent::TabletRelease:
            if (_pendown)// && event->buttons() == Qt::NoButton)
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
                        _DrawFreehandLineTo(event->pos());

                        _history.addDrawnItem(_lastDrawnItem);

                        emit CanUndo(true);
                        emit CanRedo(false);
                    }
                    else
                        _RestoreCursor();
#ifndef _VIEWER
                }
#endif
                _pendown = false;
                _startSet = false;
            }
            emit PointerTypeChange(QTabletEvent::Cursor);
            event->accept();
            break;
        default:
            event->ignore();
            break;
    }
}

void DrawArea::_DrawGrid(QPainter& painter)
{
    if (!_bGridOn)
        return;
    int x, y;
    if (_gridIsFixed)
    {
        x = _nGridSpacing; y = _nGridSpacing;
    }
    else
    {
        x = _nGridSpacing - (_topLeft.x() % _nGridSpacing);
        y = _nGridSpacing - (_topLeft.y() % _nGridSpacing);
    }

    painter.setPen(QPen(_gridColor, 2, Qt::SolidLine));
    for (; y <= height(); y += _nGridSpacing)
        painter.drawLine(0, y, width(), y);
    for (; x <= width(); x += _nGridSpacing)
        painter.drawLine(x, 0, x, height());
}

void DrawArea::_DrawPageGuides(QPainter& painter)
{
    if (!_bPageGuidesOn)
        return;

    int sh = (int)(std::round(_pageHeight / _magn));     // screen height of printed page

    int nxp = _topLeft.x() / _screenWidth,
        nyp = _topLeft.y() / sh,
        nx  = (_topLeft.x() + width())/_screenWidth, 
        ny  = (_topLeft.y() + height())/sh,
        x=height(), y;

    painter.setPen(QPen(_pageGuideColor, 2, Qt::DotLine));
    if (nx - nxp > 0)     // x - guide
        for(x = nxp + 1 * _screenWidth - _topLeft.x(); x < width(); x += _screenWidth )
                painter.drawLine(x, 0, x, height());
    if (ny - nyp > 0)    // y - guide
        for (y = nyp + 1 * sh - _topLeft.y(); y < height(); y += sh)
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
 *                          screenshots
 *                  bottom  background image if any
 *          - paint these in bottom to top order on this widget                  
 *-------------------------------------------------------*/
void DrawArea::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);             // show image on widget
    QRect dirtyRect = event->rect();
    painter.fillRect(dirtyRect, _backgroundColor);             // draw on here
    if(!_background.isNull())                                  // bottom : background layer
        painter.drawImage(dirtyRect, _background, dirtyRect);
    if (_bGridOn)
        _DrawGrid(painter);
            // images below drawing
    QRect r = dirtyRect.translated(_topLeft);           // screen -> absolute coord
    ScreenShotImage* pimg = _belowImages.FirstVisible(r);    // pimg intersects r
    while (pimg)                                               // image layer
    {
        QRect intersectRect = pimg->Area(r);      // absolute
        painter.drawImage(intersectRect.translated(-_topLeft), pimg->image, intersectRect.translated(-pimg->topLeft) );
        pimg = _belowImages.NextVisible();
    }
            // top of these the drawing
    painter.drawImage(dirtyRect, _canvas, dirtyRect);          // canvas layer
    if (_bPageGuidesOn)
        _DrawPageGuides(painter);

    if(_pSprite)
        painter.drawImage(dirtyRect.translated(_pSprite->topLeft), _pSprite->image, dirtyRect);  // sprite layer: dirtyRect: actual area below sprite 
}

void DrawArea::resizeEvent(QResizeEvent* event)
{
    if (width() > _canvas.width() || height() > _canvas.height()) 
    {
        int newWidth =  qMax(width() + 128,  _canvas.width());
        int newHeight = qMax(height() + 128, _canvas.height());
        _ResizeImage(&_canvas, QSize(newWidth, newHeight), true);
        _history.SetClippingRect(QRect(_topLeft, QSize(newWidth, newHeight)));
        update();
    }
    QWidget::resizeEvent(event);

    _canvasRect = QRect(0, 0, geometry().width(), geometry().height() ).translated(_topLeft);     // 0,0 relative rectangle
    _clippingRect = _canvasRect;
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
    _lastDrawnItem.add(_lastPointC + _topLeft);
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

        if (dx < 4 && dy < 4)
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
void DrawArea::MoveToActualPosition(QRect rect)
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
    painter.setPen(QPen(_PenColor(), _actPenWidth, Qt::SolidLine, Qt::RoundCap,
        Qt::RoundJoin));
    if (_erasemode)
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
    else
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.drawLine(_lastPointC, endPointC);
    int rad = (_actPenWidth / 2) + 2;
    update(QRect(_lastPointC, endPointC).normalized()
        .adjusted(-rad, -rad, +rad, +rad));

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
    _SetOrigin(QPoint() );  // new _topLeft and _canvasRect

    emit CanUndo(false);
    emit CanRedo(false);
}

#ifndef _VIEWER
void DrawArea::PageSetup()      // public slot
{
    int w[] = {3840,
               3440,
               2560,
               2560,
               2048,
               1920,
               1920,
               1680,
               1600,
               1536,
               1440,
               1366,
               1360,
               1280,
               1280,
               1280,
               1024,
               800 ,
               640
    },
        h[] = {2160,
               1440 ,
               1440 ,
               1080 ,
               1152 ,
               1200 ,
               1080 ,
               1050 ,
               900  ,
               864  ,
               900  ,
               768  ,
               768  ,
               1024 ,
               800  ,
               720  ,
               768  ,
               600  ,
               360
    };

    float fact[] = {1.0, 2.54, 25.4};   // inch, cm, mm

    PageSetupDialog* ps = new PageSetupDialog(this, _printerName);

    if (ps->exec())
    {
        #define SQUARE(a)  (a*a)
        _screenHeight = h[ps->resolutionIndex];
        _screenWidth = w[ps->resolutionIndex];
        float cosine = (float)_screenWidth / (float)std::sqrt(SQUARE(_screenWidth) + SQUARE(_screenHeight));
        _ppi = (float)(ps->screenDiagonal) *  fact[ps->unitFactor] * cosine / _screenWidth;
        _portrait  = ps->orientation;        
        _printerName = ps->actPrinter;
        QPrinter printer;
        printer.setPrinterName(_printerName);
        printer.setOrientation(_portrait ? QPrinter::Portrait : QPrinter::Landscape);

        QPrinterInfo info(printer);

        QPageSize pageSize = info.defaultPageSize();
        _pageWidth = pageSize.sizePixels(printer.resolution()).width();
        _pageHeight= pageSize.sizePixels(printer.resolution()).height();
        _magn = (float)_pageWidth / (float)_screenWidth;
    }
    delete ps;
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
#endif
void DrawArea::_Redraw()
{
    int savewidth = _penWidth;
    MyPenKind savekind = _myPenKind;
    bool saveEraseMode = _erasemode;

    _ClearCanvas();
    if (_history.SetFirstItemToDraw() >= 0)
    {
        while (_ReplotItem(_history.GetOneStep()))
            ;
    }
    SetPenWidth(savewidth);
    SetPenKind(savekind);
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

bool DrawArea::_ReplotItem(HistoryItem* phi)
{
    if (!phi || phi->TopLeft().y() > (_topLeft.y() + _clippingRect.height()))
        return false;

    DrawnItem* pdrni;     // used when we must draw something onto the screen

    auto plot = [&]()   // lambda to plot point pointed by pdrni
                {
        if (!pdrni->isVisible || !pdrni->intersects(_clippingRect))   // if the clipping rectangle has no intersection with         
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

        _lastPointC = pdrni->points[0] - _topLeft;
        _myPenKind = pdrni->penKind;
        _actPenWidth = pdrni->penWidth;
        _erasemode = pdrni->type == heEraser ? true : false;
        for (int i = 1; i < pdrni->points.size(); ++i)
            _DrawLineTo(pdrni->points[i] - _topLeft);

    };

    switch(phi->type)
    {
        case heScribble:
        case heEraser:    
            if( (pdrni = phi->GetDrawable(0)))
                plot();
            break;
        case heItemsDeleted:    // nothing to do
            break;
        case heItemsPastedTop:
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
        _RemoveRubberBand();

        QRect rect = _history.Undo();
        MoveToActualPosition(rect); 
//        _clippingRect = rect;
        _ClearCanvas();

        _Redraw();
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

    _RemoveRubberBand();

    MoveToActualPosition(phi->Area());
// ??    _clippingRect = phi->Area();

    _Redraw();

    _clippingRect = _canvasRect;

    emit CanUndo(_history.CanUndo() );
    emit CanRedo(_history.CanRedo());
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
    _history.SetClippingRect(_canvasRect);

#ifndef _VIEWER
    ShowCoordinates(_topLeft);
#endif
}

void DrawArea::_ShiftOrigin(QPoint delta)    // delta changes _topLeft, positive delta.x: scroll right
{
    QPoint o = _topLeft;       // origin of screen top left relative to "paper"

    o -= delta;                // calculate new screen origin
    if (o.x() < 0)
        o.setX(0);
    if (o.y() < 0)
        o.setY(0);

    _SetOrigin(o);
}
void DrawArea::_ShiftAndDisplayBy(QPoint delta)    // delta changes _topLeft, negative delta.x: scroll right
{
    _ShiftOrigin(delta);
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
    QPoint pt = _topLeft;

    if(!toTop)
        pt.setY(0);   // do not move in y direction
    _ShiftAndDisplayBy(pt);
}
void DrawArea::_End()
{
    _topLeft = _history.BottomRightVisible();
    _SetOrigin(_topLeft);
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
    QPoint pt(amount,0);
        _ShiftAndDisplayBy(pt);
}
void DrawArea::_Right(int amount)
{
    QPoint pt(-amount,0);
        _ShiftAndDisplayBy(pt);
}

#ifndef _VIEWER
void DrawArea::ShowCoordinates(const QPoint& qp)
{
    emit TextToToolbar(QString("x:%1, y:%2").arg(qp.x()).arg(qp.y()));
}

// ****************************** Sprite ******************************

/*========================================================
 * TASK:    Create a new sprite using lists of selected 
 *          items and images
 * PARAMS:  event position and _rubberRect position; both
 *          relative to _topLeft
 * GLOBALS: _topLeft, _pSprite
 * RETURNS: _pSprite
 * REMARKS: - _nSelectedList is used to create the other lists
 *            which leaves previously copied items intact
 *              so paste operation still pastes the non sprite
 *              data
 *-------------------------------------------------------*/
Sprite* DrawArea::_CreateSprite(QPoint pos, QRect &rect)
{
    _pSprite = new Sprite(&_history);       // copies selected items into lists
    _pSprite->topLeft = rect.topLeft();     // sprite top left
    _pSprite->dp = pos - rect.topLeft();    // cursor position rel. to sprite
    _pSprite->image = QImage(rect.width(), rect.height(), QImage::Format_ARGB32);
    _pSprite->image.fill(qRgba(255, 255, 255, 0));     // transparent

    QPainter painter(&_pSprite->image);
    QRect sr, tr;   // source & target
    for (ScreenShotImage& si : _pSprite->images)
    {
        tr = QRect(si.topLeft, _pSprite->rect.size() );        // (0,0, width, height)
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


    for (auto& di : _pSprite->items)
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
    if (_erasemode)
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setPen(QPen((_darkMode ? Qt::white : Qt::black), 2, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin)) ;
    painter.drawLine(0, 0, _pSprite->rect.width(), 0);
    painter.drawLine(_pSprite->rect.width(), 0, _pSprite->rect.width(), _pSprite->rect.height());
    painter.drawLine(_pSprite->rect.width(), _pSprite->rect.height(), 0, _pSprite->rect.height());
    painter.drawLine(0, _pSprite->rect.height(), 0, 0);

    // restore data
    _myPenKind = pk;
    _actPenWidth = pw;
    _erasemode = em;

    return _pSprite;
}

void DrawArea::MoveSprite(QPoint dr)
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
    _history.addPastedItems(ps->topLeft + _topLeft, ps);    // add at window relative position: top left = (0,0)
    QRect updateRect = ps->rect.translated(ps->topLeft);    // original rectangle
    update(updateRect);
    delete ps;
}
#endif
