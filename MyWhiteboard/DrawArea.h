#pragma once

#include <QColor>
#include <QImage>
#include <QBitmap>
#include <QPoint>
#include <QWidget>
#include <QTabletEvent>

#include "history.h"

// ******************************************************

class DrawArea : public QWidget
{
    Q_OBJECT

public:
        // cursors for drawing: arrow, cross for draing, opena and closed hand for moving, 
    enum CursorShape {csArrow, csCross, csOHand, csCHand, csPen, csEraser };

    DrawArea(QWidget* parent = nullptr);

//    void setTabletDevice(QTabletEvent* event)    {        updateCursor(event);    }
    void ClearBackground();

    int Load(QString name) 
    { 
        int res = _history.Load(name); 
        if (res >= 0)    // TODO send message if read error
        {
            _ClearCanvas();
            _Redraw();
        }
        _modified = false;
        emit CanUndo(true);
        emit CanRedo(false);
        return res;
    }
    bool Save(QString name) { return  _history.Save(name); }

    bool OpenBackgroundImage(const QString& fileName);
    bool SaveVisibleImage(const QString& fileName, const char* fileFormat);
    void SetBackgroundImage(QImage& image);

    void SetPenColor(MyPenKind newColor);
    void SetPenWidth(int newWidth);

    void SetOrigin() { _topLeft = QPoint(); }

    bool IsModified() const { return  _history.CanUndo() ? _modified : false; }
    MyPenKind PenKind() const { return _myPenKind;  }
    int PenWidth() const { return    _myPenWidth; }

signals:
    void CanUndo(bool state);     // state: true -> can undo
    void CanRedo (bool  state);   // state: true -> can redo
    void WantFocus();
    void PointerTypeChange(QTabletEvent::PointerType pt);

public slots:
    void NewData();
    void ClearCanvas();
    void ClearHistory();
    void Print();
    void Undo();
    void Redo();
    void SetCursor(CursorShape cs);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    void tabletEvent(QTabletEvent* event) override;

private:
    bool    _modified = false;
    bool    _scribbling = false;    // for mouse
    bool    _pendown = false;       // for pen
    bool    _erasemode = false;
    int     _myPenWidth = 1;
    MyPenKind _myPenKind = penBlack;
   
    QImage  _background,// an image for background layer
            _canvas;    // draw on this then show background and this on the widget
                        // origin: (0,0) point on canvas first shown
    bool    _isBackgroundSet = false;      // an image has been loaded into _background
    bool    _fixedBackground = true; // background will not scroll with image
    QPoint  _topLeft,   // actual top left of infinite canvas, relative to origin
            _lastPoint; // last point drawn relative to visible image
    DrawnItem _lastDrawnItem;
    History _history;

    void _InitiateDrawing(QEvent* event);

    void _ClearCanvas();

    void _DrawLineTo(const QPoint& endPoint);   // on _canvas then update
    void _ResizeImage(QImage* image, const QSize& newSize, bool isTransparent);

    bool _ReplotItem(const DrawnItem* pdrni);
    void _Redraw();
    QColor _PenColor() const
    {
        static MyPenKind _prevKind = penNone;
        static QColor color;
        if (_myPenKind == _prevKind)
            return color;

        _prevKind = _myPenKind;

        switch (_myPenKind)
        {
            default:
            case penBlack: return  color = QColor(Qt::black);
            case penRed: return    color = QColor(Qt::red);
            case penGreen: return  color = QColor(Qt::green);
            case penBlue: return   color = QColor(Qt::blue);
            case penEraser: return color = QColor(Qt::white);
        }
    }

};
