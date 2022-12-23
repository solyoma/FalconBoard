#pragma once

#ifndef _DRAWABLES_H
#define _DRAWABLES_H


#include <algorithm>
#include <limits>

#include <QApplication>
#include <QtGui>
#include <QFile>
#include <QDataStream>
#include <QIODevice>

#include "common.h"

#include "quadtree.h"

// for versions 1.x
enum class HistEventV1 {
    heNone_v1,
            // these are types that were saved on disk for version 1.X
    heScribble,        // series of points from start to finish of scribble
    heEraser,          // eraser used
    heScreenShot,
    heText,
};


enum class HistEvent {
    heNone,
    heDrawable,             // these can be saved on disk, when they are visible
        // these are not saved
    heBackgroundLoaded,     // an image loaded into _background
    heBackgroundUnloaded,   // image unloaded from background
    heClearCanvas,          // visible image erased
    heEraserStroke,         // eraser stroke added
    heItemsDeleted,         // store the list of items deleted in this event
    heItemsPastedBottom,// list of draw events added first drawn item is at index 'drawableItem'
    heItemsPastedTop,    // list of draw events added first drawn item is at index 'drawableItem'
                        // last one is at index 'lastScribbleIndex'
                        // Undo: set _indexLastScribbleItem to that given in previous history item
                        // Redo: set _indexLastScribbleItem to 'lastScribbleIndex'
    heRecolor,              // save old color, set new color
    heRotation,             // items rotated
    hePenWidthChange,        // change pen widths for all drawables selected
    heSpaceDeleted,         // empty space is deleted
    heVertSpace,            // insert vertical space
                };

enum class DrawableType {
    dtNone,
    dtCross,            // an X as a marker
    dtDot,              // single dot
    dtEllipse,          // 
    dtLine,             // 2 endpoints
    dtRectangle,        // 4 courner points
    dtScreenShot,       // shown below any other drawable
    dtScribble,         // series of points from start to finish of scribble, eraser strokes added to  scribble
    dtText
};
 // determined from the number of points
enum ScribbleSubType { sstScribble,     // any number
                       sstLine,         // only two points
                       sstQuadrangle     // 4 points, where the last and first are equal
                     };    
enum MyLayer { 
                mlyBackgroundImage,         // lowest layer: background image
                mlyScreenShot,              // screenshot layer: may write over it
                mlyScribbleItem,            // scribbles are drawn here
                mlySprite                   // layer for sprites (moveable images)
             };

#if !defined _VIEWER && defined _DEBUG
extern bool isDebugMode;
#endif
            //----------------------------------------------------
            // ------------------- helpers -------------
            //----------------------------------------------------
QRectF QuadAreaToArea(const QuadArea& qarea);
QuadArea AreaForItem(const int& i);

qreal Round(qreal number, int dec_digits);
bool IsItemsEqual(const int& i1, const int& i2);
QuadArea AreaForQRect(QRectF rect);

static bool __IsLineNearToPoint(QPointF p1, QPointF p2, QPointF& ccenter, qreal r)   // line between p2 and p1 is inside circle w. radius r around point 'ccenter'
{
#define SQR(x)  ((x)*(x))
#define DIST2(a,b) (SQR((a).x() - (b).x()) + SQR((a).y() - (b).y()))
    // simple checks:
    if (DIST2(p1, ccenter) > r && DIST2(p2, ccenter) > r && DIST2(p1,p2) < 2*r)
        return false;

    // first transform coord system origin to center of circle 
    p1 -= ccenter;
    p2 -= ccenter;
    // then solve the equations for a line given by p1 and p2 to intersect a circle w. radius r
    qreal dx = p2.x() - p1.x(),
        dy = p2.y() - p1.y(),
        dr2 = SQR(dx) + SQR(dy),
        D = p1.x() * p2.y() - p2.x() * p1.y(),
        discr = SQR(r) * dr2 - SQR(D),
        pdr2 = 1 / dr2;

    if (discr < 0)   // then the line does not intersect the circle
        return false;
	else if (EqZero(discr) )  // floating point, no abs. 0 test
	{                       // tangent line. Check tangent point.
		qreal xm = (D * dy) * pdr2,
			ym = -(D * dx) * pdr2;
		return (SQR(xm) + SQR(ym) <= SQR(r));
	}
	else    // one or two intersections with the line, but is there any for the line section?
	{
        
        if (SQR(p1.x()) + SQR(p1.y()) <= SQR(r) || SQR(p2.x()) + SQR(p2.y()) <= SQR(r))
            return true;    // at least one of the endpoints is inside the circle

        // not so easy... Get intersection points ip1,ip2
        qreal sqrt_discr = sqrt(discr),
            xm1 = (D * dy) * pdr2,
            xm2 = (dx * sqrt_discr * (dy < 0 ? -1 : 1)) * pdr2,
            ym1 = -(D * dx) * pdr2,
            ym2 = abs(dy) * sqrt_discr * pdr2;
        // the two intersection points are:
        QPointF ip1 = QPointF(xm1 + xm2, ym1 + ym2),
                ip2 = QPointF(xm1 - xm2, ym1 - ym2);

        // here neither of the end points is inside the circle and
        // the 4 points are on the same line (or less than 'eps' distance from it)
        // If the points are ordered by their x or y coordinates
        // for intersection the order of points on the line must be 
        // p1 -> ip1 -> ip2 -> p2         
        if(EqZero(abs(p2.y() - p1.y()) ) )      // vertical line
        {
            // order points by y coordinate
            if (p1.y() > p2.y())
                std::swap<QPointF>(p1, p2);
            if (ip1.y() > ip2.y())
                std::swap<QPointF>(ip1, ip2);

            return  (p1.y() <= ip1.y() && ip2.y() <= p2.y());
        }
        // non-vertical line: order points by x coordinate
        if (p1.x() > p2.x())
            std::swap<QPointF>(p1, p2);
        if (ip1.x() > ip2.x())
            std::swap<QPointF>(ip1, ip2);

        return p1.x() <= ip1.x() && ip2.x() <= p2.x();
    }
#undef SQR
#undef DIST2
};

            //----------------------------------------------------
            // ------------------- MyRotation::Type -------------
            //----------------------------------------------------
// Rotations and flips are not kommutative. However a flip followed by a rotation
// with an angle is the same as a rotation of -1 times the anngle followed by the same
// flip. each flip is its own inverse and two different consecutevie flips are
// equivalent to a rotation with 180 degrees 
struct MyRotation
{
    enum Type 
    {
        flipNone,                    // when there's no flip
        rotR90, rotL90, rot180,     // only used to set angle, but never used as 'flipType'
        rotFlipH, rotFlipV         // these leave the top left corner in place
    };

    qreal angle = 0.0;        // (in degrees) rotate by this (absolute for ellipse and rectangle) relative to prev. state to others
    Type flipType = flipNone; // after rotated flip by this (may only be a flip or flipNone for no flip)

    static constexpr qreal AngleForType(Type myrot)
    {
        qreal alpha = 0.0;
        switch (myrot)
        {
            case rotL90: alpha = -90; break;	// top left -> bottom left
            case rotR90: alpha =  90; break;
            case rot180: alpha = 180; break;
            default:                        // for no rotation and flips
                break;
        }
        return alpha;
    }
    static constexpr Type TypeForAngle(qreal angle)
    {
        if (!angle)
            return flipNone;
        if (angle == 90.0)
            return rotR90;
        if (angle == -90.0 || angle == 270.0)
            return rotL90;
        if (angle == -180.0 || angle == 180.0)
            return rot180;
    }
    static constexpr inline bool IsFlipType(Type typ)
    {
        return typ == flipNone || typ == rotFlipH || typ == rotFlipV;
    }
    inline constexpr bool IsPureRotation() const
    {
        return flipType == flipNone;
    }
    inline bool HasSimpleRotation() const
    {
        return abs(fmod(angle, 90.0)) < eps;
    }
    inline bool HasNoRotation() const
    {
        return fabs(angle) < eps;
    }

    inline void Reset() { angle = 0.0; flipType = flipNone; }

    MyRotation& operator=(Type typ) // sets angle and _tr too !
    {
        _SetRot((angle=AngleForType(typ)));
        
        if(IsFlipType(typ))
            flipType = typ;
        return *this;
    }
    MyRotation& operator=(qreal anAngle)    // sets flipType and _tr too !
    {
        _SetRot(anAngle);
        flipType = flipNone;
        return *this;
    }
      // when adding rotations the rotation added is either
      // a pure rotation by an angle or a pure flip
      // but never come these together!
    void AddAngle(qreal anAngle)        // sets _tr too
    {
        if (flipType != flipNone)
            anAngle = -anAngle;         // flip + angle = -angle + flip

        _SetRot(angle + anAngle);
    }

    void AddType(Type typ)
    {
        if (!IsFlipType(typ))   // then rotation
            _SetRot(angle + AngleForType(typ) );
        else                    // then flip or no flip
        {
            if (flipType == flipNone)     // none + flip = flip
                flipType = typ;
            else if(typ != flipNone)      // flip1 + flip2 =   +0 rotation and flipNone for flip1==flip2
            {                             //               = +180 rotation and flipnone for flip1 != flip2
                if(flipType != typ)
                    _SetRot(angle + 180);
                flipType = flipNone;
            }
        }
    }

    // (flipNone, angle) = (angle, flipNone)
    // (  flip, angle)   = (-angle, flip)

    // (angle, flip)     + (aangle, flipNone) = (angle + aangle,  flip)
    // (angle, flipNone) + (    0,   aflip) = (angle,           aflip)
    // (angle, flip)     + (    0,   aflip) = (angle + (flip != aflip ? 180:0, flip + aflip)
    // (angle, flip)     + (aangle,  aflip) = (angle, flipNone) + (flip, aangle) + (0, aflip) = (angle - aangle,  flip + aflip)
    MyRotation AddRotation(MyRotation arot) 
    {
        if (arot.flipType != flipNone)   // then no rotation
            AddType(arot.flipType);
        else                             // pure rotation, no flip
            AddAngle(arot.angle);
        return *this;
    }
    inline void InvertAngle()
    {
        _SetRot((angle = -angle));
    }

    bool ResultsInNoRotation(MyRotation &arot, bool markIfNoRotation = false)    // returns true if it is a no-rotation state
    {
        MyRotation tmpRot = *this;
        tmpRot.AddRotation(arot);
        return tmpRot.flipType == flipNone && !tmpRot.angle;
    }

    // in the following rotation functions it is assumed that any rectangle or image has sides parallel to the
    // coordinate axes. No such assumption is made for point and polygons
    //--------- order of rotations: first rotate by angle then flip. |angle| <= 180 
    bool RotateRect(QRectF& rect, QPointF center, bool noCheck = false) const // assumes outer edge of rectangle is inside center
    {
        bool result = true;
        QRectF r = rect;
        r.translate(-center);

        if(angle)   // angle == 0.0 check is ok
            r = _tr.map(r).boundingRect();

        if (flipType != flipNone )
        {
            switch (flipType)
            {
                case rotFlipH: 
                {
                    qreal w = r.width();
                    r.setX(-r.right());	// top left is replaced by original top right
                    r.setWidth(w);
                }
                break;
                case rotFlipV:
                {
                    qreal h = r.height();
                    r.setY(-r.bottom());
                    r.setHeight(h);
                }
                break;
                default:
                    break;
            }
        }
        r.translate(center);

        if (!noCheck && (r.top() < 0 || r.left() < 0))
        {
            _CantRotateWarning();
            result = false;
        }
        else
        {
            rect = r;
            result = true;
        }
        
        return result;
    }

    bool RotatePoly(QPolygonF& points, QPointF center, bool noCheck = false) const
    {
        QRectF bndRect = points.boundingRect();

        points.translate(-center);
        if (!noCheck && !RotateRect(bndRect, center, noCheck)) // must or could rotate
            return false;

        if(angle)
            points = _tr.map(points);

        if (flipType == rotFlipH)
        {
            for (auto& pt : points)
                pt.setX(-pt.x());
        }
        else if (flipType == rotFlipV)
        {
            for (auto& pt : points)
                pt.setY(-pt.y());
        }
        points.translate(center);
        return true;
    }
    bool RotateSinglePoint(QPointF &pt, QPointF center, bool noCheck = false)  const
    {
        QPointF p = pt;

        p -= center;
        p = _tr.map(p);
        if (flipType == rotFlipH)
        {
            p.setX(-p.x());
        }
        else if (flipType == rotFlipV)
        {
            p.setY(-p.y());
        }
        p = p + center;

        if (!noCheck && (p.x() < 0 || p.y() < 0) )
        {
            _CantRotateWarning();
            return false;
        }

        pt = p;
        return true;
    }
    bool RotatePixmap(QPixmap& img, QPointF topLeft, QPointF center, bool noCheck = false)  const
    {
        if (!noCheck)
        {
            QRectF r = QRectF(topLeft, img.size());
            r = _tr.map(r).boundingRect();
            if (r.x() < 0 || r.y() < 0)
            {
                _CantRotateWarning();
                return false;
            }
        }
        bool fliph = false, flipv = true;	// default flip orientations

        img = img.transformed(_tr, Qt::SmoothTransformation);   // rotation

        QImage image;
        switch (flipType)
        {
            case MyRotation::rotFlipH:
                fliph = true;
                flipv = false;		// NO break here! 
                [[fallthrough]];	// min C++17!
            case MyRotation::rotFlipV:
                image = img.toImage();
                image = image.mirrored(fliph, flipv);
                img = QPixmap::fromImage(image);
                break;
            default:
                break;
        }
        return true;
    }
    bool RotateLine(QLineF& line, QPointF center, bool noCheck = false)
    {
        QLineF l = line;

        l.translate(-center);
        l = _tr.map(l);       // always rotation first

        if (flipType == rotFlipH)
        {
            l.setP1(QPointF(-l.p1().x(), l.p1().y()) );
            l.setP2(QPointF(-l.p1().x(), l.p1().y()) );
        }
        else if (flipType == rotFlipV)
        {
            l.setP1(QPointF(l.p1().x(), -l.p1().y()) );
            l.setP2(QPointF(l.p1().x(), -l.p1().y()) );
        }
        l.translate(center);
        if(noCheck || (l.p1().x() >= 0 && l.p1().y() >= 0 && l.p2().x() >= 0 && l.p2().y() >= 0) )
        {
            line = l;
            return true;
        }
        else
        {
            _CantRotateWarning();
            return false;
        }
    }
    bool IsNull() const { return !angle && flipType == flipNone; }
 private:
    QTransform _tr;
    void _CantRotateWarning() const;
    void _SetRot(qreal alpha)
    {
        alpha = fmod(alpha, 360);   // set alpha into the (-180, 180] range
        if (alpha > 180.0)
            alpha -= 180.0;
        if (fabs(alpha) < eps)
            alpha = 0.0;
        angle = alpha;

        _tr = QTransform();
        _tr.rotate(alpha);
        angle = alpha;
    }
};
QDataStream& operator<<(QDataStream& ofs, const MyRotation& mr);
QDataStream& operator>>(QDataStream& ifs, MyRotation& mr);
            //----------------------------------------------------
            // ------------------- Drawable Pen -------------
            //----------------------------------------------------


class DrawablePen
{
    FalconPenKind _penKind = penBlack;
public:
    qreal penWidth = 1.0;

    DrawablePen() = default;
    //DrawablePen(qreal penWidth, QColor penColor) : _penKind(penNone), penWidth(penWidth), penColor(penColor) {}
    DrawablePen(FalconPenKind penKind, qreal penWidth) : _penKind(penKind), penWidth(penWidth) 
    {
    }
    DrawablePen(const  DrawablePen&) = default;
    DrawablePen& operator=(const DrawablePen& o) = default;
    DrawablePen& operator=(DrawablePen&& o) = default;

    void SetPenKind(FalconPenKind pk)
    {
        _penKind = pk;
    }

    QColor PenColor() const 
    { 
        QColor penColor = drawColors[_penKind];
        return penColor; 
    }
    constexpr FalconPenKind PenKind() const { return _penKind; }
    void SetPainterPenAndBrush(QPainter* painter, const QRectF& clipR = QRectF(), QColor brushColor = QColor())
    {
        if(clipR.isValid())
            painter->setClipRect(clipR);

        QPen pen(QPen(PenColor(), penWidth, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin) );
        painter->setPen(pen);
        if (brushColor.isValid())
            painter->setBrush(QBrush(brushColor));
        else
            painter->setBrush(QBrush());
        // painter's default compositionmode is CompositionMode_SourceOver
        // painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
    }
};

            //----------------------------------------------------
            // ------------------- Drawable Header -------------
            //----------------------------------------------------
struct DrawableItem : public DrawablePen
{
    static bool drawStarted;

    DrawableType dtType = DrawableType::dtNone;
    QPointF startPos;       // position of first point relative to logical (0,0) of 'paper roll' (widget coord: topLeft + DrawArea::_topLeft is used) 
                            // DOES NOT always correspond to top left of area!
    MyRotation rot;         // used to store the actual state of the rotations and check if rotation is possible
    int   zOrder = -1;      // not saved. Drawables are saved from lowest zOrder to highest zOrder
    bool  isVisible = true;                              // not saved
    // eraser strokes for a drawables. Oonly those parts of the eraser stroke that intersects the bounding rectangle of the
    // drawable plus an eraser pen width/2 wide margin are saved here. When drawing the drawable eraser strokes are
    // clipped to the bounding box of the drawable plus half of the pen width for scribbles rectangles and ellipses.
    // drawables completely under eraser should not be saved (TODO?)
    struct EraserData
    {
        int eraserPenWidth = 30;
        QPolygonF eraserStroke;
    };

    QVector<EraserData> erasers;   // a polygon(s) confined to the bounding rectangle of points for eraser strokes (above points)
                                 // may intersect the visible lines only because of the pen width!


    DrawableItem() = default;
    DrawableItem(DrawableType dt, QPointF startPos, int zOrder = -1, FalconPenKind penKind = penBlack, qreal penWidth = 1.0) : dtType(dt), startPos(startPos), zOrder(zOrder), DrawablePen(penKind, penWidth) {}
    DrawableItem(const DrawableItem& other) { *this = other; }
    virtual ~DrawableItem()
    {
        erasers.clear();
    }
    DrawableItem& operator=(const DrawableItem& other);

    int AddEraserStroke(int eraserWidth, const QPolygonF& eraserStroke);  // returns # of sub-strokes added
    virtual void RemoveLastEraserStroke(EraserData* andStoreHere = nullptr);

    bool IsVisible() const { return isVisible; }
    bool IsImage() const { return dtType == DrawableType::dtScreenShot; }
    virtual bool IsFilled() const { return false; }
    virtual bool PointIsNear(QPointF p, qreal distance) const  // true if the point is near the lines or for filled items: inside it
    {
        p -= startPos;
        return p.x() * p.x() + p.y() * p.y() < distance * distance;
    }

    virtual QRectF Area() const { return QRectF(); }
    virtual bool Intersects(QRectF rect) const
    {
        return Area().intersects(rect);
    }
    virtual QPointF GetLastDrawnPoint() const
    {
        return startPos;
    }
    virtual void Translate(QPointF dr, qreal minY);            // only if not deleted and top is > minY. Override this only for scribbles

    virtual void Rotate(MyRotation rot, QPointF &center);    // alpha used only for 'rotAngle'
    /*=============================================================
     * TASK: Function to override in all subclass
     * PARAMS:  painter              - existing painter
     *          topLeftOfVisibleArea - document relative coordinates of visible part
     *          clipR                - document relative clipping rectangle used for eraser
     * GLOBALS: drawStarted
     * RETURNS: none
     * REMARKS: - each Draw functions must follow the same structure:
     *              if drawStarted is set perform a normal drawing
     *              else call the common DrawWithEraser function, which
     *              first sets drawStarted to true then calls back
     *              the function that called it then draws the eraser strokes
     *              inside the clipping rectangle
     *------------------------------------------------------------*/
    virtual void Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR = QRectF())
    {
        if (drawStarted)
        {
            // draw normally using 'painter',clipR (if valid) and 'topLeftOfVisibleArea'
            SetPainterPenAndBrush(painter, clipR.translated(-topLeftOfVisibleArea));
            QPointF pt = startPos - topLeftOfVisibleArea;
            painter->drawPoint(pt);
            // this example draws a single point (good for DrawableDot)
        }
        else
            DrawWithEraser(painter, topLeftOfVisibleArea, clipR);
    }
    /*=============================================================
     * TASK:    common draw functions for every subclass
     * PARAMS:  same one as for Draw()
     * GLOBALS: drawStarted
     * RETURNS:
     * REMARKS: - sets drawStated to true then  calls back
     *              the function that called it then draws the eraser strokes
     *              inside the clipping rectangle
     *------------------------------------------------------------*/
    void DrawWithEraser(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR = QRectF())
    {
        drawStarted = true;

        if (erasers.size())                 // then paint object first then erasers on separate pixmap 
        {                                   // and copy the pixmap to the visible area
            QRectF area = Area();           // includes half of pen width (none for screenshots)
            //QPixmap pxm(area.size().toSize());
            QImage pxm(area.width(), area.height(), QImage::Format_ARGB32);
            pxm.fill(Qt::transparent);
            QPainter myPainter(&pxm);
            QPointF tl = area.topLeft();
            //if (dtType != DrawableType::dtScreenShot)
            //    tl -= QPointF(penWidth, penWidth) / 2.0;
            Draw(&myPainter, tl,clipR);     // calls painter of subclass, 
                                            // which must check 'drawStarted' and 
                                            // must not call this function if it is set
                                            // clipRect is not important as the pixmap is exactly the right size
            // and now the erasers
#if !defined _VIEWER && defined _DEBUG
            if (!isDebugMode)
            {
#endif
                myPainter.setCompositionMode(QPainter::CompositionMode_Clear);
                myPainter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
#if !defined _VIEWER && defined _DEBUG
            }
#endif

            for (auto er : erasers)
            {
                QPen pen(QPen(Qt::black, er.eraserPenWidth, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));
                myPainter.setPen(pen);
                if (er.eraserStroke.size() == 1)
                    myPainter.drawPoint(er.eraserStroke.at(0) - tl);
                else
                    myPainter.drawPolyline(er.eraserStroke.translated(-tl));    // do not close path
            }
            painter->setClipRect(clipR);
            painter->drawImage(tl - topLeftOfVisibleArea, pxm);
            //painter->drawPixmap(tl - topLeftOfVisibleArea, pxm);
                        //pxm.save("pxm.png", "png");
            // end DEBUG
        }
        else                                // no erasers: just paint normally
            Draw(painter, topLeftOfVisibleArea, clipR);

        drawStarted = false;
    }
protected:
    void _RotateErasers(MyRotation rot, QPointF center); // returns center point of center or QPoint(-1,-1) if rotation is not possible
};

QDataStream& operator<<(QDataStream& ofs, const DrawableItem& di);
QDataStream& operator>>(QDataStream& ifs, DrawableItem& di);

            //----------------------------------------------------
            // ------------------- Drawable Cross -------------
            //----------------------------------------------------
struct DrawableCross : public DrawableItem
{
    qreal length=10;                 // total length of lines intersecting at 90

    DrawableCross() : DrawableItem()
    {
        dtType = DrawableType::dtCross;
        _Setup();
    }
    DrawableCross(QPointF pos, qreal len, int zorder, FalconPenKind penKind, qreal penWidth);
    DrawableCross(const DrawableCross& o) = default;
    ~DrawableCross() = default;
    void Translate(QPointF dr, qreal minY) override;            // only if not deleted and top is > minY. Override this only for scribbles
    void Rotate(MyRotation rot, QPointF &center) override;    // alpha used only for 'rotAngle'
    virtual MyRotation::Type RotationType() { return MyRotation::flipNone;  /* yet */ }
    QRectF Area() const override;    // includes half of pen width+1 pixel
    void Draw(QPainter* painter, QPointF startPosOfVisibleArea, const QRectF& clipR = QRectF()) override;
private:
    QLineF _ltrb, _lbrt;
    void _Setup();
    friend QDataStream& operator>>(QDataStream& ifs, DrawableCross& di);	  // call AFTER header is read in

};
QDataStream& operator<<(QDataStream& ofs, const DrawableCross& di);
QDataStream& operator>>(QDataStream& ifs,       DrawableCross& di);  // call AFTER header is read in

            //----------------------------------------------------
            // ------------------- Drawable Dot -------------
            //----------------------------------------------------
struct DrawableDot : public DrawableItem
{
    DrawableDot() : DrawableItem()
    {
        dtType = DrawableType::dtDot;
    }
    DrawableDot(QPointF pos, int zorder, FalconPenKind penKind, qreal penWidth) : DrawableItem(DrawableType::dtDot, pos, zOrder, penKind, penWidth) { }
    DrawableDot(const DrawableDot& o) = default;
    ~DrawableDot() = default;
    QRectF Area() const override    // includes half od pen width+1 pixel
    { 
        qreal d = penWidth / 2.0 + 1.0;
        return QRectF(startPos - QPointF(d, d), QSize(2*d, 2*d) ); 
    }
    // in base class bool PointIsNear(QPointF p, qreal distance) const override// true if the point is near the circumference or for filled ellpse: inside it
};
QDataStream& operator<<(QDataStream& ofs, const DrawableDot& di);
QDataStream& operator>>(QDataStream& ifs,       DrawableDot& di);  // call AFTER header is read in

            //----------------------------------------------------
            // ------------------- Drawable Ellipse -------------
            //----------------------------------------------------

struct DrawableEllipse : public DrawableItem
{
    QRectF rect;                // determines the two axes and the center of the ellipse
    bool isFilled=false;        // whether closed polygon (ellipse or rectangle) is filled

    DrawableEllipse() : DrawableItem()
    {
        dtType = DrawableType::dtEllipse;
    }
    DrawableEllipse(QRectF rect, int zorder, FalconPenKind penKind, qreal penWidth, bool isFilled = false);
    DrawableEllipse(const DrawableEllipse& o);
    DrawableEllipse(DrawableEllipse&& o) noexcept;
    ~DrawableEllipse() = default;
    DrawableEllipse &operator=(const DrawableEllipse& o);
    DrawableEllipse &operator=(DrawableEllipse&& o) noexcept;

    void Translate(QPointF dr, qreal minY) override;            // only if not deleted and top is > minY
    void Rotate(MyRotation rot, QPointF &center) override;    // alpha used only for 'rotAngle'
    QRectF Area() const override // includes half od pen width+1 pixel
    {
        qreal d = penWidth / 2.0 + 1.0;
        return _rotatedRect.adjusted(-d,-d,d,d); 
    }
    QPointF GetLastDrawnPoint() const override
    {
        return  startPos + QPointF(_rotatedRect.width(), _rotatedRect.height() / 2.0);
    }

    bool PointIsNear(QPointF p, qreal distance) const override// true if the point is near the circumference or for filled ellipse: inside it
    {
        if (!_rotatedRect.contains(p))
            return false;

        QPointF center = rect.center();     // unrotated / simple rotated ellipse
        // first, if ellipse is rotated, rotate point by -angle 
        // into the original (unrotated) ellipse based coord system
        // flips don't matter
        if (fabs(rot.angle) < eps)
        {
            QTransform tr;
            tr.rotate(-rot.angle);

            p = tr.map(p - center);
        }
        else   // transform center to origin and point with it
            p = p - center;         // point P(xp,yp)
        // then check transformed point

        qreal a = rect.width() / 2.0, b = rect.height() / 2,    // major and minor axes of ellipse
            a2 = a * a, b2 = b * b,                             // their squares
            xp2=p.x()*p.x(), yp2=p.y()*p.y();                   // and squares of coords of point P
        
        qreal xq2, yq2; // square of coordinates of point Q (xq,yq), where line between p and center intersects the circumference
        xq2 = a2 * b2 * xp2 / (xp2 * b2 + yp2 * a2);
        yq2 = b2 * (1 - xq2 / a2);
        // square of distances of P and Q from origin
        qreal distP = sqrt(xp2 + yp2), distQ = sqrt(xq2 + yq2);
                    
        if (isFilled)
            return distP <= distQ + distance;
        return (distP <= distQ && distP >= distQ - distance) ||
                (distP > distQ && distP < distQ + distance * distance);
    }
    virtual void Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR = QRectF()) override;
private:
    QPolygonF _points;       // used when rotated by not an angle not a multiple of 90 degrees or not a flip
    QRectF _rotatedRect;         // used for Area(), same as 'rect' unless rotated

    void _ToPolygonF()
    {
        QPainterPath myPath;
        myPath.addEllipse(rect);
        _points = myPath.toFillPolygon();
        _rotatedRect = _points.boundingRect();
    }
};
QDataStream& operator<<(QDataStream& ofs, const DrawableEllipse& di);
QDataStream& operator>>(QDataStream& ifs,       DrawableEllipse& di);  // call AFTER header is read in

            //----------------------------------------------------
            // ------------------- Drawable Rectangle -------------
            //----------------------------------------------------
struct DrawableLine : public DrawableItem
{
    QPointF endPoint;

    DrawableLine() : DrawableItem()
    {
        dtType = DrawableType::dtLine;
    }
    DrawableLine(QPointF startPos, QPointF endPoint, int zorder, FalconPenKind penKind, qreal penWidth);
    DrawableLine(const DrawableLine& ol);
    DrawableLine(DrawableLine&& ol);
    ~DrawableLine() {}
    DrawableLine& operator=(const DrawableLine& ol);
    DrawableLine& operator=(const DrawableLine&& ol);
    void Translate(QPointF dr, qreal minY) override;            // only if not deleted and top is > minY
    void Rotate(MyRotation rot, QPointF &center) override;    // alpha used only for 'rotAngle'
    QRectF Area() const override// includes half of pen width+1 pixel
    {
        QRectF rect = QRectF(startPos, QSize((endPoint - startPos).x(), (endPoint - startPos).y()) ).normalized();
        qreal d = penWidth / 2.0 + 1.0;
        return rect.adjusted(-d, -d, d, d);
    }
    QPointF GetLastDrawnPoint() const override
    {
        return endPoint;
    }
    bool PointIsNear(QPointF p, qreal distance) const override
    {                                                         
        return __IsLineNearToPoint(startPos, endPoint, p, distance);
    }
    void Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR = QRectF()) override;
};
QDataStream& operator<<(QDataStream& ofs, const DrawableLine& di);
QDataStream& operator>>(QDataStream& ifs,       DrawableLine& di);  // call AFTER header is read in

;
            //----------------------------------------------------
            // ------------------- Drawable Rectangle -------------
            //----------------------------------------------------
struct DrawableRectangle : public DrawableItem
{
    QRectF rect;
    qreal angle = 0.0;          // angle if rotated by an angle and not just 90,180,270, etc
    bool isFilled=false;        // whether closed polygon and it is filled

    DrawableRectangle() : DrawableItem()
    {
        dtType = DrawableType::dtRectangle;
    }
    DrawableRectangle(QRectF rect, int zorder, FalconPenKind penKind, qreal penWidth, bool isFilled = false);
    DrawableRectangle(const DrawableRectangle& o);
    ~DrawableRectangle() = default;
    DrawableRectangle& operator=(const DrawableRectangle& di);
    DrawableRectangle& operator=(DrawableRectangle&& di) noexcept;

    void Translate(QPointF dr, qreal minY) override;            // only if not deleted and top is > minY
    void Rotate(MyRotation rot, QPointF &center) override;    // alpha used only for 'rotAngle'
    QRectF Area() const override// includes half of pen width+1 pixel
    {
        qreal d = penWidth / 2.0 + 1.0;
        return _rotatedRect.adjusted(-d, -d, d, d);
    }
    QPointF GetLastDrawnPoint() const override
    {
        return rect.bottomRight();
    }

    bool PointIsNear(QPointF p, qreal distance) const override// true if the point is near the circumference or for filled rectangle: inside it
    {                                                           // the two axes are parallel to x,y
        if (!_rotatedRect.contains(p))
            return false;

        if(fabs(rot.angle) < eps && !rot.HasSimpleRotation() )
            rot.RotateSinglePoint(p, _rotatedRect.center(), true);

        if (isFilled)
            return rect.contains(p);
        // when both x and y distance is too large
        bool bDistanceTooLarge = (               // x distance
            (p.x() < rect.left() - distance) ||                                        
            (p.x() > rect.right() + distance) ||
            ((p.x() > rect.left() + distance) && (p.x() < rect.right() - distance) )
                                  ) &&          
                                  (              // y distance
            (p.y() < rect.top() - distance) ||
            (p.y() > rect.bottom() + distance) ||
            ((p.y() > rect.top() + distance) && (p.y() < rect.bottom() - distance) )
                                  );
        return !bDistanceTooLarge;
    }
    void Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR = QRectF()) override;
private:
    QRectF _rotatedRect;         // used for Area(), same as 'rect' unless rotated
    QPolygonF _points;
    void _ToPolygonF()
    {
        QPainterPath myPath;
        myPath.addRect(rect);
        _points = myPath.toFillPolygon();
    }
};
QDataStream& operator<<(QDataStream& ofs, const DrawableRectangle& di);
QDataStream& operator>>(QDataStream& ifs,       DrawableRectangle& di);  // call AFTER header is read in

            //----------------------------------------------------
            // ------------------- Drawable Screenshot -------------
            //----------------------------------------------------
 /*-------------------------------------------------------*/
struct DrawableScreenShot : public DrawableItem     // for a screenshot startPos is the center of the image!
{
    DrawableScreenShot() : DrawableItem()
    {
        dtType = DrawableType::dtScreenShot;
    }
    DrawableScreenShot(const DrawableScreenShot& other) : DrawableItem(other), _image(other._image), _rotatedImage(other._rotatedImage), _rotatedArea(other._rotatedArea)
    {
    
    }
    DrawableScreenShot(QPointF topLeft, int zOrder, const QPixmap &image) : DrawableItem(DrawableType::dtScreenShot, topLeft, zOrder), _image(image) 
    {
        _rotatedArea = QRectF(startPos, image.size());
    }
    ~DrawableScreenShot() = default;

    const QPixmap& Image(bool original=false) const 
    { 
        return (original || _rotatedImage.isNull()) ? _image : _rotatedImage; 
    }
    void SetImage(QPixmap& image);


    QRectF Area() const override;
    // canvasRect relative to paper (0,0)
    // result: relative to image
    // isNull() true when no intersection with canvasRect
    QRectF AreaOnCanvas(const QRectF& canvasRect) const;

    void Translate(QPointF dr, qreal minY) override;            // only if not deleted and top is > minY
    void Rotate(MyRotation rot, QPointF &center);
    bool PointIsNear(QPointF p, qreal distance) const override // true if the point is inside the image
    {
        if(fabs(rot.angle) < eps && !rot.HasSimpleRotation() )
            rot.RotateSinglePoint(p, _rotatedArea.boundingRect().center(), true);
        return _rotatedArea.contains(p);
    }
    void Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR = QRectF()) override;    // screenshot are painted in paintEvent first followed by other drawables
    // QPolygonF ToPolygonF()  - default
private:
    QPixmap _image;             // screenshot image
    QPixmap _rotatedImage;      // that is shown
    QPolygonF _rotatedArea;     // line area of enclosing rectangle after rotation
};

QDataStream& operator<<(QDataStream& ofs, const  DrawableScreenShot& di);
QDataStream& operator>>(QDataStream& ifs,        DrawableScreenShot& di);           // call AFTER header is read in

            //----------------------------------------------------
            // ------------------- Drawable Scribble -------------
            //----------------------------------------------------
struct DrawableScribble   : public DrawableItem     // drawn on layer mltScribbleItem
{                   
    QPolygonF points;         // coordinates are relative to logical origin (0,0) => canvas coord = points[i] - origin
    bool isFilled = false;    // used when this is a closed polygon (created from not n * pi/4) rotated ellipse or rectangle

    // DEBUG
    bool bSmoothDebug = true;
    // end DEBUG

    DrawableScribble() : DrawableItem()
    {
        dtType = DrawableType::dtScribble;
    }
    DrawableScribble(FalconPenKind penKind, qreal penWidth, int zorder) noexcept;
    DrawableScribble(const DrawableScribble& di);
    DrawableScribble(DrawableScribble&& di) noexcept;
    /* one possibility to rotate all with arbitrary angle is to create a scribble from them
    DrawableScribble(const DrawableCross& o);
    DrawableScribble(const DrawableEllipse& o);
    DrawableScribble(const DrawableRectangle& o);
    DrawableScribble(const DrawableText& o);
    */
    DrawableScribble& operator=(const DrawableScribble& di);

    DrawableScribble& operator=(DrawableScribble&& di)  noexcept;

    DrawableType Type() const 
    {
        return dtType;
    }

    void Clear();       // clears points

    static bool IsExtension(const QPointF& p, const QPointF& p1, const QPointF& p2 = QPoint()); // vectors p->p1 and p1->p are parallel?

    QPointF Add(QPointF p, bool smoothed = false, bool reset=false);          
    void Add(int x, int y, bool smoothed = false, bool reset = false);
    void Reset();               // points and smoother

    bool Intersects(const QRectF& arect) const;



    void Translate(QPointF dr, qreal minY) override;    // only if not deleted and top is > minY
    void Rotate(MyRotation rot, QPointF &center) override;    // alpha used only for 'rotAngle'
    QRectF Area() const override // includes half od pen width+1 pixel
    {
        qreal d = penWidth / 2.0 + 1.0;
        return points.boundingRect().adjusted(-d, -d, d, d);
    }
    QPointF GetLastDrawnPoint() const override
    {
        return points[points.size() - 1];
    }

    bool PointIsNear(QPointF p, qreal distance) const override// true if the point is near the circumference or for filled ellpse: inside it
    {
        for (auto i = 0; i < points.size() - 1; ++i)
            if (__IsLineNearToPoint(points[i], points[i + 1], p, distance) )
                return true;
        return false;
    }
    void Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR = QRectF()) override;
};

QDataStream& operator<<(QDataStream& ofs, const DrawableScribble& di);
QDataStream& operator>>(QDataStream& ifs, DrawableScribble& di);           // call AFTER header is read in

            //----------------------------------------------------
            // ------------------- Drawable Text -------------
            //----------------------------------------------------
struct DrawableText : public DrawableItem
{
    QString _text;                  // so that a text() fucntion can be created
    QString fontAsString;           // list of all properties separated by commas

    DrawableText() : DrawableItem()       // default constructor            TODO
    {
        dtType = DrawableType::dtText;
    }
    DrawableText(QPointF topLeft, int zorder) noexcept
    {
        ;  // TODO 
    }

    DrawableText(const DrawableText& di) = default;             // TODO
    DrawableText(DrawableText&& di) noexcept = default;   // TODO
    DrawableText& operator=(const DrawableText& di)
    {
        // TODO
        return *this;
    }

    DrawableText& operator=(DrawableText&& di)  noexcept
    {
        // TODO
        return *this;
    }

    void setText(QString txt)
    {
        // TODO: ??
        _text = txt;
    }
    QString text() const { return _text; }

    void setFont(QString font) { fontAsString = font; }

//    void Translate(QPointF dr, qreal minY) override;    // only if not deleted and top is > minY
//    void Rotate(MyRotation rot, QRectF center, qreal alpha=0.0) override;    // alpha used only for 'rotAngle'
    QRectF Area() const override { return QRectF();  /* TODO ??? */ }
    bool PointIsNear(QPointF p, qreal distance) const override // true if the point is near the circumference or for filled ellpse: inside it
    {
        return false; // ???
    }
    void Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR = QRectF()) override;
    // QPolygonF ToPolygonF()  - default
};

QDataStream& operator<<(QDataStream& ofs, const DrawableText& di);
QDataStream& operator>>(QDataStream& ifs,       DrawableText& di);           // call AFTER header is read in


            //----------------------------------------------------
            // ------------------ drawable index --------------
            //----------------------------------------------------
struct DrawableItemIndex // index inside a DrawableList
{
    DrawableType type = DrawableType::dtNone;
    int index = -1;           // in DrawableList
    int zorder = 0;          // type independent
};

//using IndexVectorIterator = std::map<int, DrawableItemIndex>::iterator;
//using IndexVectorReverseIterator = std::map<int, DrawableItemIndex>::reverse_iterator;
//class DrawableIndexVector : public std::map<int, DrawableItemIndex> // z order => index in list i.e. all screenshots come first
//{
//public:
//    IndexVectorIterator Insert(int i, DrawableItem* item)
//    {
//        DrawableItemIndex drix = { item->dtType, i, item->zOrder };
//        auto res = insert(std::pair(item->zOrder, drix) );
//        return res.first;
//    }
//};
using DrawableIndexVector = IntVector;

                                           // followed by all other drawables
QuadArea AreaForItem(const int& index); // index in DrawableList
bool IsItemsEqual(const int& index1, const int& index2);

            //----------------------------------------------------
            // ----Drawable Item List - a list for all drawable types -
            //----------------------------------------------------
using DrawableItemList = QList<DrawableItem*>;


            //----------------------------------------------------
            // ---- QuadTreeDelegate - using QuadTree if set up else no problem -
            //----------------------------------------------------

struct QuadTreeDelegate
{
    // Quad tree for storing indices to items in this list
    QuadTree<int, decltype(AreaForItem), decltype(IsItemsEqual)>* pItemTree = nullptr;
    QuadTreeDelegate()   {  }
    ~QuadTreeDelegate() { delete pItemTree; }
    void SetUp()
    {
        delete pItemTree;
        pItemTree = new QuadTree<int, decltype(AreaForItem), decltype(IsItemsEqual)>(QuadArea(0, 0, 4000, 3000), AreaForItem, IsItemsEqual);
    }

    int Count(const QuadArea &area=QuadArea()) { return pItemTree ? pItemTree->Count(area) : 0; }
    QuadArea Area() { return pItemTree ? pItemTree->Area() :QuadArea(); }
    void Clear()
    {
        if(pItemTree)
            pItemTree->Resize(QuadArea(0, 0, 4000, 3000), nullptr, false);
    }
    void Add(int ix) 
    { 
        if(pItemTree) 
            pItemTree->Add(ix); 
    }
    void Remove(int ix) 
    { 
        if(pItemTree) 
            pItemTree->Remove(ix); 
    }
    IntVector GetValues(const DrawableItemList* pItems, const QRectF& area) const
    {
        extern QuadArea AreaForQRect(QRectF rect);
        return GetValues(pItems, AreaForQRect(area));

    }
    IntVector GetValues(const DrawableItemList* pItems, const QuadArea& area) const
    {

        auto sortFunc = [&](int i, int j) 				// returns true if item 'i' comes before item 'j'
        {
            DrawableItem* pdri1 = (*pItems)[i], * pdri2 = (*pItems)[j];
            // no two items may have the same z-order
            bool zOrderOk = pdri1->zOrder < pdri2->zOrder;

            if (zOrderOk)	// zOrder OK
                return true;
            // zorder i >= zorder j
            return false;
        };

        std::vector<int> iv = pItemTree->GetValues(area);	// only visible elements!
        std::sort(iv.begin(), iv.end(), sortFunc);
        IntVector resv = QVector<int>(iv.begin(), iv.end()); //  ::fromStdVector(iv);	<- deprecated
        return resv;
    }
    void Resize(QuadArea area, const int* pNewItem = nullptr, bool savePrevVals = true)
    {
        return pItemTree->Resize(area, pNewItem, savePrevVals);
    }
    int BottomItem()
    {
        return pItemTree->BottomItem();
    }
};

            //----------------------------------------------------
            // ----ZorderStore - zorder for srcreenshots and others -
            //----------------------------------------------------
class ZorderStore
{
    int _lastImageZorder = 0,
        _lastZorder = DRAWABLE_ZORDER_BASE;
public:
    ZorderStore() = default;
    ~ZorderStore() = default;

    void Reset()
    {
        _lastImageZorder = 0;
            _lastZorder = DRAWABLE_ZORDER_BASE;
    }
    int GetZorder(bool bScreenShot, bool increment = true)
    { 
        int res = bScreenShot ? _lastImageZorder : _lastZorder;
        if (increment)
        {
            if (bScreenShot)
                ++_lastImageZorder;
            else 
                ++_lastZorder;
        }
        return res;
    }
};
            //----------------------------------------------------
            // ----Drawable List - a list for all drawable types -
            //----------------------------------------------------
class DrawableList
{                                     
    DrawableItemList _items,
                     _redoItems;

    QuadTreeDelegate* _pQTree = nullptr;  // contains indices in _pItems, NOT created and not deleted in this class

    ZorderStore* _pZorderStore = nullptr; // set from owner (History)
    IntVector _indicesInRect;           // from this rectangle
    int _runningIndex = -1;

    constexpr qreal _dist(QPointF p1, QPointF p2)  // square of distance of points p1 and p2
    {
        p1 = p2 - p1;
        return p1.x() * p1.x() + p1.y() * p1.y();
    };
public:
    // must set a drawable store
    DrawableList() = default;
    DrawableList(const DrawableList& other)
    {
        *this = other;
    }

    DrawableList(DrawableList&& other) noexcept
    {
        *this = other;
    }

    DrawableList &operator=(const  DrawableList& other)
    {
        _items = other._items;
        _redoItems = other._redoItems;

        _pZorderStore = other._pZorderStore;
        //_allocatedZorders = true;

        _pQTree = other._pQTree;

        _runningIndex = other._runningIndex;
        _indicesInRect = other._indicesInRect;
        return *this;
    }
    DrawableList &operator=(DrawableList&& other)   noexcept
    {
        _items = other._items;
        _redoItems = other._redoItems;
        other._items.clear();
        other._redoItems.clear();

        //if (_allocatedZorders)
        //    delete _pZorderStore;
        _pZorderStore = other._pZorderStore;
        other._pZorderStore = nullptr;
        //_allocatedZorders = other._allocatedZorders;
        //other._allocatedZorders = false;

        _pQTree = other._pQTree;		
        other._pQTree = nullptr;

        _runningIndex = other._runningIndex;
        _indicesInRect = other._indicesInRect;
        return *this;
    }

    DrawableList& SetZorderStore(ZorderStore* pzorders = nullptr)
    {
        //if (pzorders)
            _pZorderStore = pzorders;
        //else
        //{
        //    //_allocatedZorders = true;
        //    _pZorderStore = new ZorderStore;
        //}
        return *this;
    }
    DrawableList& SetQuadTreeDelegate(QuadTreeDelegate* pqtree)
    {
        _pQTree = pqtree;
        return *this;
    }

    ~DrawableList()              // pQTree is deleted elsewhere!
    {
        Clear();
        _pQTree = nullptr;
    }

    DrawableItemList* Items() { return &_items; }

    void ResetZorder()
    {
        _pZorderStore->Reset();
    }

    void Clear(bool andDeleteQuadTree = false)
    {
        if (_pQTree)
        {
            _pQTree->Clear();
            if (andDeleteQuadTree)
                _pQTree = nullptr;
        }

        for (auto a : _items)
                delete a;
        _items.clear();
        if(_pZorderStore)
            ResetZorder();
    }
            // these two used only when a new drawable is just added or the top van is remeoved
    void Undo()    // moves last item to _redoItems w.o. deleting them
    {                       // and removes them from the quad tree
        int i = _items.size() - 1;
        if (_pQTree)
             _pQTree->Remove(i);
        _redoItems.push_back(_items.at(i));
        _items.pop_back();
    }

    void Redo()    // moves last item from _redoItems 
    {                       // and adds them from the quad tree
        int i = _items.size(), k = _redoItems.size()-1;
        _items.push_back(_redoItems.at(k));
        _redoItems.pop_back();
        if (_pQTree)
            _pQTree->Add(i);
    }

    void ClearRedo()
    {
        for (auto a : _redoItems)
            delete a;
        _redoItems.clear();
    }

    void Pop_back(int n = 1)    // Deletes last n items. Used only when history's undo list is discarded
    {
        for (int i = _items.size() - 1; i >= 0; --i)
        {
            SetVisibility(i, false);    // removes them from QuadTree if they were there
            delete _items[i];
            _items.pop_back();
        }
    }

    int Count(const QuadArea& area = QuadArea())  // count of visible items
    {
        return _pQTree->Count(area);
    }

    int CountOfScreenShots()
    {
        int cnt = 0;
        for (auto p : _items)
            if (p->dtType == DrawableType::dtScreenShot && p->isVisible)
                ++cnt;
        return cnt;
    }

    QuadArea Area() { return _pQTree ? _pQTree->Area() : QuadArea(); }

    IntVector ListOfItemIndicesInRect(QRectF& r) const; // ordered by z-order then y, then x
    IntVector ListOfItemIndicesInQuadArea(QuadArea& r) const; // ordered by z-order then y, then x
    QPointF DrawableList::BottomRightLimit(QSize& screenSize);

    DrawableItem* FirstVisibleDrawable(QRectF& r)  // smallest in zOrder
    {
        _indicesInRect = ListOfItemIndicesInRect(r);

        _runningIndex = -1; //  _itemIndices.begin();
        return NextVisibleDrawable();
    }

    DrawableItem* NextVisibleDrawable()  // smallest in zOrder
    {
        DrawableItem* pdh;

        for (++_runningIndex; _runningIndex < _indicesInRect.size(); ++_runningIndex)
        {
            pdh = (*this)[_indicesInRect[_runningIndex] ];
            if (pdh->isVisible)
                return pdh;
        }
        return nullptr;
    }
    DrawableItem* operator[](const DrawableItemIndex& drix) const
    {
        return _items.operator[](drix.index);
    }
    // must redefine this 
    DrawableItem* operator[](int ix) const
    {
        return  _items[ix];
    }

    void SetVisibility(DrawableItemIndex drix, bool visible)
    {
        SetVisibility(drix.index, visible);
    }
    void SetVisibility(int index, bool visible)
    {                     // _pQTree must not be null
        if (_items[index]->isVisible != visible)    // when it does change
            _items[index]->isVisible = visible;
        if (visible)
            _pQTree->Add(index);
        else
            _pQTree->Remove(index);
    }


    //    IndexVectorIterator Push_back(DrawableItem* pdrwi)
    int Push_back(DrawableItem* pdrwi)
    {
        int ix = _items.size();  // index of items to add
        if (pdrwi->zOrder < 0)
            pdrwi->zOrder = _pZorderStore->GetZorder(pdrwi->dtType == DrawableType::dtScreenShot);
        _items.push_back(pdrwi);
        // return _itemIndices.Insert(ix, pdrwi);
        return ix;
    }

    int CopyDrawable(DrawableItem& dri, QPointF topLeft) // adds copy of existing object on the heap
    {
        if(_pZorderStore) // then the copy's zOrder must be increased'
            dri.zOrder = _pZorderStore->GetZorder(dri.dtType == DrawableType::dtScreenShot);

        DrawableItem* pres = nullptr;
        switch (dri.dtType)
        {
            case DrawableType::dtDot:       pres = new DrawableDot((DrawableDot&)dri); break;
            case DrawableType::dtCross:     pres = new DrawableCross((DrawableCross&)dri); break;
            case DrawableType::dtEllipse:   pres = new DrawableEllipse((DrawableEllipse&)dri); break;
            case DrawableType::dtLine:      pres = new DrawableLine((DrawableLine&)dri); break;
            case DrawableType::dtRectangle: pres = new DrawableRectangle((DrawableRectangle&)dri); break;
            case DrawableType::dtScreenShot:pres = new DrawableScreenShot((DrawableScreenShot&)dri); break;
            case DrawableType::dtScribble:  pres = new DrawableScribble((DrawableScribble&)dri); break;
            case DrawableType::dtText:      pres = new DrawableText((DrawableText&)dri); break;
            default:  break;
        }
        if (pres)
        {
            pres->Translate(-topLeft, 0);
            return Push_back(pres);
        }

        return -1;
    }

    //DrawableItemIndex AddDrawable(DrawableItem* pdrh) // add original, 'pdrh' points to complete object (subclass) which MUST be created on the heap!
    int AddDrawable(DrawableItem* pdrh) // add original, 'pdrh' points to complete object (subclass) which MUST be created on the heap!
    {
        int ix = _items.size();  // index of items to add
        if (pdrh->zOrder < 0 && _pZorderStore)
            pdrh->zOrder = _pZorderStore->GetZorder(pdrh->dtType == DrawableType::dtScreenShot);
        _items.push_back(pdrh);
        if(_pQTree)
            _pQTree->Add(ix);
        return ix;
        //        IndexVectorIterator ivi = _itemIndices.Insert(ix, pdrh);
                //return ivi->second;
    }

    // create new object of given parameters on the heap
                                        // pimage must be set for dtScreenShot
    int AddDrawable(DrawableType dType, QPixmap *pimage=nullptr, FalconPenKind penKind = penBlack, qreal penWidth = 1, QPointF topLeft = QPointF(), QSizeF sizef = QSizeF(), bool isFilled = false)
    {
        int ivi;    // _pZorderStore must exist

        switch (dType)
        {
            case DrawableType::dtDot:       ivi = Push_back(new DrawableDot(topLeft, _pZorderStore->GetZorder(false), penKind, penWidth)); break;
            case DrawableType::dtCross:     ivi = Push_back(new DrawableCross(topLeft, sizef.width(), _pZorderStore->GetZorder(false), penKind, penWidth)); break;
            case DrawableType::dtEllipse:   ivi = Push_back(new DrawableEllipse(QRectF(topLeft, sizef), _pZorderStore->GetZorder(false), penKind, penWidth, isFilled)); break;
            case DrawableType::dtLine:      ivi = Push_back(new DrawableLine(topLeft, QPointF(sizef.width(),sizef.height()),_pZorderStore->GetZorder(false),penKind,penWidth) ); break;
            case DrawableType::dtRectangle: ivi = Push_back(new DrawableRectangle(QRectF(topLeft, sizef), _pZorderStore->GetZorder(false), penKind, penWidth, isFilled)); break;
            case DrawableType::dtScreenShot:ivi = Push_back(new DrawableScreenShot(topLeft, _pZorderStore->GetZorder(true), *pimage)); break;
            case DrawableType::dtScribble:  ivi = Push_back(new DrawableScribble(penKind, penWidth, _pZorderStore->GetZorder(false))); break;    // add points later
            case DrawableType::dtText:      ivi = Push_back(new DrawableText(topLeft, _pZorderStore->GetZorder(false))); break;                  // add text later
            default:  break;
        }
        if(_pQTree)
            _pQTree->Add(ivi);
        return ivi;

        //return ivi->second;
    }
    int AddDrawable(DrawableType dType, FalconPenKind penKind = penBlack, qreal penWidth = 1, QPointF topLeft = QPointF(), QSizeF sizef = QSizeF(), bool isFilled = false)
    {
        return AddDrawable(dType, nullptr, penKind, penWidth, topLeft, sizef, isFilled);
    }

    int Size(DrawableType type = DrawableType::dtNone) const
    {
        auto _size = [&](DrawableType dt)
        {
            int n = 0;
            for (auto a : _items)
                if (a->dtType == dt)
                    ++n;
            return n;
        };

        switch (type)
        {
            case DrawableType::dtNone: 
                return _items.size();
            default: return _size(type);
        }
    }
    void Erase(IntVector& iv)
    {
        for (int i = iv.size() - 1; i >= 0; --i)
        {
            delete _items.at(iv[i]);
            _items.removeAt(iv[i]);
        }
    }
    void Erase(int from)
    {
        for (int i = _items.size() - 1; i >= from; --i)
        {
            delete _items.at(i);
            _items.removeAt(i);
        }
    }
    QRectF ItemsUnder(QPointF point, IntVector& iv, DrawableType type = DrawableType::dtNone)
    {
        // copies indices for visible drawables under cursor into list
        // for non filled drawables when we are near the line, else even when inside
        // point is document relative
        QRectF rect;
        auto addIfNear = [&, this](DrawableItem* pdrw, int i)
        {
            if (pdrw->isVisible && pdrw->PointIsNear(point, pdrw->penWidth / 2.0 + 3))
            {
                iv.append(i); //  ({ pdrw->dtType, i, pdrw->zOrder });
                rect = rect.united(pdrw->Area());
            }
        };

        int dist = 50; // generous sized area
        QRectF r(point - QPointF(dist, dist), QSizeF(2 * dist, 2 * dist));
        IntVector iv1 = ListOfItemIndicesInRect(r);

        int siz = iv1.size();
        if (type == DrawableType::dtNone)
        {
            for (int i = 0; i < siz; ++i)
            {
                auto pdrw = _items[ iv1.at(i) ];
                addIfNear(pdrw, iv1[i]);
            }
        }
        else
        {
            for (int i = 0; i < siz; ++i)
            {
                auto pdrw = _items[ iv1.at(i) ];

                if (pdrw->dtType == type)
                    addIfNear(pdrw, iv1[i]);
            }
        }
        return rect;
    }
    int/*DrawableItemIndex*/ IndexOfTopMostItemUnder(QPointF point, int penWidth, DrawableType type = DrawableType::dtNone) const
    {
        if (point == QPoint(-1, -1))  // select topmost item
        {
            for (int i = _items.size() - 1; i >= 0; --i)
                if (_items[i]->dtType == type && _items[i]->isVisible)
                    return i;
            return -1;
        }

        // else create a vector with all of the items under an area
        // around the point

        DrawableItemIndex res;

        QRectF rect(point.x() - 5, point.y() - 5, 10, 10);
        IntVector iv = ListOfItemIndicesInRect(rect);

        if (type == DrawableType::dtNone)
        {
            for (int i = 0; i < iv.size(); ++i)
            {
                auto pdrw = _items[iv.at(i)];

                if (pdrw->isVisible && pdrw->PointIsNear(point, penWidth / 2.0 + 3))
                    if (pdrw->zOrder > res.zorder)
                    {
                        res.type = pdrw->dtType;
                        res.index = i;
                        res.zorder = pdrw->zOrder;
                    }
            }
        }
        else
        {
            int i;
            DrawableItem* pdrw;
            for (i = 0; i < iv.size(); ++i)
            {
                pdrw = _items[iv.at(i)];
                if (pdrw->isVisible && pdrw->dtType == type)
                    break;
            }

            if (i < iv.size())
            {
                res.index = iv.at(i);
                res.zorder = pdrw->zOrder;
                res.type = type;

                for (++i; i < iv.size(); ++i)
                {
                    pdrw = (*this)[iv.at(i)];
                    if (pdrw->isVisible && pdrw->PointIsNear(point, penWidth / 2.0 + 3))
                    {
                        if (pdrw->zOrder > res.zorder)
                        {
                            res.index = iv.at(i);
                            res.zorder = pdrw->zOrder;
                        }
                    }
                }
            }
        }
        return res.index;
    }
    DrawableScreenShot* FirstVisibleScreenShot(const QRectF& canvasRect);
    DrawableScreenShot* NextVisibleScreenShot();

    void Remove(int which)
    {
        if(_pQTree)
            _pQTree->Remove(which);                 // uses _pItems[which]

        _items.erase(_items.begin() + which);   // remove after not in use
    }
    void TranslateDrawable(int index, QPointF dr, qreal minY)
    {
        (*this)[index]->Translate(dr, minY);

    }
    void RotateDrawable(int index, MyRotation rot, QPointF &center)    // alpha used only for 'rotAngle'
    {
        (*this)[index]->Rotate(rot, center);
    }
    void VertShiftItemsBelow(int belowY, int deltaY);

private:
    int _imageIndex = -1;
    QRectF _canvasRect;
};

#endif