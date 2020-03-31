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

void DrawArea::ClearArea()
{
    _history.push_back(DrawnItem(heVisibleCleared));
    ClearImage();
}

void DrawArea::ClearBackground()
{
    _background.fill(qRgb(255, 255, 255));
}

bool DrawArea::OpenBackgroundImage(const QString& fileName)
{
    QImage loadedImage;
    if (!loadedImage.load(fileName))
        return false;

    QSize newSize = loadedImage.size().expandedTo(size());
    _ResizeImage(&loadedImage, newSize);
    _background = loadedImage;
    if (!_background.isNull())
        _Redraw();
    else
        _modified = false;
    update();
    return true;
}

bool DrawArea::SaveVisibleImage(const QString& fileName, const char* fileFormat)
{
    QImage visibleImage = _image;           
    _ResizeImage(&visibleImage, size());

    if (visibleImage.save(fileName, fileFormat)) {
        _modified = false;
        return true;
    }
    return false;
}

void DrawArea::SetPenColor(const QColor& newColor)
{
    _myPenColor = newColor;
}

void DrawArea::SetPenWidth(int newWidth)
{
    _myPenWidth = newWidth;
}

void DrawArea::NewData()
{
    ClearHistory();
    ClearBackground();
    _modified = false;
}

void DrawArea::ClearImage()
{
    _image.fill(qRgb(255, 255, 255));
    _modified = true;
    update();
}

void DrawArea::_DrawBackground()
{
    if (!_background.isNull()) 
    { 
        _image = _background; update(); 
    }
}

void DrawArea::_InitiateDrawing(QEvent* event)
{
    _lastPoint = _pendown ? ((QTabletEvent*)event)->pos(): ((QMouseEvent*)event)->pos();
    _lastDrawnItem.clear();
    _lastDrawnItem.penColor = _myPenColor;
    _lastDrawnItem.penWidth = _myPenWidth;
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
    QPainter painter(this);
    QRect dirtyRect = event->rect();
    painter.drawImage(dirtyRect, _image, dirtyRect);
}

void DrawArea::resizeEvent(QResizeEvent* event)
{
    if (width() > _image.width() || height() > _image.height()) {
        int newWidth =  qMax(width() + 128,  _image.width());
        int newHeight = qMax(height() + 128, _image.height());
        _ResizeImage(&_image, QSize(newWidth, newHeight));
        update();
    }
    QWidget::resizeEvent(event);
}

void DrawArea::tabletEvent(QTabletEvent* event)
{
    switch (event->type()) 
    {
        case QEvent::TabletPress:
            if (!_pendown) 
            {
                _pendown = true;
                _InitiateDrawing(event);
                emit PointerTypeChange(event->pointerType());
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
    QPainter painter(&_image);
    painter.setPen(QPen(_myPenColor, _myPenWidth, Qt::SolidLine, Qt::RoundCap,
        Qt::RoundJoin));
    painter.drawLine(_lastPoint, endPoint);
    _modified = true;

    int rad = (_myPenWidth / 2) + 2;
    update(QRect(_lastPoint, endPoint).normalized()
        .adjusted(-rad, -rad, +rad, +rad));
    _lastPoint = endPoint;
}

void DrawArea::_ResizeImage(QImage* image, const QSize& newSize)
{
    if (image->size() == newSize)
        return;

    QImage newImage(newSize, QImage::Format_RGB32);
    newImage.fill(qRgb(255, 255, 255));
    QPainter painter(&newImage);
    painter.drawImage(QPoint(0, 0), *image);
    *image = newImage;
}

void DrawArea::ClearHistory()
{
    _history.clear();
    ClearImage();
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
        QSize size = _image.size();
        size.scale(rect.size(), Qt::KeepAspectRatio);
        painter.setViewport(rect.x(), rect.y(), size.width(), size.height());
        painter.setWindow(_image.rect());
        painter.drawImage(0, 0, _image);
    }
#endif // QT_CONFIG(printdialog)
}

void DrawArea::_Redraw()
{
    if(!_background.isNull())
        _image = _background;

    _history.SetFirstItemToDraw();
    while (_ReplotItem(_history.GetOneStep()))
        ;
}

bool DrawArea::_ReplotItem(const DrawnItem* pdrni)
{
    if (!pdrni)
        return false;

    if (pdrni->points.size())    // else clear screen
    {
        _lastPoint = pdrni->points[0] - _topLeft; 
        _myPenColor = pdrni->penColor;
        _myPenWidth = pdrni->penWidth;
        for (int i = 1; i < pdrni->points.size(); ++i)
            _DrawLineTo(pdrni->points[i]);
    }
    else
        ClearImage();

    return true;
}

void DrawArea::Undo()
{
    ClearImage();
    if (_history.CanUndo())
    {
        if (!_background.isNull())
            _image = _background;
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

#include <QFileInfo>
void DrawArea::SetCursor(CursorShape cs)
{
    switch (cs)
    {
        case csArrow: setCursor(Qt::ArrowCursor); break;
        case csOHand: setCursor(Qt::OpenHandCursor); break;
        case csCHand: setCursor(Qt::ClosedHandCursor); break;
        case csPen:   setCursor(Qt::CrossCursor); break;
        case csEraser: 
            {
                    QBitmap pbm(QString(":/MyWhiteboard/Resources/eraserpen.png"));
                    setCursor(QCursor(pbm, pbm)); break;
            }
        default:break;
    }
}
