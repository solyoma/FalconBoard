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
}

void DrawArea::ClearArea()
{
    _history.push_back(DrawnItem());
    clearImage();
}

bool DrawArea::openImage(const QString& fileName)
{
    QImage loadedImage;
    if (!loadedImage.load(fileName))
        return false;

    QSize newSize = loadedImage.size().expandedTo(size());
    resizeImage(&loadedImage, newSize);
    _image = loadedImage;
    _modified = false;
    update();
    return true;
}

bool DrawArea::saveImage(const QString& fileName, const char* fileFormat)
{
    QImage visibleImage = _image;
    resizeImage(&visibleImage, size());

    if (visibleImage.save(fileName, fileFormat)) {
        _modified = false;
        return true;
    }
    return false;
}

void DrawArea::setPenColor(const QColor& newColor)
{
    _myPenColor = newColor;
}

void DrawArea::setPenWidth(int newWidth)
{
    _myPenWidth = newWidth;
}

void DrawArea::clearImage()
{
    _image.fill(qRgb(255, 255, 255));
    _modified = true;
    update();
}

void DrawArea::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        _lastPoint = event->pos();
        _scribbling = true;
        _lastDrawnItem.clear();
        _lastDrawnItem.penColor = _myPenColor;
        _lastDrawnItem.penWidth = _myPenWidth;
        _lastDrawnItem.points.push_back(_lastPoint);
    }
}

void DrawArea::mouseMoveEvent(QMouseEvent* event)
{
    if ((event->buttons() & Qt::LeftButton) && _scribbling)
    {
        drawLineTo(event->pos());
        _lastDrawnItem.points.push_back(_lastPoint);
    }
}

void DrawArea::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && _scribbling) {
        drawLineTo(event->pos());
        _scribbling = false;

        _history.push_back(_lastDrawnItem);

        emit canUndo(true);
        emit canRedo(false);
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
        resizeImage(&_image, QSize(newWidth, newHeight));
        update();
    }
    QWidget::resizeEvent(event);
}

void DrawArea::drawLineTo(const QPoint& endPoint)
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

void DrawArea::resizeImage(QImage* image, const QSize& newSize)
{
    if (image->size() == newSize)
        return;

    QImage newImage(newSize, QImage::Format_RGB32);
    newImage.fill(qRgb(255, 255, 255));
    QPainter painter(&newImage);
    painter.drawImage(QPoint(0, 0), *image);
    *image = newImage;
}

void DrawArea::clearHistory()
{
    _history.clear();
    emit canUndo(false);
    emit canRedo(false);
}

void DrawArea::print()
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

void DrawArea::Redraw(DrawnItem& item)
{
    setPenColor(item.penColor);
    setPenWidth(item.penWidth);

}

bool DrawArea::_ReplotItem(const DrawnItem* pdrni)
{
    if (!pdrni)
        return false;

    if (pdrni->points.size())    // else clear screen
    {
        _lastPoint = pdrni->points[0];
        _myPenColor = pdrni->penColor;
        _myPenWidth = pdrni->penWidth;
        for (int i = 1; i < pdrni->points.size(); ++i)
            drawLineTo(pdrni->points[i]);
    }
    else
        clearImage();

    return true;
}

void DrawArea::Undo()
{
    clearImage();
    if (_history.CanUndo())
    {
        _history.BeginUndo();
        const DrawnItem* pdrni;
        while(_ReplotItem(_history.UndoOneStep()) )
            ;

        emit canRedo(true);
    }
    emit canUndo(_history.CanUndo());
}

void DrawArea::Redo()
{
    _ReplotItem (_history.Redo());

    emit canUndo(_history.CanUndo() );
    emit canRedo(_history.CanRedo());
}
