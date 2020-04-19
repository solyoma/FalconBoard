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

    int Load(QString name);
    bool Save(QString name) { return  _history.Save(name); }

    bool OpenBackgroundImage(const QString& fileName);
    bool SaveVisibleImage(const QString& fileName, const char* fileFormat);
    void SetBackgroundImage(QImage& image);

    void SetMode(bool darkMode, QString color);
    void SetBackgroundColor(QColor bck) { _backgroundColor = bck;  }    // light/ dark / black mode
    void SetPenKind(MyPenKind newKind);
    void SetPenWidth(int newWidth);
    void SetEraserWidth(int newWidth);

    void SetOrigin() { _topLeft = QPoint(); }

    bool IsModified() const { return  _history.CanUndo() ? _modified : false; }
    MyPenKind PenKind() const { return _myPenKind;  }
    int PenWidth() const { return    _actPenWidth; }

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
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    void tabletEvent(QTabletEvent* event) override;

private:
    bool    _modified = false;
    bool    _spaceBarDown = false;  // true when canvas is moved with the mouse or the pen
    bool    _scribbling = false;    // true for mouse darwing (_spaceBarDown == false)
    bool    _pendown = false;       // true for pen
    bool    _erasemode = false;
    int     _penWidth = 1;
    int     _eraserWidth = 30;
    int     _actPenWidth = 1;
    MyPenKind _myPenKind = penBlack;
   
    QImage  _background,// an image for background layer
            _canvas,    // draw on this then show background and this on the widget
                        // origin: (0,0) point on canvas first shown
            _loadedImage;   // background image loaded from disk
    bool    _isBackgroundSet = false;      // an image has been loaded into _background
    bool    _fixedBackground = true; // background will not scroll with image

    bool _darkMode = false;
    QColor _backgroundColor = Qt::white;

    QPoint  _topLeft,   // actual top left of infinite canvas, relative to origin  either 0 or negative values
            _lastPoint; // canvas relative last point drawn relative to visible image
    DrawnItem _lastDrawnItem;
    QCursor _savedCursor;
    bool _cursorSaved = false;
    History _history;

    void _InitiateDrawing(QEvent* event);

    void _ClearCanvas();

    void _DrawLineTo(const QPoint& endPoint);   // on _canvas then update
    void _ResizeImage(QImage* image, const QSize& newSize, bool isTransparent);

    bool _ReplotItem(const DrawnItem* pdrni);
    void _Redraw();
    QColor _PenColor() const;
    void _SaveCursor(QCursor newCursor);
    void _RestoreCursor();

    void _ShiftOrigin(QPoint delta);    // delta changes _topLeft, negative delta.x: scroll right
    void _ShiftAndDisplay(QPoint delta);
    void _PageUp();
    void _PageDown();
    void _Home();
    void _End();
    void _Up();
    void _Down();
    void _Left();
    void _Right();
};
