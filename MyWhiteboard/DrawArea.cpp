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
        _modified = false;
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
    else
        _modified = false;
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
    _modified = false;
}

void DrawArea::_ClearCanvas()
{
    _canvas.fill(qRgba(255,255,255, 0));     // transparent
    _modified = true;
    update();
}

void DrawArea::_InitiateDrawing(QEvent* event)
{
    _lastPoint = _pendown ? ((QTabletEvent*)event)->pos(): ((QMouseEvent*)event)->pos();
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
    _lastDrawnItem.points.push_back(_lastPoint);
}

void DrawArea::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !_pendown)  // even when using a pen some mouse message still appear
    {
        _scribbling = true;
        _InitiateDrawing(event);
    }
    emit WantFocus();
}

void DrawArea::mouseMoveEvent(QMouseEvent* event)
{
    if ((event->buttons() & Qt::LeftButton) && _scribbling)
    {
        _DrawLineTo(event->pos());
        _lastDrawnItem.points.push_back(_lastPoint+_topLeft);
    }
}

void DrawArea::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && _scribbling) {
        _DrawLineTo(event->pos());
        _scribbling = false;

        _history.push_back(_lastDrawnItem);

        emit CanUndo(true);
        emit CanRedo(false);
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
                _pendown = true;
                emit PointerTypeChange(pointerT);
                _InitiateDrawing(event);
            }
            break;
    case QEvent::TabletMove:
#ifndef Q_OS_IOS
//        if (event->device() == QTabletEvent::RotationStylus)
//            updateCursor(event);
#endif
        if (_pendown) 
        {
            _DrawLineTo(event->pos());
            _lastDrawnItem.points.push_back(_lastPoint+_topLeft);
        }
        break;
    case QEvent::TabletRelease:
        if (_pendown && event->buttons() == Qt::NoButton)
        {
            _pendown = false;
            _history.push_back(_lastDrawnItem);

            emit CanUndo(true);
            emit CanRedo(false);
        }
        update();
        break;
    default:
        break;
    }
    event->accept();
}

void DrawArea::_DrawLineTo(const QPoint& endPoint)
{
    QPainter painter(&_canvas);
    painter.setPen(QPen(_PenColor(), _actPenWidth, Qt::SolidLine, Qt::RoundCap,
        Qt::RoundJoin));
    if (_erasemode)
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.drawLine(_lastPoint, endPoint);
    _modified = true;

    int rad = (_actPenWidth / 2) + 2;
    update(QRect(_lastPoint, endPoint).normalized()
        .adjusted(-rad, -rad, +rad, +rad));
    _lastPoint = endPoint;
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

bool DrawArea::_ReplotItem(const DrawnItem* pdrni)
{
    if (!pdrni)
        return false;

    switch(pdrni->histEvent)
    {
        case heScribble:
        case heEraser:    // else clear screen, move, etc
            _lastPoint = pdrni->points[0] - _topLeft; 
            _myPenKind = pdrni->penKind;
            _actPenWidth = pdrni->penWidth;
            _erasemode = pdrni->histEvent == heEraser ? true : false;
            for (int i = 1; i < pdrni->points.size(); ++i)
                _DrawLineTo(pdrni->points[i]);
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

        _modified = true;
        emit CanRedo(true);
    }
    emit CanUndo(_history.CanUndo());
}

void DrawArea::Redo()
{
    _modified = true;
    _ReplotItem (_history.Redo());

    emit CanUndo(_history.CanUndo() );
    emit CanRedo(_history.CanRedo());
}

#include <QFileInfo>
void DrawArea::SetCursor(CursorShape cs)
{
    _erasemode = false;
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

