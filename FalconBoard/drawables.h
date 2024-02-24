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

#include "config.h"     // version info
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
    heAppendFiles,          // file(s) appended to current data
    heBackgroundLoaded,     // an image loaded into _background
    heBackgroundUnloaded,   // image unloaded from background
    heClearCanvas,          // visible image erased
    heEraserStroke,         // eraser stroke added
    heItemsDeleted,         // store the list of items deleted in this event
    heItemsPastedBottom,    // list of draw events added first drawn item is at index 'drawableItem'
    heItemsPastedTop,       // list of draw events added first drawn item is at index 'drawableItem'
                            // last one is at index 'lastScribbleIndex'
                            // Undo: set _indexLastScribbleItem to that given in previous history item
                            // Redo: set _indexLastScribbleItem to 'lastScribbleIndex'
    heMoveItems,
    hePenWidthChange,       // change pen widths for all drawables selected
    hePenColorChanged,      // redefine pen colors
    heRecolor,              // save old color, set new color
    heRotation,             // items rotated
    heSpaceDeleted,         // empty space is deleted
    heVertSpace,            // insert vertical space
                };

enum class DrawableType : int {
    dtNone,
    dtCross,            // an X as a marker
    dtDot,              // single dot
    dtEllipse,          // 
    dtLine,             // 2 endpoints
    dtRectangle,        // 4 courner points
    dtScreenShot,       // shown below any other drawable
    dtScribble,         // series of points from start to finish of scribble, eraser strokes added to  scribble
    dtText,

    dtDrawableTop = dtText,
    dtNonDrawableStart = 0x80,   // it is dealt with DrawableItem::operator>>() and DrawableItem::operator<<() 
    dtPen = dtNonDrawableStart,  // pen data for redefined pens
    dtNonDrawableEnd,            // end of drawables
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

            //----------------------------------------------------
            // ------------------- MyRotation::Type -------------
            //----------------------------------------------------
// Rotations and flips are not kommutative. However a flip followed by a rotation
// with an angle is the same as a rotation of -1 times the angle followed by the same
// flip. each flip is its own inverse and two different consecutive flips are
// equivalent to a rotation with 180 degrees 
struct MyRotation
{
    enum Type 
    {
        flipNone,                    // when there's no flip
        rotR90, rotL90, rot180,     // only used to set angle, but never used as 'flipType'
        rotFlipH, rotFlipV         // these leave the top left corner in place
    };

    qreal angle = 0.0;          // (in degrees) rotate by this (absolute for ellipse and rectangle) relative to prev. state to others
    Type flipType = flipNone;   // after rotated flip by this (may only be a flip or flipNone for no flip)

    static constexpr qreal AngleForType(Type myrot);
    static constexpr Type TypeForAngle(qreal angle);
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
        return EqZero(fmod(angle, 90.0));
    }
    inline bool HasNoRotation() const
    {
        return EqZero(angle);
    }

    inline void Reset() { angle = 0.0; flipType = flipNone; }

    MyRotation& operator=(Type typ); // sets angle and _tr too !
    MyRotation& operator=(qreal anAngle);    // sets flipType and _tr too !
      // when adding rotations the rotation added is either
      // a pure rotation by an angle or a pure flip
      // but never come these together!
    void AddAngle(qreal anAngle);        // sets _tr too

    void AddType(Type typ);

    // (flipNone, angle) = (angle, flipNone)
    // (  flip, angle)   = (-angle, flip)

    // (angle, flip)     + (aangle, flipNone) = (angle + aangle,  flip)
    // (angle, flipNone) + (    0,   aflip) = (angle,           aflip)
    // (angle, flip)     + (    0,   aflip) = (angle + (flip != aflip ? 180:0, flip + aflip)
    // (angle, flip)     + (aangle,  aflip) = (angle, flipNone) + (flip, aangle) + (0, aflip) = (angle - aangle,  flip + aflip)
    MyRotation AddRotation(MyRotation arot);
    inline void InvertAngle()
    {
        _SetRot((angle = -angle));
    }

    bool ResultsInNoRotation(MyRotation& arot, bool markIfNoRotation = false);    // returns true if it is a no-rotation state

    // in the following rotation functions it is assumed that any rectangle or image has sides parallel to the
    // coordinate axes. No such assumption is made for point and polygons
    //--------- order of rotations: first rotate by angle then flip. |angle| <= 180 
    bool RotateRect(QRectF& rect, QPointF center, bool noCheck = false) const; // assumes outer edge of rectangle is inside center

    bool RotatePoly(QPolygonF& points, QPointF center, bool noCheck = false) const;
    bool RotateSinglePoint(QPointF& pt, QPointF center, bool noCheck = false, bool invertRotation = false)  const;
                                   // only rotation can be inverted flips can't
    
    bool RotatePixmap(QPixmap& img, QPointF topLeft, QPointF center, bool noCheck = false)  const;
    bool RotateLine(QLineF& line, QPointF center, bool noCheck = false);
    bool IsNull() const { return !angle && flipType == flipNone; }
 private:
    QTransform _tr;
    void _SetRot(qreal alpha);
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
        globalDrawColors.SetDrawingPen(pk);
    }

    QColor PenColor() const 
    { 
        QColor penColor = globalDrawColors.Color(_penKind);
        return penColor; 
    }
    constexpr FalconPenKind PenKind() const { return _penKind; }
    void SetPainterPenAndBrush(QPainter* painter, const QRectF& clipR, QColor brushColor = QColor())
    {
        globalDrawColors.SetDrawingPen(_penKind);
        if(clipR.isValid())
            painter->setClipRect(clipR);  // clipR must be DrawArea relative

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
    static bool drawStarted;    // when unset then Draw() will call DrawWithErasers(), which sets this then
                                // calls Draw() to draw the drawable. When Draw() returns erasers are drawn
                                // over the points of the drawable

    DrawableType dtType = DrawableType::dtNone;
    QPointF refPoint;       // position of reference point relative to logical (0,0) of 'paper roll' 
                            // (widget coord: topLeft + DrawArea::_topLeft is used) 
                            // May be the first point or top left of area or the center point of the drawable!
    MyRotation rot;         // used to store the actual state of the rotations and check if rotation is possible
    int   zOrder = -1;      // not saved on disk. Drawables are saved from lowest zOrder to highest zOrder
    bool  isVisible = true;                              // not saved in file
    static qreal  yOffset;  // add this to every coordinates read including erasers
    // eraser strokes for a drawables. Oonly those parts of the eraser stroke that intersects the bounding rectangle of the
    // drawable plus an eraser pen width/2 wide margin are saved here. When drawing the drawable eraser strokes are
    // clipped to the bounding box of the drawable plus half of the pen width for scribbles rectangles and ellipses.
    // TODO: drawables completely under eraser should not be saved, erased part should not be considered part of drawable
    struct EraserData
    {
        int eraserPenWidth = 30;
        QPolygonF eraserStroke;
    };

    QVector<EraserData> erasers;   // a polygon(s) confined to the bounding rectangle of points for eraser strokes (above points)
                                 // may intersect the visible lines only because of the pen width!


    DrawableItem() = default;
    DrawableItem(DrawableType dt, QPointF refPoint, int zOrder = -1, FalconPenKind penKind = penBlack, qreal penWidth = 1.0) : dtType(dt), refPoint(refPoint), zOrder(zOrder), DrawablePen(penKind, penWidth) {}
    DrawableItem(const DrawableItem& other):DrawablePen(other) { *this = other; }
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
        p -= refPoint;
        return p.x() * p.x() + p.y() * p.y() < distance * distance;
    }

    virtual QRectF Area() const { return QRectF(); }
    virtual bool Intersects(QRectF rect) const
    {
        return Area().intersects(rect);
    }
    virtual QPointF GetLastDrawnPoint() const
    {
        return refPoint;
    }
    virtual bool CanTranslate(const QPointF dp) const
    { 
        QPointF p = refPoint + dp;
        return p.x() >=0 && p.y() >= 0;
    }
    virtual bool CanRotate(MyRotation rot, const QPointF center) const
    {
        QPointF pt = refPoint - center;
        QTransform tr;
        tr.rotate(rot.angle);
        pt = tr.map(pt) + center;
        return pt.x() >= 0 && pt.y() >= 0;
    }

    virtual bool Translate(QPointF dr, qreal minY);            // only if not deleted and top is > minY. Override this only for scribbles

    virtual bool Rotate(MyRotation rot, QPointF center);    // alpha used only for 'rotAngle'
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
    virtual void Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)
    {
        if (drawStarted)
        {
            // draw normally using 'painter',clipR (if valid) and 'topLeftOfVisibleArea'
            SetPainterPenAndBrush(painter, clipR.translated(-topLeftOfVisibleArea));
            QPointF pt = refPoint - topLeftOfVisibleArea;
            painter->drawPoint(pt);
            // this example draws a single point (good for DrawableDot)
        }
        else
            DrawWithEraser(painter, topLeftOfVisibleArea, clipR);
    }
    /*=============================================================
     * TASK:    common (not overridden) function for every subclass
     * PARAMS:  same one as for Draw()
     * GLOBALS: drawStarted
     * RETURNS:
     * REMARKS: - sets drawStated to true then  calls back
     *              the function that called it then draws the eraser strokes
     *              inside the clipping rectangle
     *------------------------------------------------------------*/
    void DrawWithEraser(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)
    {
        drawStarted = true;

        if (erasers.size())                 // then paint object first then erasers on separate pixmap 
        {                                   // and copy the pixmap to the visible area
            QRectF area = Area();           // includes half of pen width (none for screenshots)
            //QPixmap pxm(area.size().toSize());
            QImage pimg(area.width(), area.height(), QImage::Format_ARGB32);
            pimg.fill(Qt::transparent);
            QPainter myPainter(&pimg);
            QPointF tl = area.topLeft();
            //if (dtType != DrawableType::dtScreenShot)
            //    tl -= QPointF(penWidth, penWidth) / 2.0;
            Draw(&myPainter, tl,clipR);     // calls painter of subclass, 
                                            // which must check 'drawStarted' and 
                                            // must not call this function if it is set
                                            // clipRect is not important as the pixmap is exactly the right size
            // DEBUG
            // pimg.save("pimg-before.png", "png");
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

            for (auto &er : erasers)
            {
                QPen pen(QPen(Qt::black, er.eraserPenWidth, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));
                myPainter.setPen(pen);
                if (er.eraserStroke.size() == 1)
                    myPainter.drawPoint(er.eraserStroke.at(0) - tl);
                else
                    myPainter.drawPolyline(er.eraserStroke.translated(-tl));    // do not close path
            }
            painter->setClipRect(clipR.translated(-topLeftOfVisibleArea) );
            painter->drawImage(tl - topLeftOfVisibleArea, pimg);
            //painter->drawPixmap(tl - topLeftOfVisibleArea, pimg);
            // DEBUG
            //            pimg.save("pimg.png", "png");
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
struct DrawableCross : public DrawableItem      // refPoint is the center
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
    bool CanTranslate(const QPointF dr) const override;
    bool CanRotate(MyRotation rot, QPointF center) const override;
    bool Translate(QPointF dr, qreal minY) override;            // only if not deleted and top is > minY. Override this only for scribbles
    bool Rotate(MyRotation rot, QPointF center) override;    // alpha used only for 'rotAngle'
    virtual MyRotation::Type RotationType() { return MyRotation::flipNone;  /* yet */ }
    QRectF Area() const override;    // includes half of pen width+1 pixel
    void Draw(QPainter* painter, QPointF startPosOfVisibleArea, const QRectF& clipR) override;
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
    DrawableDot(QPointF pos, int zorder, FalconPenKind penKind, qreal penWidth) : DrawableItem(DrawableType::dtDot, pos, zorder, penKind, penWidth) { }
    DrawableDot(const DrawableDot& o) = default;
    ~DrawableDot() = default;
    QRectF Area() const override    // includes half od pen width+1 pixel
    { 
        qreal d = penWidth / 2.0 + 1.0;
        return QRectF(refPoint - QPointF(d, d), QSize(2*d, 2*d) ); 
    }
    // in base class bool PointIsNear(QPointF p, qreal distance) const override// true if the point is near the circumference or for filled ellpse: inside it
};
QDataStream& operator<<(QDataStream& ofs, const DrawableDot& di);
QDataStream& operator>>(QDataStream& ifs,       DrawableDot& di);  // call AFTER header is read in

            //----------------------------------------------------
            // ------------------- Drawable Ellipse -------------
            //----------------------------------------------------

struct DrawableEllipse : public DrawableItem    // refPoint is the center
{
    QRectF rect;                // determines the two axes and the center of the ellipse
    bool isFilled=false;        // whether closed polygon (ellipse or rectangle) is filled

    DrawableEllipse() : DrawableItem()
    {
        dtType = DrawableType::dtEllipse;
    }
    DrawableEllipse(QRectF rect, int zorder, FalconPenKind penKind, qreal penWidth, bool isFilled = false);
    DrawableEllipse(QPointF refPoint, qreal radius, int zorder, FalconPenKind penKind, qreal penWidth, bool isFilled = false);    // circle
    DrawableEllipse(QPointF refPoint, qreal a, qreal b, int zorder, FalconPenKind penKind, qreal penWidth, bool isFilled = false); // ellipse with principlal half-axes a and b;
    DrawableEllipse(const DrawableEllipse& o);
    DrawableEllipse(DrawableEllipse&& o) noexcept;
    ~DrawableEllipse() = default;
    DrawableEllipse &operator=(const DrawableEllipse& o);
    DrawableEllipse &operator=(DrawableEllipse&& o) noexcept;

    inline bool IsCircle() const { return rect.width() == rect.height(); }
    bool CanTranslate(const QPointF dr) const override;
    bool CanRotate(MyRotation rot, QPointF center) const override;

    bool Translate(QPointF dr, qreal minY) override;                   // only if not deleted and top is > minY
    bool Rotate(MyRotation rot, QPointF centerOfRotation) override;    // alpha used only for 'rotAngle'
    QRectF Area() const override // includes half of pen width+1 pixel
    {
        qreal d = penWidth / 2.0 + 1.0;
        return _rotatedRect.adjusted(-d,-d,d,d); 
    }
    QPointF GetLastDrawnPoint() const override
    {
        return  refPoint + QPointF(_rotatedRect.width(), _rotatedRect.height() / 2.0);
    }

    bool PointIsNear(QPointF p, qreal distance) const override// true if the point is near the circumference or for filled ellipse: inside it
    {
        if (!_rotatedRect.contains(p))
            return false;

        QPointF center = rect.center();     // unrotated / simple rotated ellipse
        // first, if ellipse is rotated, rotate point by -angle 
        // into the original (unrotated) ellipse based coord system
        // flips don't matter
        if (!EqZero(rot.angle))
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
    virtual void Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR) override;
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
            // ------------------- Drawable line -------------
            //----------------------------------------------------
struct DrawableLine : public DrawableItem
{
    QPointF endPoint;

    DrawableLine() : DrawableItem()
    {
        dtType = DrawableType::dtLine;
    }
    DrawableLine(QPointF refPoint, QPointF endPoint, int zorder, FalconPenKind penKind, qreal penWidth);
    DrawableLine(const DrawableLine& ol);
    DrawableLine(DrawableLine&& ol);
    ~DrawableLine() {}
    DrawableLine& operator=(const DrawableLine& ol);
    DrawableLine& operator=(const DrawableLine&& ol);
    bool CanTranslate(const QPointF dr) const override;
    bool CanRotate(MyRotation rot, QPointF center) const override;
    bool Translate(QPointF dr, qreal minY) override;            // only if not deleted and top is > minY
    bool Rotate(MyRotation rot, QPointF center) override;    // alpha used only for 'rotAngle'
    QRectF Area() const override// includes half of pen width+1 pixel
    {
        QRectF rect = QRectF(refPoint, QSize((endPoint - refPoint).x(), (endPoint - refPoint).y()) ).normalized();
        qreal d = penWidth / 2.0 + 1.0;
        return rect.adjusted(-d, -d, d, d);
    }
    QPointF GetLastDrawnPoint() const override
    {
        return endPoint;
    }
    bool PointIsNear(QPointF p, qreal distance) const override;
    void Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR) override;
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

    bool CanTranslate(const QPointF dr) const override;
    bool CanRotate(MyRotation rot, QPointF center) const override;
    bool Translate(QPointF dr, qreal minY) override;            // only if not deleted and top is > minY
    bool Rotate(MyRotation rot, QPointF center) override;    // alpha used only for 'rotAngle'
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

        if(!EqZero(rot.angle) && !rot.HasSimpleRotation() )
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
    void Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR) override;
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
struct DrawableScreenShot : public DrawableItem     // for a screenshot refPoint is the center of the image!
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
        _rotatedArea = QRectF(refPoint, image.size());
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

    bool CanTranslate(const QPointF dr) const override;
    bool CanRotate(MyRotation rot, QPointF center) const override;

    bool Translate(QPointF dr, qreal minY) override;            // only if not deleted and top is > minY
    bool Rotate(MyRotation rot, QPointF center);
    bool PointIsNear(QPointF p, qreal distance) const override // true if the point is inside the image
    {
        QRectF rect = QRectF(refPoint - QPointF(_image.size().width() / 2, _image.size().height() / 2), _image.size());
        if (!EqZero(rot.angle) && !rot.HasSimpleRotation())
        {
            QTransform tr;
            tr.rotate(-rot.angle);
            QPointF center = _rotatedArea.boundingRect().center();
            p = tr.map(p - center) + center;
        }
        return rect.contains(p);
    }
    void Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR) override;    // screenshot are painted in paintEvent first followed by other drawables
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

    bool CanTranslate(const QPointF dr) const override;
    bool CanRotate(MyRotation rot, QPointF center) const override;

    bool Translate(QPointF dr, qreal minY) override;    // only if not deleted and top is > minY
    bool Rotate(MyRotation rot, QPointF center) override;    // alpha used only for 'rotAngle'
    QRectF Area() const override // includes half od pen width+1 pixel
    {
        qreal d = penWidth / 2.0 + 1.0;
        return points.boundingRect().adjusted(-d, -d, d, d);
    }
    QPointF GetLastDrawnPoint() const override
    {
        return points[points.size() - 1];
    }

    bool PointIsNear(QPointF p, qreal distance) const override;// true if the point is near the circumference or for filled ellpse: inside it
    void Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR) override;
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

//    bool Translate(QPointF dr, qreal minY) override;    // only if not deleted and top is > minY
//    void Rotate(MyRotation rot, QRectF center, qreal alpha=0.0) override;    // alpha used only for 'rotAngle'
    QRectF Area() const override { return QRectF();  /* TODO ??? */ }
    bool PointIsNear(QPointF p, qreal distance) const override // true if the point is near the circumference or for filled ellpse: inside it
    {
        return false; // ???
    }
    void Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR) override;
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
        return GetValues(pItems, AreaForQRect(area));

    }
    IntVector GetValues(const DrawableItemList* pItems, const QuadArea& area) const
    {

        auto sortFunc = [&](int i, int j) 				// returns true if item 'i' comes before item 'j'
        {
            DrawableItem* pdri1 = (*pItems)[i], * pdri2 = (*pItems)[j];
            // no two items may have the same z-order
            bool zOrder1Smaller = pdri1->zOrder < pdri2->zOrder;

            if (zOrder1Smaller)	// zOrder OK
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
    DrawableItemList _items,              // pointers to drawables in memory
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

    DrawableItemList &Items() { return _items; }

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
    QPointF BottomRightLimit(QSize screenSize);

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
        int ivi=-1;    // _pZorderStore must exist

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
    int/*DrawableItemIndex*/ IndexOfTopMostItemInlist(IntVector &iv) const
    {
        int j = 0;
        for (int i = 0, zOrder = 0; i < iv.size(); ++i)
        {
            int zo = _items[iv.at(i)]->zOrder;
            if (zo > zOrder)
            {
                j = i;
                zOrder = zo;
            }
        }
        return j;

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
        if (iv.isEmpty())
            return res.index; // -1

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
            DrawableItem* pdrw=nullptr;
            for (i = 0; i < iv.size(); ++i)
            {
                pdrw = _items[iv.at(i)];
                if (pdrw->isVisible && pdrw->dtType == type && pdrw->PointIsNear(point, penWidth / 2.0 + 3))
                    break;
            }

            if (i < iv.size())
            {
                res.index = iv.at(i);
                res.zorder = pdrw->zOrder;
                res.type = type;

                for (++i; i < iv.size(); ++i)
                {
                    pdrw = _items[iv.at(i)];
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
            else
                res.index = -1;
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
    bool MoveItems(QPointF dr, const DrawableIndexVector& drl)
    {
        for (auto& a : drl)
            if (!(*this)[a]->CanTranslate(dr))
                return false;
        for (auto& a : drl)
            (*this)[a]->Translate(dr,-1);
        return true;
    }
    bool TranslateDrawable(int index, QPointF dr, qreal minY)
    {
        return (*this)[index]->Translate(dr, minY);

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