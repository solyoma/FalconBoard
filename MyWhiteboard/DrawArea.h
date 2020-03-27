#pragma once

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QWidget>

#include "history.h"

// ******************************************************

class DrawArea : public QWidget
{
    Q_OBJECT

public:
    DrawArea(QWidget* parent = nullptr);
    void ClearArea();

    bool openImage(const QString& fileName);
    bool saveImage(const QString& fileName, const char* fileFormat);
    void setPenColor(const QColor& newColor);
    void setPenWidth(int newWidth);

    bool isModified() const { return _modified; }
    QColor penColor() const { return _myPenColor; }
    int penWidth() const { return    _myPenWidth; }

signals:
    void canUndo(bool state);     // state: true -> can undo
    void canRedo (bool  state);   // state: true -> can redo
    void wantFocus();

public slots:
    void clearImage();
    void clearHistory();
    void print();
    void Redraw(DrawnItem& item);
    void Undo();
    void Redo();


protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void drawLineTo(const QPoint& endPoint);
    void resizeImage(QImage* image, const QSize& newSize);

    bool _ReplotItem(const DrawnItem* pdrni);

    bool    _modified = false;
    bool    _scribbling = false;
    int     _myPenWidth = 1;
    QColor  _myPenColor = Qt::blue;
    QImage  _image;
    QPoint  _lastPoint;
    DrawnItem _lastDrawnItem;
    History _history;
};
