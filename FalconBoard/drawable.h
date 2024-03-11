#pragma once
#ifndef _DRAWABLE_H
	#define _DRAWABLE_H

#include <algorithm>
#include <limits>

#include <QApplication>
#include <QtGui>
#include <QFile>
#include <QDataStream>
#include <QIODevice>

#include "common.h"

enum HistEvent2 {
    heNone,
    // only drawable items are saved on disk
    heDrawable,        // series of points of scribbles or data for shapes (rectangle, circle, cross), and images or texts
    // these are not saved
    heRecolor,         // save old color, set new color
    heItemsDeleted,         // store the list of items deleted in this event
    heSpaceDeleted,         // empty space is deleted
    heVertSpace,            // insert vertical space
    heClearCanvas,          // visible image erased
    heBackgroundLoaded,     // an image loaded into _background
    heBackgroundUnloaded,   // image unloaded from background
    heItemsPastedBottom,// list of draw events added first drawn item is at index 'scribbleItem'
    heItemsPastedTop    // list of draw events added first drawn item is at index 'scribbleItem'
                        // last one is at index 'lastScribbleIndex'
                        // Undo: set _indexLastScribbleItem to that given in previous history item
                        // Redo: set _indexLastScribbleItem to 'lastScribbleIndex'
};
enum MyRotation {
    rotNone,
    rotR90, rotL90, rot180, rotFlipH, rotFlipV,     // rotate around the center of the bounding box, rotL90 => alpha == 90.0
    rotAlpha                                        // alpha: around the center of the bounding box, increases counterclockwise
};
enum MyLayer {
    mlyBackgroundImage,         // lowest layer: background image
    mlyDrawable,              // screenshot layer above background image
    mlyScribbleItem,            // scribbles rectangles, ellipses are drawn here
    mlySprite                   // top layer for sprites (moveable images)
};


enum DrawableType 
{
    dtNone,            // not yet set (invalid)
    dtCross,           // two 45º crossing lines, given by center coordinate and line length in penWidth units, may be filled
    dtEllipse,         // an ellipse given by center point and two axes, may be filled
    dtImage,           // a QImage. given by the top left coordinate and the index in the screenshot list in memory
    dtRectangle,       // A rectangle given by the top-left and bottom-right points
    dtScribble,        // a hand drawn line or a straight line
    dtText
};


            // ---------------- in memory images ---------------
struct ImageRead
{
    QPointF position;           // documentum relative (logical (0,0) of 'paper roll') (widget coord: topLeft + DrawArea::_topLeft is used) 
    QPixmap image;              // image from the disk or from screenshot (transparency included)
    bool isVisible = true;
    void Draw(QPainter& painter, QPointF topLeft)
    {
        painter.drawPixmap(position - topLeft, image);
    }

    QRectF Area() const { return QRectF(position, QSize(image.width(), image.height())); }
    // canvasRect relative to paper (0,0)
    // result: relative to image
    // isNull() true when no intersection with canvasRect ((0,0) relative!)
    QRectF AreaOnCanvas(const QRectF& canvasRect) const
    {
        return Area().intersected(canvasRect);
    }
    void Translate(QPointF p, int minY) // only if not deleted and top is > minY
    {
        if (position.y() > minY)
        {
            position += p;
        }
    }
};
inline QDataStream& operator<<(QDataStream& ofs, const ImageRead& bimg)
{
    ofs << bimg.position << bimg.image;
    return ofs;
}
inline QDataStream& operator>>(QDataStream& ifs, ImageRead& bimg)
{
    ifs >> bimg.position >> bimg.image;
    return ifs;
}

/*========================================================
 * A List of screenshots loaded for a history
 *-------------------------------------------------------*/
class  DrawableImageList : public  QList<ImageRead>
{
    int _index = -1;
public:
    void Add(QPixmap& image, QPoint pt) // pt: document relative position
    {
        ImageRead img;
        img.image = image;
        img.position = pt;
        push_back(img);
    }
    void Drop(int index) { /* TODO */ }   // removes from list without changing the index of the other images
    // canvasRect and result are relative to (0,0)
    QRect AreaOnCanvas(int index, const QRect& canvasRect) const;
    ImageRead* DrawableImageAt(int index) 
    {
        return &(*this)[index];
    }
    ImageRead* FirstVisible(const QRect& canvasRect)
    {
        _index = -1;
        return NextVisible();
    }
    ImageRead* NextVisible()
    {
        for (++_index; _index < size(); ++_index)
            if ((*this)[_index].isVisible)
                return DrawableImageAt(_index);
        _index = -1;
        return nullptr;
    }
    void Clear()
    {
        clear();
    }
    int ImageIndexFor(QPoint& p) const // -1: no such image else index in here
    {                                  // else image with largest zOrder
        for (int n = size()-1; n >= 0; --n)     // larger index, larger zOrder
            if ((*this)[n].isVisible && (*this)[n].Area().contains(p))
                return n;
        return -1;
    }
};


            // ---------------- common of all drawables ---------------
struct DrawParams
{
    FalconPenKind penKind = penBlackOrWhite;
    QColor  penColor = Qt::black;       // used for undefined pens
    int penWidth = 1;       // only for dtScribble
};
struct DrawableBase : public DrawParams
{
    DrawableBase() {};
    DrawableBase(const DrawableBase& db)
    {
        penKind = db.penKind;
        penColor = db.penColor;
        penWidth = db.penWidth;
    }
    ~DrawableBase() {};


    void SetParams(const DrawParams& params)
    {
        (DrawParams&)*this = params;
    }
    void SetupPainter(QPainter& painter, bool isFilled = false)
    {
        QPen pen(penColor);
        pen.setWidth(penWidth);
        _savedBrush = painter.brush();
        if (isFilled)
            painter.setBrush(penColor);
        painter.setPen(pen);
    }
    void ResetPainterBrush(QPainter& painter, bool isFilled = false)
    {
        painter.setBrush(_savedBrush);
    }
    virtual QRectF Area() const { return QRectF(); }
    virtual void Draw(QPainter& painter, QPointF& topLeft) {}
    virtual bool IsEmpty() const { return true; }
    virtual bool IsClosed() const { return false; }
private:
    QBrush _savedBrush;
};

inline QDataStream& operator<<(QDataStream& ofs, const DrawableBase& p)
{
    ofs << (int)p.penKind << p.penColor << p.penWidth;
    return ofs;
}
inline QDataStream& operator>>(QDataStream& ifs, DrawableBase& p)
{
    int n;
    ifs >> n; 
    p.penKind = (FalconPenKind)n;
    ifs >> p.penColor >> p.penWidth;
    return ifs;
}

            // ---------------- DrawableCross -------------
struct DrawableCross : public DrawableBase
{
    QRectF bndRect;

    // these to fullfill requirements for uses in QVariant
    DrawableCross() = default;
    ~DrawableCross() = default;
    DrawableCross(const DrawableCross&) = default;
    DrawableCross& operator=(const DrawableCross& o) 
    {
        *(DrawableBase*)this = *(DrawableBase*)(&o);
        bndRect = o.bndRect;         
    }
    // functions

    QRectF Area() const override
    {
        return bndRect;
    }

    void Draw(QPainter& painter, QPointF &topLeft) override
    {
        SetupPainter(painter);
        QRectF rect = bndRect.translated(-topLeft);
        painter.drawLine(rect.topLeft(),  rect.bottomRight());
        painter.drawLine(rect.topRight(), rect.bottomLeft());
    }
};
inline QDataStream& operator<<(QDataStream& ofs, const DrawableCross& c)
{
    ofs << reinterpret_cast<const DrawableBase&>(c) << c.bndRect;
    return ofs;
}
inline QDataStream& operator>>(QDataStream& ifs, DrawableCross& c)
{
    ifs >> reinterpret_cast<DrawableBase&>(c) >> c.bndRect;
    return ifs;
}

            // ---------------- DrawableEllipse -------------
struct DrawableEllipse : public DrawableBase
{
    QRectF bndRect;
    bool isFilled = false;  // only for closed polygons

    // these to fullfill requirements for uses in QVariant
    DrawableEllipse() = default;
    ~DrawableEllipse() = default;
    DrawableEllipse(const DrawableEllipse&) = default;
    DrawableEllipse& operator=(const DrawableEllipse&) = default;

    QRectF Area() const override
    {
        return bndRect;
    }

    void Draw(QPainter& painter, QPointF &topLeft)  override
    {
        SetupPainter(painter, isFilled);
        painter.drawEllipse(bndRect.translated(-topLeft));
        ResetPainterBrush(painter);
    }
};
inline QDataStream& operator<<(QDataStream& ofs, const DrawableEllipse& c)
{
    ofs << reinterpret_cast<const DrawableBase&>(c) << c.bndRect << c.isFilled;
    return ofs;
}
inline QDataStream& operator>>(QDataStream& ifs, DrawableEllipse& c)
{
    ifs >> reinterpret_cast<DrawableBase&>(c) >> c.bndRect >> c.isFilled;
    return ifs;
}

            // ---------------- DrawableImage  -------------
struct DrawableImage : public DrawableBase        // shown on layer lyDrawable below the drawings
{
    DrawableImageList* pList = nullptr;
    int itemIndex = -1;     // not saved to disk, set after read

    DrawableImage() = default;
    DrawableImage(DrawableImageList* plist) : pList(plist) {};
   ~DrawableImage() = default;
    DrawableImage(const DrawableImage&) = default;

    DrawableImage& operator=(const DrawableImage&) = default;

    QRectF Area() const override
    {
        return (*pList)[itemIndex].Area();
    }

    void Draw(QPainter& painter, QPointF &topLeft) override
    {
        (*pList)[itemIndex].Draw(painter, topLeft);
    }
};
      //  DrawableBase not used
inline QDataStream& operator<<(QDataStream& ofs, const DrawableImage& bimg)
{
    ofs << (*bimg.pList)[bimg.itemIndex];
    return ofs;
}
inline QDataStream& operator>>(QDataStream& ifs, DrawableImage& bimg)
{
    ImageRead img;
    ifs >> img;
    bimg.itemIndex = bimg.pList->size();
    bimg.pList->push_back(img);
    return ifs;
}

            // ---------------- DrawableRectangle  -------------
struct DrawableRectangle : public DrawableBase
{
    QRectF bndRect;
    bool isFilled = false;  // only for closed polygons

    // these to fullfill requirements for uses in QVariant
    DrawableRectangle() = default;
    DrawableRectangle(QRectF r, bool isFilled) : bndRect(r),isFilled(isFilled) { };
    ~DrawableRectangle() = default;
    DrawableRectangle(const DrawableRectangle&) = default;
    DrawableRectangle& operator=(const DrawableRectangle&) = default;

    QRectF Area() const override
    {
        return bndRect;
    }

    void Draw(QPainter& painter, QPointF &topLeft)         override
    {
        SetupPainter(painter, isFilled);
        painter.drawRect(bndRect.translated(-topLeft));
        ResetPainterBrush(painter);
    }
};

inline QDataStream& operator<<(QDataStream& ofs, const DrawableRectangle& c)
{
    ofs << reinterpret_cast<const DrawableBase&>(c) << c.isFilled << c.bndRect;
    return ofs;
}
inline QDataStream& operator>>(QDataStream& ifs, DrawableRectangle& c)
{
    ifs >> reinterpret_cast<DrawableBase&>(c) >> c.isFilled >> c.bndRect;
    return ifs;
}
            // ---------------- DrawablerScribble  -------------
struct DrawableScribble : public DrawableBase
{
    QPolygonF points;

    // these to fullfill requirements for uses in QVariant
    DrawableScribble() = default;
    ~DrawableScribble() = default;
    DrawableScribble(const DrawableScribble&) = default;
    DrawableScribble& operator=(const DrawableScribble&) = default;
        // functions
    void Add(QPointF p)
    {
        points.push_back(p);
    }
    void Add(qreal x, qreal y) { Add(QPoint(x, y)); };

    QRectF Area() const override
    {
        return points.boundingRect();
    }

    void Draw(QPainter& painter, QPointF &topLeft)   override
    {
        SetupPainter(painter);
        painter.drawPolyline(points.translated(-topLeft));  // not closed
    }
};
inline QDataStream& operator<<(QDataStream& ofs, const DrawableScribble& c)
{
    ofs << reinterpret_cast<const DrawableBase&>(c) << c.points;
    return ofs;
}
inline QDataStream& operator>>(QDataStream& ifs, DrawableScribble& c)
{
    ifs >> reinterpret_cast<DrawableBase&>(c) >> c.points;
    return ifs;
}

struct DrawableText : public DrawableBase
{
    QPointF position;
    QFont font;
    QString text;
    QRectF bndRect;     // determine ^set before calling Area()
            // these to fullfill requirements for uses in QVariant
    DrawableText() = default;
    ~DrawableText() = default;
    DrawableText(const DrawableText&) = default;
    DrawableText &operator=(const DrawableText&) = default;

    void SetFont(QFont fnt)
    {
        font = fnt;
    }

    QRectF Area() const override
    {
        return bndRect;
    }

    void Draw(QPainter& painter, QPointF &topLeft)   override
    {
        painter.setFont(font);
        painter.setPen(penColor);
        painter.drawText(position-topLeft, text);
    }
};

inline QDataStream& operator<<(QDataStream& ofs, const DrawableText& t)
{
    ofs << reinterpret_cast<const DrawableBase&>(t) << t.position << t.font << t.text;
    return ofs;
}
inline QDataStream& operator>>(QDataStream& ifs, DrawableText& t)
{
    ifs >> reinterpret_cast<DrawableBase&>(t) >> t.position >> t.font >> t.text;
    return ifs;
}

        // --------------------- drawable piece data ---------------

// a single, continuous piece of a drawable item, part of a Drawable
// it does not contain any coordinate or length all such data is stored
// in the points array of
class DrawablePiece
{
    DrawableBase* _pData;   // data stored in memory points to any descendants
                            // beware when copying or moving objects!
public:
    DrawableType dType = dtNone;

    DrawablePiece() : _pData(nullptr) {};
    DrawablePiece(const DrawableBase &data, DrawableType dType) : dType(dType)
    {
        DrawableCross      *pdc;
        DrawableEllipse    *pde;
        DrawableImage      *pdi;
        DrawableRectangle  *pdr;
        DrawableScribble   *pds;
        DrawableText       *pdt;

        switch (dType)
        {
            case dtCross:       pdc = new DrawableCross    (dynamic_cast<const DrawableCross    &>(data)); _pData = pdc; break;
            case dtEllipse:     pde = new DrawableEllipse  (dynamic_cast<const DrawableEllipse  &>(data)); _pData = pde; break;
            case dtImage :      pdi = new DrawableImage    (dynamic_cast<const DrawableImage    &>(data)); _pData = pdi; break;
            case dtRectangle:   pdr = new DrawableRectangle(dynamic_cast<const DrawableRectangle&>(data)); _pData = pdr; break;
            case dtScribble:    pds = new DrawableScribble (dynamic_cast<const DrawableScribble &>(data)); _pData = pds; break;
            case dtText :       pdt = new DrawableText     (dynamic_cast<const DrawableText     &>(data)); _pData = pdt; break;
        }
    };
    DrawablePiece(const DrawablePiece& other) : DrawablePiece(*other._pData, other.dType) {}
    DrawablePiece(DrawablePiece &&other) : _pData(other._pData),dType(other.dType) 
    {
        other._pData = nullptr;
    };
    //template<class T> DrawablePiece(const T& d, DrawableType type) : _pData(new T(d)), dType(type)
    //{
    //}

    DrawablePiece(const DrawableCross     &d) : _pData(new DrawableCross(d))    ,dType(dtCross) {}
    DrawablePiece(const DrawableEllipse   &d) : _pData(new DrawableEllipse(d))  ,dType(dtEllipse) {}
    DrawablePiece(const DrawableImage     &d) : _pData(new DrawableImage(d))    ,dType(dtImage) {}
    DrawablePiece(const DrawableRectangle &d) : _pData(new DrawableRectangle(d)),dType(dtRectangle) {}
    DrawablePiece(const DrawableScribble  &d) : _pData(new DrawableScribble(d)) ,dType(dtScribble) {}
    DrawablePiece(const DrawableText      &d) : _pData(new DrawableText(d))     ,dType(dtText) {}
    ~DrawablePiece()
    {
        delete _pData;
    }

    DrawablePiece& operator=(const DrawablePiece& d)
    {
        DrawablePiece pc(d);    //
        *this = std::move(pc);
        pc._pData = nullptr;
        return *this;
    }

    DrawablePiece& operator=(DrawablePiece&& d)
    {
        dType = d.dType;
        _pData = d._pData;

        d.dType = dtNone;
        d._pData = nullptr;
        return *this;
    }
    DrawableBase& operator*() {return *_pData; }

    template<class T> operator T() { return *dynamic_cast<T*>(_pData); }

    void Draw(QPainter& painter, QPointF& topLeft)
    {
        _pData->Draw(painter, topLeft);
    }
    bool IsEmpty() const
    {
        return _pData->IsEmpty();
    }
    bool IsClosed() const
    {
        return _pData->IsClosed();
    }
    QRectF Area() const
    {
        return _pData->Area();
    }
    friend QDataStream& operator<<(QDataStream& ofs, const DrawablePiece& pdt);
    friend QDataStream& operator>>(QDataStream& ifs, DrawablePiece& pdt);
};
inline QDataStream& operator<<(QDataStream& ofs, const DrawablePiece& pdt)
{
    ofs << (int)pdt.dType;
    switch (pdt.dType)
    {
        case dtCross:       ofs << *dynamic_cast<DrawableCross*>(pdt._pData);
            break;
        case dtEllipse:     ofs << *dynamic_cast<DrawableEllipse*>(pdt._pData);
            break;
        case dtImage:       ofs << *dynamic_cast<DrawableImage*>(pdt._pData);
            break;
        case dtRectangle:   ofs << *dynamic_cast<DrawableRectangle*>(pdt._pData);
            break;
        case dtScribble:    ofs << *dynamic_cast<DrawableScribble*>(pdt._pData);
            break;
        case dtText:        ofs << *dynamic_cast<DrawableText*>(pdt._pData);
            break;
        default:
            break;
    }
    return ofs;
}
inline QDataStream& operator>>(QDataStream& ifs, DrawablePiece& pdt)
{
    int n;
    ifs >> n;
    pdt.dType = (DrawableType)n;
    switch (pdt.dType)
    {
        case dtCross:       ifs >> *dynamic_cast<DrawableCross*>(pdt._pData);
            break;
        case dtEllipse:     ifs >> *dynamic_cast<DrawableEllipse*>(pdt._pData);
            break;
        case dtImage:       ifs >> *dynamic_cast<DrawableImage*>(pdt._pData);
            break;
        case dtRectangle:   ifs >> *dynamic_cast<DrawableRectangle*>(pdt._pData);
            break;
        case dtScribble:    ifs >> *dynamic_cast<DrawableScribble*>(pdt._pData);
            break;
        case dtText:        ifs >> *dynamic_cast<DrawableText*>(pdt._pData);
            break;
        default:
            break;
    }
    return ifs;
}


using DrawablePieceVector = QVector<DrawablePiece>;

/*=============================================================
 *  class for storing one drawable item
 *      it contains common data for all Drawablepiece-es
 *      a polygon (which is a QVectorF<QPointF> and need 
 *          not be closed, unless filled) with all points of 
 *          all pieces
 *          Coordinates are relative to logical origin 
 *                  (0,0) => canvas coord = points[i] - origin
 *------------------------------------------------------------*/

struct Drawable {
    HistEvent2 hType = heNone;

    bool isVisible = true;      // invisible drawables are not exported This property not saved on disk
    // save these data on disk in the .mwb file
    //  but only the relevan ones
    int zOrder = 0;         // for images >0 & < 
    DrawablePieceVector drawables;
        // for display
    MyRotation rot = rotNone;   // rotate around the center of the bounding box
    qreal alpha = 0.0;      // angle of counterclockwise rotation, may be 0 for flips H and V, <0 -> for anticlockwise
    QRectF bndRect;
public:
    Drawable(HistEvent2 he = heNone, int zorder = 0) noexcept : hType(he), zOrder(zorder) { }      // default constructor
    Drawable(const Drawable& di) = default;

    void Clear()       // clears points and pieces and sets type to heNone
    {
        drawables.clear();
        zOrder = 0;
        alpha = 0;
        rot = rotNone;
        hType = heNone;
        bndRect = QRectF();
    }
    void AddPiece(DrawablePiece& piece)             // piece's _pData pointer is set tu nullptr
    {
        bndRect = bndRect.united(piece.Area());
        drawables.push_back(piece);
    }

    void Reset()
    {
        Rotate(0.0);
    }
        // real rotation happens at display time
        // except for the bounding box
    void Rotate(qreal angle) 
    { 
        double eps = 0.001;
        if (abs(alpha - angle) < eps)
            return;

        alpha = angle; 
        double a = abs(alpha/360.0);
        a = ceil( (a - std::floor(a)) * 360.0 * 1000.0)/1000.0; // rounded to 3 decimal digits
        if (a > 180)
            a -= 360;
        alpha = alpha - 0 ? -a : a;

        if (alpha == 90.000)
            rot = rotL90;
        else if (alpha == -90.000)
            rot = rotR90;
        else if (alpha == 180.000 || alpha == -180.0)
            rot = rot180;
        else
            rot = rotAlpha;

        _SetupRotation(alpha);
    }
    void Rotate(MyRotation mr, qreal angle=0.0) 
    { 
        alpha = angle;
        switch (mr)
        {
            case rotR90:alpha = 90; break;
            case rotL90:alpha = -90; break;
            case rot180:alpha = 180; break;
            case rotAlpha:
            default:
                break;
        }
        _SetupRotation(alpha);
        _RotateBndRect();
    }

    void Draw(QPainter& painter, QPointF topLeft, qreal scale=1.0)
    {
        if (alpha != 0)
        {
            QTransform transform;
            transform.rotate(alpha);
            if (scale != 1.0)
                transform.scale(scale, scale);
            painter.setTransform(transform);
        }
        for (auto &a : drawables)
            a.Draw(painter, topLeft);
    }
    friend QDataStream& operator<<(QDataStream& ofs, const Drawable& drawable);
    friend QDataStream& operator>>(QDataStream& ifs, Drawable& d);
private:
    // do not save these on disk (reccreate on read)
    static int _topDrawableZOrder;      // default 0
    static int _topImageZOrder;         // default 0
    QTransform _transform;
    DrawableImageList _screenShotImageList;


    void _SetupRotation(qreal alpha)    // alpha in degrees and between -180º, + 180º
    {
        _transform.reset();
        _transform.rotate(alpha);
    }

    void _RotateBndRect()
    {
        switch (rot)
        {
            case rotNone:
            case rotFlipH:
            case rotFlipV:
                break;
            case rotL90:
            case rotR90:
                bndRect = bndRect.transposed();
                break;
            case rotAlpha:
                    bndRect = _transform.mapRect(bndRect);  // rotate the coordinates and return the bounding rectamgle
                break;
        }
        
    }
};
inline QDataStream& operator<<(QDataStream& ofs, const Drawable& drawable)
{
    if (drawable.isVisible && drawable.hType == heDrawable)
    {
        ofs << (int)drawable.hType;
        ofs << drawable.zOrder;
        ofs << (int)drawable.rot;
        ofs << drawable.alpha;
        ofs << drawable.drawables.size();
        for (auto &a : drawable.drawables)
            ofs << a;
    }
    return ofs;
}
inline QDataStream& operator>>(QDataStream& ifs, Drawable& d)
{
    if (ifs.atEnd())
        return ifs;

    int n;

    ifs >> d.hType;
    if (d.hType != heDrawable)
        throw("Type error");
    ifs >> d.zOrder;
    ifs >> n;
    d.rot = (MyRotation)n;
    ifs >> d.alpha;

    ifs >> n;  
    DrawablePiece dp;
    for (int i = 0; i < n; ++i)
    {
        ifs >> dp;
        n = d.drawables.size();
        if (dp.dType == dtImage)
        {
            DrawableImage dimg;
            ifs >> dimg;
        }
        d.drawables.push_back(dp);
    }



    return ifs;
}

using DrawableList = QList<Drawable>;


#endif
