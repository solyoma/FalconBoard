#pragma once
#ifndef _HISTORY_H
#define _HISTORY_H


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
    heRecolor,         // save old color, set new color
    heItemsDeleted,         // store the list of items deleted in this event
    heSpaceDeleted,         // empty space is deleted
    heVertSpace,            // insert vertical space
    heClearCanvas,          // visible image erased
    heBackgroundLoaded,     // an image loaded into _background
    heBackgroundUnloaded,   // image unloaded from background
    heItemsPastedBottom,// list of draw events added first drawn item is at index 'drawableItem'
    heItemsPastedTop    // list of draw events added first drawn item is at index 'drawableItem'
                        // last one is at index 'lastScribbleIndex'
                        // Undo: set _indexLastScribbleItem to that given in previous history item
                        // Redo: set _indexLastScribbleItem to 'lastScribbleIndex'
                };

enum class DrawableType {
    dtNone,
    dtCross,            // an X as a marker
    dtDot,              // single dot
    dtEllipse,          // - " -
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
enum MyRotation { rotNone, 
                  rotR90, rotL90, rot180, rotFlipH, rotFlipV,     // these leave the top left corner in place
                  rotAlpha                                        // alpha: around the center of the bounding box, increases counterclockwise
                };
enum MyLayer { 
                mlyBackgroundImage,         // lowest layer: background image
                mlyScreenShot,              // screenshot layer: may write over it
                mlyScribbleItem,            // scribbles are drawn here
                mlySprite                   // layer for sprites (moveable images)
             };

            //----------------------------------------------------
            // ------------------- helper -------------
            //----------------------------------------------------
static bool __IsLineNearToPoint(QPointF p1, QPointF p2, QPointF& ccenter, qreal r)   // line between p2 and p1 is inside circle w. radius r around point 'point'
{
#define SQR(x)  (x)*(x)
#define DIST2(a,b) SQR((a).x() - (b).x()) + SQR((a).y() - (b).y())
    // first transform coord system origin to center of circle 
    p1 -= ccenter;
    p2 -= ccenter;
    // then solve the equations for a line given by p1 and p2 to intersect a circle w. radius r
    qreal dx = p2.x() - p1.x(),
        dy = p2.y() - p1.y(),
        dr = dx * dx + dy * dy,
        D = p1.x() * p2.y() - p2.x() * p1.y(),
        discr = SQR(r) * SQR(dr) - SQR(D),
        pdr2 = 1 / SQR(dr);

    if (discr < 0)   // then the line does not intersect the circle
        return false;
    else
    {
        const qreal eps = 1e-3;
        if (discr < eps)  // floating point, no abs. 0 test
        {                       // tangent line. Check tangent point.
            qreal xm = (D * dy) * pdr2,
                ym = -(D * dx) * pdr2;
            return (SQR(xm) + SQR(ym) <= SQR(r));
        }
        else    // one or two intersections with the line, but is there any for the line section?
        {
            if (SQR(p1.x()) + SQR(p1.y()) <= SQR(r) || SQR(p1.x()) + SQR(p1.y()) <= SQR(r))
                return true;    // at least one of the endpoints inside circle

            // not so easy... Get intersection points ip1,ip2
            qreal sqrt_discr = sqrt(discr),
                xm1 = (D * dy) * pdr2,
                xm2 = (dx * sqrt_discr * (dy < 0 ? -1 : 1)) * pdr2,
                ym1 = -(D * dx) * pdr2,
                ym2 = abs(dy) * sqrt_discr * pdr2;
            // the two intersection points are:
            QPointF ip1 = QPointF(xm1 + xm2, ym1 + ym2),
                ip2 = QPointF(xm1 - xm2, ym1 - ym2);
            // these 4 points are on the same line (or less than 'eps' distance from it)
            // The order of points on the line must be:
            if (p1.x() > p2.x())
                std::swap<QPointF>(p1, p2);
            if (ip1.x() > ip2.x())
                std::swap<QPointF>(ip1, ip2);

            // now p1x() <= p2x() and ip1.x() <= ip2.x()
            // when order along the line is p1, ip1, pip2 p2 then the line section
            // intersects the circle

            // check horizontal line
            if (abs(p2.y() - p1.y()) < eps)
                return p1.x() <= ip1.x() && ip2.x() <= p2.x();
            //// check vertical line
            //if (abs(p2.x() - p1.x()) < eps)
            return (p1.y() <= ip1.y() && ip2.y() <= p2.y()) ||
                (p1.y() >= ip1.y() && ip2.y() >= p2.y());
        }
    }
#undef SQR
#undef DIST2
};




            //----------------------------------------------------
            // ------------------- Drawable Pen -------------
            //----------------------------------------------------
// stores the coordinates of line strokes from pen down to pen up:
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
    FalconPenKind PenKind() const { return _penKind; }
    void SetPainterPenAndBrush(QPainter* painter, const QRectF& clipR = QRectF(), QColor brushColor = QColor())
    {
        QPen pen( PenColor() );
        pen.setWidth(penWidth);
        painter->setPen(pen);
        if (brushColor.isValid())
            painter->setBrush(QBrush(brushColor));
        painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
        if(clipR.isValid())
            painter->setClipRect(clipR);
    }
};

            //----------------------------------------------------
            // ------------------- Drawable Header -------------
            //----------------------------------------------------
struct DrawableItem : public DrawablePen                               
{
    static bool drawStarted;

    DrawableType dtType = DrawableType::dtNone;
    QPointF startPos;       // relative to logical (0,0) of 'paper roll' (widget coord: topLeft + DrawArea::_topLeft is used) 
    qreal angle = 0.0;      // rotate by this in degree
    int   zOrder = 0;                                    // not saved. Drawables are saved from lowest zOrder to highest zOrder
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
    DrawableItem(DrawableType dt, QPointF startPos, int zOrder = -1, FalconPenKind penKind=penBlack, qreal penWidth=1.0) : dtType(dt), startPos(startPos), zOrder(zOrder), DrawablePen(penKind, penWidth){}
    DrawableItem(const DrawableItem& other) { *this = other; }
    DrawableItem& operator=(const DrawableItem& other);

    virtual void AddEraserStroke(int eraserWidth, const QPolygonF &eraserStroke);
    virtual void RemoveLastEraserStroke(EraserData* andStoreHere = nullptr);

    bool IsVisible() const { return isVisible; }
    bool IsImage() const { return dtType == DrawableType::dtScreenShot; }
    virtual bool IsFilled() const { return false; }
    virtual bool PointIsNear(QPointF p, qreal distance) const { return false; }// true if the point is near the circumference or for filled ellpse: inside it

    virtual QRectF Area() const { return QRectF();  }
    virtual bool Intersects(QRectF rect) const
    {
        return Area().intersects(rect);
    }
    virtual QPointF GetLastDrawnPoint() const
    { 
        return startPos; 
    }
    virtual void Translate(QPointF dr, qreal minY);            // only if not deleted and top is > minY. Override this only for scribbles
    virtual void Rotate(MyRotation rot, QRectF inThisrectangle, qreal alpha=0.0);    // alpha used only for 'rotAlpha'
    /*=============================================================
     * TASK: Function to override in all subclass
     * PARAMS:  painter              - existing painter
     *          topLeftOfVisibleArea - document relative coordinates of visible part
     *          clipR                - clipping rectangle used for eraser
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

        if (erasers.size())                 // then paint object with erasers on separate pixmap 
        {
            QPixmap pxm(Area().size().toSize());
            QPainter myPainter(&pxm);       
            Draw(&myPainter, topLeftOfVisibleArea);     // calls painter of subclass, 
                                            // which must check 'drawStarted' and 
                                            // must not call this function if it is set
                                            // clipRect is not important as the pixmap is exactly the right size
            // and now the erasers
            myPainter.setCompositionMode(QPainter::CompositionMode_Clear);

            for (auto er : erasers)
            {
                QPen pen(Qt::black);
                pen.setWidth(er.eraserPenWidth);
                myPainter.setPen(pen);
                myPainter.drawPolyline(er.eraserStroke);    // do not close path
            }
            painter->drawPixmap(topLeftOfVisibleArea, pxm);
        }
        else
            Draw(painter, topLeftOfVisibleArea, clipR);

        drawStarted = false;
    }
 };

inline QDataStream& operator<<(QDataStream& ofs, const DrawableItem& di);
inline QDataStream& operator>>(QDataStream& ifs, DrawableItem& di);

            //----------------------------------------------------
            // ------------------- Drawable Cross -------------
            //----------------------------------------------------
struct DrawableCross : public DrawableItem
{
    qreal length;                 // total length of lines intersecting at 45 degree

    DrawableCross()
    {
        dtType = DrawableType::dtCross;
    }
    DrawableCross(QPointF pos, qreal len, int zorder, FalconPenKind penKind, qreal penWidth);
    DrawableCross(const DrawableCross& o) = default;
    ~DrawableCross() = default;
    void Rotate(MyRotation rot, QRectF inThisrectangle, qreal alpha=0.0) override;    // alpha used only for 'rotAlpha'
    QRectF Area() const override    // includes half od pen width+1 pixel
    { 
        qreal d = penWidth / sqrt(2.0);
        return QRectF(startPos - QPointF(d, d), QSize(2*d, 2*d) ); 
    }
    void Draw(QPainter* painter, QPointF startPosOfVisibleArea, const QRectF& clipR = QRectF()) override;
    bool PointIsNear(QPointF p, qreal distance) const override// true if the point is near the circumference or for filled ellpse: inside it
    {
        p -= startPos;
        return p.x() * p.x() + p.y() * p.y() < distance * distance;
    }
};
inline QDataStream& operator<<(QDataStream& ofs, const DrawableCross& di);
inline QDataStream& operator>>(QDataStream& ifs,       DrawableCross& di);  // call AFTER header is read in

            //----------------------------------------------------
            // ------------------- Drawable Dot -------------
            //----------------------------------------------------
struct DrawableDot : public DrawableItem
{
    DrawableDot()
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
    void Draw(QPainter* painter, QPointF startPosOfVisibleArea, const QRectF& clipR = QRectF()) override;
    bool PointIsNear(QPointF p, qreal distance) const override// true if the point is near the circumference or for filled ellpse: inside it
    {
        p -= startPos;
        return p.x() * p.x() + p.y() * p.y() < distance * distance;
    }
};
inline QDataStream& operator<<(QDataStream& ofs, const DrawableDot& di);
inline QDataStream& operator>>(QDataStream& ifs,       DrawableDot& di);  // call AFTER header is read in

            //----------------------------------------------------
            // ------------------- Drawable Ellipse -------------
            //----------------------------------------------------
struct DrawableEllipse : public DrawableItem
{
    QRectF rect;
    bool isFilled=false;        // wheather closed polygon (ellipse or rectangle) is filled

    DrawableEllipse()
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
    void Rotate(MyRotation rot, QRectF inThisrectangle, qreal alpha=0.0) override;    // alpha used only for 'rotAlpha'
    QRectF Area() const override // includes half od pen width+1 pixel
    {
        qreal d = penWidth / 2.0 + 1.0;
        return rect.adjusted(-d,-d,d,d); 
    }
    QPointF GetLastDrawnPoint() const override
    {
        return  startPos + QPointF(rect.width(), rect.height() / 2.0);
    }

    bool PointIsNear(QPointF p, qreal distance) const override// true if the point is near the circumference or for filled ellipse: inside it
    {
        QPointF center = rect.center();
        // transform center to origin and point with it
        p = p - center;         // point P(xp,yp)

        qreal a = rect.width() / 2.0, b = rect.height() / 2,    // major and minor axes of ellipse
            a2 = a * a, b2 = b * b,                             // their squares
            xp2=p.x()*p.x(), yp2=p.y()*p.y();                   // and squares of coords of point P
        
        qreal xq2, yq2; // square of coordinates of point Q (xq,yq), where line between p and center intersects the circumference
        xq2 = a2 * b2 * xp2 / (xp2 * b2 + yp2 * a2);
        yq2 = b2 * (1 - xq2 / a2);
        // square of distances of P and Q from origin
        qreal distP2 = xp2 + yp2, distQ2 = xq2 + yq2;
                    // cheating dist(P+distance) is not this
        if (isFilled)
            return distQ2 <= distP2 + distance * distance;
        return (distP2 <= distQ2 && distP2 >= distQ2 - distance * distance) ||
                (distP2 <= distQ2 + distance * distance);
    }
    virtual void Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR = QRectF()) override;
};
inline QDataStream& operator<<(QDataStream& ofs, const DrawableEllipse& di);
inline QDataStream& operator>>(QDataStream& ifs,       DrawableEllipse& di);  // call AFTER header is read in

            //----------------------------------------------------
            // ------------------- Drawable Rectangle -------------
            //----------------------------------------------------
struct DrawableRectangle : public DrawableItem
{
    QRectF rect;
    bool isFilled=false;            // wheather closed polynom (ellipse or rectangle) is filled

    DrawableRectangle()
    {
        dtType = DrawableType::dtRectangle;
    }
    DrawableRectangle(QRectF rect, int zorder, FalconPenKind penKind, qreal penWidth, bool isFilled = false);
    DrawableRectangle(const DrawableRectangle& o);
    ~DrawableRectangle() = default;
    DrawableRectangle& operator=(const DrawableRectangle& di);
    DrawableRectangle& operator=(DrawableRectangle&& di) noexcept;

    void Translate(QPointF dr, qreal minY) override;            // only if not deleted and top is > minY
    void Rotate(MyRotation rot, QRectF inThisrectangle, qreal alpha=0.0) override;    // alpha used only for 'rotAlpha'
    QRectF Area() const override// includes half od pen width+1 pixel
    {
        qreal d = penWidth / 2.0 + 1.0;
        return rect.adjusted(-d, -d, d, d);
    }
    QPointF GetLastDrawnPoint() const override
    {
        return rect.bottomRight();
    }

    bool PointIsNear(QPointF p, qreal distance) const override// true if the point is near the circumference or for filled rectangle: inside it
    {                                                           // the two axes are parallel to x,y
        bool b = (p.x() >= rect.x() - distance) && (p.y() <= rect.right() + distance) &&
            (p.y() >= rect.y() - distance) && (p.y() <= rect.bottom() + distance);

        if (b)
            return b;

        if (isFilled)
            return rect.contains(p);

        return ( 
                 (
                     (p.x() >= rect.left() -  distance && p.x() <= rect.left() +  distance) ||
                     (p.x() >= rect.right() - distance && p.x() <= rect.right() + distance) 
                 ) && 
                     (p.y() >= rect.top() -   distance && p.y() <= rect.bottom() +distance)
               ) || 
               (
                (
                    (p.y() >= rect.top() - distance && p.y() <= rect.top() + distance) ||
                    (p.y() >= rect.bottom() - distance && p.y() <= rect.bottom() + distance)
                    ) &&
                (p.x() >= rect.left() - distance && p.x() <= rect.right() + distance)
                );
    }
    void Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR = QRectF()) override;
};
inline QDataStream& operator<<(QDataStream& ofs, const DrawableRectangle& di);
inline QDataStream& operator>>(QDataStream& ifs,       DrawableRectangle& di);  // call AFTER header is read in

            //----------------------------------------------------
            // ------------------- Drawable Screenshot -------------
            //----------------------------------------------------
 /*-------------------------------------------------------*/
struct DrawableScreenShot : public DrawableItem
{
    QPixmap image;      // screenshot image

    DrawableScreenShot()
    {
        dtType = DrawableType::dtScreenShot;
    }
    DrawableScreenShot(const DrawableScreenShot& other) : DrawableItem(other)
    {
        image = other.image;
    }
    DrawableScreenShot(QPointF topLeft, int zOrder, const QPixmap &image) : DrawableItem(DrawableType::dtScreenShot, topLeft, zOrder), image(image) {}
    ~DrawableScreenShot() = default;

    void AddImage(QPixmap& image);

    const QPixmap& Image() const { return image; }

    inline QRectF Area() const;
    // canvasRect relative to paper (0,0)
    // result: relative to image
    // isNull() true when no intersection with canvasRect
    QRectF AreaOnCanvas(const QRectF& canvasRect) const;
    void Rotate(MyRotation rot, QRectF encRect, qreal alpha = 0.0);    // only used for 'rotAlpha'
    bool PointIsNear(QPointF p, qreal distance) const override // true if the point is near the circumference or for filled ellpse: inside it
    {
        return Area().contains(p);
    }
    void Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR = QRectF()) override;    // screenshot are painted in paintEvent first followed by other drawables
};

inline QDataStream& operator<<(QDataStream& ofs, const  DrawableScreenShot& di);
inline QDataStream& operator>>(QDataStream& ifs,        DrawableScreenShot& di);           // call AFTER header is read in

            //----------------------------------------------------
            // ------------------- Drawable Scribble -------------
            //----------------------------------------------------
struct DrawableScribble   : public DrawableItem     // drawn on layer mltScribbleItem
{                   
    QPolygonF points;         // coordinates are relative to logical origin (0,0) => canvas coord = points[i] - origin

    DrawableScribble()
    {
        dtType = DrawableType::dtScribble;
    }
    DrawableScribble(FalconPenKind penKind, qreal penWidth, int zorder) noexcept;
    DrawableScribble(const DrawableScribble& di);
    DrawableScribble(DrawableScribble&& di) noexcept;
    DrawableScribble& operator=(const DrawableScribble& di);

    DrawableScribble& operator=(DrawableScribble&& di)  noexcept;

    DrawableType Type() const 
    {
        return dtType;
    }

    void clear();       // clears points

    static bool IsExtension(const QPointF& p, const QPointF& p1, const QPointF& p2 = QPoint()); // vectors p->p1 and p1->p are parallel?

    void add(QPointF p);          // must use SetBoundingRectangle after all points adedd
    void add(int x, int y);      // - " - 
    void Smooth();               // points

    bool intersects(const QRectF& arect) const;



    void Translate(QPointF dr, qreal minY) override;    // only if not deleted and top is > minY
    void Rotate(MyRotation rot, QRectF inThisrectangle, qreal alpha=0.0) override;    // alpha used only for 'rotAlpha'
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

inline QDataStream& operator<<(QDataStream& ofs, const DrawableScribble& di);
inline QDataStream& operator>>(QDataStream& ifs, DrawableScribble& di);           // call AFTER header is read in

            //----------------------------------------------------
            // ------------------- Drawable Text -------------
            //----------------------------------------------------
struct DrawableText : public DrawableItem
{
    QString _text;                  // so that a text() fucntion can be created
    QString fontAsString;           // list of all properties separated by commas

    DrawableText() = default;       // default constructor            TODO
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

    inline void setFont(QString font) { fontAsString = font; }

//    void Translate(QPointF dr, qreal minY) override;    // only if not deleted and top is > minY
//    void Rotate(MyRotation rot, QRectF inThisrectangle, qreal alpha=0.0) override;    // alpha used only for 'rotAlpha'
    QRectF Area() const override { return QRectF();  /* TODO ??? */ }
    bool PointIsNear(QPointF p, qreal distance) const override // true if the point is near the circumference or for filled ellpse: inside it
    {
        return false; // ???
    }
    void Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR = QRectF()) override;
};

inline QDataStream& operator<<(QDataStream& ofs, const DrawableText& di);
inline QDataStream& operator>>(QDataStream& ifs,       DrawableText& di);           // call AFTER header is read in


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
    inline void Add(int ix) 
    { 
        if(pItemTree) 
            pItemTree->Add(ix); 
    }
    inline void Remove(int ix) 
    { 
        if(pItemTree) 
            pItemTree->Remove(ix); 
    }
    IntVector GetValues(const DrawableItemList* pItems, const QRectF& area) const
    {
        extern QuadArea AreaForQRect(QRectF rect);

        auto sortFunc = [&](int i, int j) 				// returns true if item 'i' comes before item 'j'
        {
            DrawableItem* pdri1 = (*pItems)[i], * pdri2 = (*pItems)[j];
            // no two items may have the same z-order
            bool zOrderOk = pdri1->zOrder < pdri2->zOrder;

            if (zOrderOk)	// zOrder OK
                return true;
            else                         // zorder i >= zorder j
            //{
            //    if (!pdri1->Area().intersects(pdri2->Area()))
            //        if (pdri1->startPos.y() < pdri2->startPos.y())
            //            return true;
            //        else if (pdri1->startPos.y() == pdri2->startPos.y() && pdri1->startPos.x() < pdri2->startPos.x())
            //            return true;
            //        else
            //            return false;
            //}
            return false;
        };

        std::vector<int> iv = pItemTree->GetValues(AreaForQRect(area));	// only visible elements!
        std::sort(iv.begin(), iv.end(), sortFunc);
        IntVector resv = QVector<int>(iv.begin(), iv.end()); //  ::fromStdVector(iv);	<- deprecated
        return resv;
    }
    inline void Resize(QuadArea area, const int* pNewItem = nullptr, bool savePrevVals = true)
    {
        return pItemTree->Resize(area, pNewItem, savePrevVals);
    }
    inline int BottomItem()
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
    }

    inline DrawableItemList* Items() { return &_items; }

    inline void ResetZorder()
    {
        _pZorderStore->Reset();
    }

    void Clear()
    {
        for (auto a : _items)
                delete a;
        _items.clear();
        if(_pQTree)
            _pQTree->Clear();
        if(_pZorderStore)
                ResetZorder();
    }

    void Undo(int n = 1)    // moves last n items to _redoItems w.o. deleting them
    {
        for (int i = _items.size() - 1; n && i != -1; --i,--n)
        {
            if (_pQTree)
                _pQTree->Remove(i);
            _redoItems.push_back(_items.at(i));
            _items.pop_back();
        }
    }
    void Redo(int n = 1)
    {
        int k = _items.size();
        for (int i = _redoItems.size() - 1; n && i != -1; --i,--n)
        {
            _items.push_back(_redoItems.at(i));
            _redoItems.pop_back();

            if (_pQTree)
                _pQTree->Add(k++);
        }
        
    }
    void ClearRedo()
    {
        for (auto a : _redoItems)
            delete a;
        _redoItems.clear();
    }

    int Count(const QuadArea& area = QuadArea())
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

    inline QuadArea Area() { return _pQTree ? _pQTree->Area() : QuadArea(); }

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
        //        for (++_runningIndex; _runningIndex != _itemIndices.end(); ++_runningIndex)
        for (++_runningIndex; _runningIndex < _indicesInRect.size(); ++_runningIndex)
        {
            pdh = (*this)[_runningIndex];
            //pdh = (*this)[_runningIndex->first];
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
        return  _items.operator[](ix);
    }

    void SetVisibility(DrawableItemIndex drix, bool visible)
    {
        SetVisibility(drix.index, visible);
    }
    void SetVisibility(int index, bool visible)
    {                     // _pQTree must not be null
        if ((*this)[index]->isVisible == visible)    // when it does change
            (*this)[index]->isVisible = visible;
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

    int CopyDrawable(DrawableItem& dri) // adds copy of existing object on the heap
    {
        if(_pZorderStore) // then the copy's zOrder must be increased'
            dri.zOrder = _pZorderStore->GetZorder(dri.dtType == DrawableType::dtScreenShot);

        switch (dri.dtType)
        {
            case DrawableType::dtDot:       return Push_back(new DrawableDot((DrawableDot&)dri)); break;
            case DrawableType::dtCross:     return Push_back(new DrawableCross((DrawableCross&)dri)); break;
            case DrawableType::dtEllipse:   return Push_back(new DrawableEllipse((DrawableEllipse&)dri)); break;
            case DrawableType::dtRectangle: return Push_back(new DrawableRectangle((DrawableRectangle&)dri)); break;
            case DrawableType::dtScreenShot:return Push_back(new DrawableScreenShot((DrawableScreenShot&)dri)); break;
            case DrawableType::dtScribble:  return Push_back(new DrawableScribble((DrawableScribble&)dri)); break;
            case DrawableType::dtText:      return Push_back(new DrawableText((DrawableText&)dri)); break;
            default:  break;
        }

        // return IndexVectorIterator();
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
                (void)rect.united(pdrw->Area());
            }
        };

        int siz = _items.size();
        if (type == DrawableType::dtNone)
        {
            for (int i = 0; i < siz; ++i)
            {
                auto pdrw = (*this)[i];
                addIfNear(pdrw, i);
            }
        }
        else
        {
            for (int i = 0; i < siz; ++i)
            {
                auto pdrw = (*this)[i];

                if (pdrw->dtType == type)
                    addIfNear(pdrw, i);
            }
        }
        return rect;
    }
    int/*DrawableItemIndex*/ IndexOfTopMostItemUnder(QPointF point, int penWidth, DrawableType type = DrawableType::dtNone) const
    {
        if (point == QPoint(-1, -1))  // select topmost item
        {
            for (int i = _items.size() - 1; i >= 0; --i)
                if (_items[i]->dtType == type)
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
                auto pdrw = (*this)[iv.at(i)];

                if (pdrw->PointIsNear(point, penWidth / 2.0 + 3))
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
                pdrw = (*this)[iv.at(i)];
                if (pdrw->dtType == type)
                    break;
            }

            if (i < _items.size())
            {
                res.index = i;
                res.zorder = pdrw->zOrder;
                res.type = type;

                for (++i; i < iv.size(); ++i)
                {
                    if (pdrw->PointIsNear(point, penWidth / 2.0 + 3))
                    {
                        if (pdrw->zOrder > res.zorder)
                        {
                            res.index = i;
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
    void RotateDrawable(int index, MyRotation rot, QRectF inThisrectangle, qreal alpha = 0.0)    // alpha used only for 'rotAlpha'
    {
        (*this)[index]->Rotate(rot, inThisrectangle, alpha);
    }
    void VertShiftItemsBelow(int belowY, int deltaY);

private:
    int _imageIndex = -1;
    QRectF _canvasRect;
};


/*========================================================
 * One history element
 *-------------------------------------------------------*/

struct History; // forward declaration it contains the 'DrawableList' common for this history

struct HistoryItem      // base class
{
    History* pHist;
    HistEvent type;
    DrawableList* pDrawables = nullptr;         // to pHist's _drawables

    HistoryItem(History* pHist, HistEvent typ = HistEvent::heNone);
    virtual ~HistoryItem() {}

    virtual int ZOrder() const { return 0; }

    virtual QPointF TopLeft() const { return QPoint(); }
    virtual DrawableItem* GetDrawable(bool onlyVisible = false, int* pIndex = nullptr) const 
    {     
        if (pIndex)
            *pIndex = -1;
        return nullptr; 
    }

    virtual bool Translatable() const { return false;  }
    virtual void Translate(QPointF p, int minY) { } // translates if top is >= minY
    virtual void Rotate(MyRotation rot, QRectF encRect, float alpha=0.0) { ; }      // rotation or flip
    virtual int Size() const { return 0; }         // size of stored scribbles or erases
    virtual void SetVisibility(bool visible) { }

    // function prototypes for easier access
    virtual int RedoCount() const { return 1; } // how many elements from _redoList must be moved back to _items
    virtual int Undo() { return 1; }        // returns amount _actItem in History need to be changed (go below 1 item)
    virtual int Redo() { return 0; }        // returns amount _actItem in History need to be changed (after ++_actItem still add this to it)
    virtual QRectF Area() const { return QRectF(); }  // encompassing rectangle for all points
    virtual bool IsVisible() const { return false; }
    virtual bool IsHidden() const { return !IsVisible(); }    // must not draw it
    virtual bool IsSaveable() const  { return IsVisible(); }    // use when items may be hidden
    virtual bool IsDrawable() const { return false; }
    virtual DrawableType Type() const { return DrawableType::dtNone; }
    virtual bool IsImage() const { return false; }
    virtual bool Intersects(QRectF& rect) const { return GetDrawable(true)->Intersects(rect);  }
    virtual void Draw(QPaintDevice* canvas, QPointF topLeftOfVisibleArea, const QRectF& clipR = QRectF()) {}                     // sets painter at every draw
    virtual void Draw(QPainter* painter, QPointF& topLeftOfVisibleArea, const QRectF& clipR = QRectF()) {} // when painter is known

    virtual bool operator<(const HistoryItem& other);
};

            //--------------------------------------------
            //      HistoryDrawableItem
            //--------------------------------------------
struct HistoryDrawableItem : public HistoryItem
{
    int indexOfDrawable;          // into History's _drawables'

    HistoryDrawableItem(History* pHist, DrawableItem& dri);   // adds a copy of 'dri'
    HistoryDrawableItem(History* pHist, DrawableItem* pdri);  // 'pDrawableItem is set to be 'pdri'
    HistoryDrawableItem(const HistoryDrawableItem& other);
    HistoryDrawableItem(HistoryDrawableItem&& other) noexcept;
    ~HistoryDrawableItem();

    void SetVisibility(bool visible) override;
    bool IsDrawable() const override { return true; }
    bool IsVisible() const override;
    virtual DrawableType Type() const override { return _Drawable()->dtType; }

    HistoryDrawableItem& operator=(const HistoryDrawableItem& other);
    HistoryDrawableItem& operator=(HistoryDrawableItem&& other) noexcept;

    int ZOrder() const override;
    QPointF TopLeft() const override { return _Drawable()->Area().topLeft(); }
    DrawableItem* GetDrawable(bool onlyVisible = false, int *pIndex=nullptr) const override;
    QRectF Area() const override;
    int Size() const override { return 1; }

    int Undo() override;        // returns amount _actItem in History need to be changed (go below 1 item)
    int Redo() override;        // returns amount _actItem in History need to be changed (after ++_actItem still add this to it)

    bool Translatable() const override { return _Drawable()->isVisible; }
    void Translate(QPointF p, int minY) override; // only if not deleted and top is > minY
    void Rotate(MyRotation rot, QRectF encRect, float alpha=0.0) override;
    // no new undo/redo needed
    bool IsImage() const override { return _Drawable()->dtType == DrawableType::dtScreenShot ? true: false; }
    void Draw(QPainter* painter, QPointF& topLeftOfVisibleArea, const QRectF& clipR = QRectF()) override
    { 
        _Drawable()->Draw(painter, topLeftOfVisibleArea); 
    }
    void Draw(QPaintDevice *canvas, QPointF topLeftOfVisibleArea, const QRectF& clipR = QRectF()) override
    { 
        QPainter painter(canvas);
        _Drawable()->Draw(&painter, topLeftOfVisibleArea,clipR); 
    }
private:
    inline DrawableItem* _Drawable() const { return (*pDrawables)[indexOfDrawable]; }
};


            //--------------------------------------------
            //      HistoryDeleteItems
            //--------------------------------------------


//--------------------------------------------
struct HistoryDeleteItems : public HistoryItem
{
    DrawableIndexVector deletedList;   // absolute indexes of scribble elements in '*pHist'
    int Undo() override;
    int Redo() override;
    HistoryDeleteItems(History* pHist, DrawableIndexVector& selected);
    HistoryDeleteItems(HistoryDeleteItems& other);
    HistoryDeleteItems& operator=(const HistoryDeleteItems& other);
    HistoryDeleteItems(HistoryDeleteItems&& other) noexcept;
    HistoryDeleteItems& operator=(const HistoryDeleteItems&& other) noexcept;
};
//--------------------------------------------
// remove an empty rectangular region 
//  before creating this item collect all items to
//  the left and right of this rectangle into two
//  lists: 'left' and 'right' then
// 
//  if 'right' is not empty moves them to the left
//      by the width of the rectangle
//  if 'right' is empty and 'left' is not do nothing
//  if both 'right' and 'left' are empty
//      move all items below the rectangle up by the
//      height of the rectangle
struct HistoryRemoveSpaceItem : public HistoryItem // using _selectedRect
{
    DrawableIndexVector modifiedList; // elements to be moved horizontally (elements on the right)
                                  // if empty move all below y up
    int delta;              // translate with this amount left OR up
    int y;                  // topmost coordinate to remove space from

    int Undo() override;
    int Redo() override;
    HistoryRemoveSpaceItem(History* pHist, DrawableIndexVector&toModify, int distance, int y);
    HistoryRemoveSpaceItem(const HistoryRemoveSpaceItem& other);
    HistoryRemoveSpaceItem& operator=(const HistoryRemoveSpaceItem& other);
    HistoryRemoveSpaceItem(HistoryRemoveSpaceItem&& other) noexcept;
    HistoryRemoveSpaceItem& operator=(HistoryRemoveSpaceItem&& other) noexcept;
};

            //--------------------------------------------
            //      HistoryPasteItemBottom
            //--------------------------------------------

//--------------------------------------------
// When pasting items into _items from bottom up 
//   'HistoryPasteItemBottom' item (its absolute index is in  
//          HistoryPasteItemTop::indexOfBottomItem
//   'HistoryScribbleItems' items athat were pasted
//   'HistoryPasteItemTop' item
struct HistoryPasteItemBottom : HistoryItem
{
    int index;          // in pHist
    int count;          // of items pasted above this item
                        // these items came in one group in *pHist's '_items'
    int moved = 0;      // 0: copy / paste, 1: moved with mouse / pen
                        //  set into both bottom and top items

    HistoryPasteItemBottom(History* pHist, int index, int count);

    HistoryPasteItemBottom(HistoryPasteItemBottom& other);
    HistoryPasteItemBottom& operator=(const HistoryPasteItemBottom& other);
    int RedoCount() const override { return count + moved + 2; }
};

            //--------------------------------------------
            //      HistoryPasteItemTop
            //--------------------------------------------

struct HistoryPasteItemTop : HistoryItem
{
    int indexOfBottomItem;      // abs. index of 'HistoryPastItemBottom' item
    int count;                  // of items pasted  
                                // these items came in one group in *pHist's '_items'
    int moved = 0;              // 0: copy / paste, 1: moved with mouse / pen
                                //  in both bottom and top items
    QRectF boundingRect;

    HistoryPasteItemTop(History* pHist, int index, int count, QRectF& rect);

    HistoryPasteItemTop(const HistoryPasteItemTop& other);
    HistoryPasteItemTop& operator=(const HistoryPasteItemTop& other);
    HistoryPasteItemTop(HistoryPasteItemTop&& other) noexcept;
    HistoryPasteItemTop& operator=(HistoryPasteItemTop&& other) noexcept;

    int Size() const override { return count; }

    DrawableItem* GetNthDrawable(int n) const; // only for top get (count - index)-th below this one
    DrawableItem* GetNthVisibleDrawable(int n) const; // only for top get (count - index)-th below this one

    bool IsVisible() const override;      // only for top: when the first element is visible, all visible
    // bool IsSaveable() const override;
    void SetVisibility(bool visible) override; // for all elements 
    bool Translatable() const override { return IsVisible(); }
    void Translate(QPointF p, int minY) override;    // only if not deleted and top is > minY
    void Rotate(MyRotation rot, QRectF encRect, float alpha=0.0) override;

    QPointF TopLeft() const override { return boundingRect.topLeft(); }
    QRectF Area() const override;

    int Undo() override;    // only for top item: hide 'count' items below this one
    int Redo() override;    // only for top item: reveal count items
};

            //--------------------------------------------
            //      HistoryRecolorItem
            //--------------------------------------------

struct HistoryReColorItem : HistoryItem
{
    DrawableIndexVector selectedList;     // indices to drawable elements in '*pHist'
    QVector<FalconPenKind> penKindList;   // colors for elements in selectedList
    FalconPenKind pk;                           
    QRectF boundingRectangle;             // to scroll here when undo/redo

    int Undo() override;
    int Redo() override;
    HistoryReColorItem(History* pHist, DrawableIndexVector&selectedList, FalconPenKind pk);
    HistoryReColorItem(HistoryReColorItem& other);
    HistoryReColorItem& operator=(const HistoryReColorItem& other);
    HistoryReColorItem(HistoryReColorItem&& other) noexcept;
    HistoryReColorItem& operator=(const HistoryReColorItem&& other) noexcept;
    QRectF Area() const override;
};

            //--------------------------------------------
            //      HistoryInsertVertSpace
            //--------------------------------------------

struct HistoryInsertVertSpace : HistoryItem
{
    int y = 0, heightInPixels = 0;

    HistoryInsertVertSpace(History* pHist, int top, int pixelChange);
    HistoryInsertVertSpace(const HistoryInsertVertSpace& other);
    HistoryInsertVertSpace& operator=(const HistoryInsertVertSpace& other);

    int Undo() override;
    int Redo() override;
    QRectF Area() const override;
};
            //--------------------------------------------
            //      HistoryRotationItem
            //--------------------------------------------

//--------------------------------------------
struct HistoryRotationItem : HistoryItem
{
    MyRotation rot;
    bool flipH = false;
    bool flipV = false;
    float rAlpha = 0.0;
    DrawableIndexVector driSelectedDrawables;
    QRectF encRect;         // encompassing rectangle: all items inside

    HistoryRotationItem(History* pHist, MyRotation rotation, QRectF rect, DrawableIndexVector selList, float alpha=0.0);
    HistoryRotationItem(const HistoryRotationItem& other);
    HistoryRotationItem& operator=(const HistoryRotationItem& other);
    int Undo() override;
    int Redo() override;
};

            //--------------------------------------------
            //      HistorySetTransparencyForAllScreenshotsItems
            //--------------------------------------------

struct HistorySetTransparencyForAllScreenshotsItems : HistoryItem
{
    DrawableIndexVector affectedIndexList;   // these images were re-created with transparent color set
    int undoBase = -1;            // top of pHist's screenShotImageList before this function was called
    QColor transparentColor;
    qreal fuzzyness = 0.0;
    HistorySetTransparencyForAllScreenshotsItems(History* pHist, QColor transparentColor, qreal fuzzyness);
    int Undo() override;
    int Redo() override;
};


// ******************************************************
class History;
        // declare here so that CopySelected can use it, but 
        // create and use it in 'DrawArea'

/*========================================================
 * describes an image with transparent background to show 
 *  on top of everything, while containing all of the items
 *  to paste into the list
 *-------------------------------------------------------*/
struct Sprite
{
    Sprite(History* ph);
    Sprite(History* ph, const QRectF &rect, const DrawableIndexVector &dri);

    History* pHist;
    QPointF topLeft;     // top,left: position of sprite rel. to top left of visible area of canvas
    QPointF dp;          // relative to top left of sprite: cursor coordinates when the sprite is selected
    QImage image;        // image of the sprite to paint over the canvas larger than rect because of borders
    QRectF rect;         // selection rectangle 0,0, width,height
    bool itemsDeleted = true;   // unless Alt was used when started moving the sprite
    bool visible = true; // copying should create new invisible sprite


    DrawableIndexVector       driSelectedDrawables;   // indices into 'pHist->_drawables' of items to be copied, that are completely inside the rubber band rectangle
                                                      // needed for hiding source items when required
    DrawableList              items;                  // copied items are duplicates of drawables in 'driSelectedDrawables'
                                                      // each point of items[..] is relative to top-left of sprite (0,0)
};

// vector of pointers to dynamically allocated history items
using HistoryItemVector = QVector<HistoryItem*>;

class HistoryList;

using HistoryItemPointer = HistoryItem*;

/*========================================================
 * Class for storing history of editing
 *  contains data for items drawn on screen, 
 *      item deletion, canvas movement, etc
 *
 *     _items:   generalized list of all items
 *          members are accessed by either added order
 *              or ordered by first y then x coordinates of
 *              their top left rectangle
 *     _redoList: contains undo-ed items
 *     _actItem: index of the the last history item added.
 *              At the same time index of the last element that
 *              must be processed when drawing
 *               new items are added after this index
 *          during undo: this index is decreased, at redo 
 *              it is increased
 *          if a new item is added after an undo it will be inserted
 *              after this position i.e. other existing items in
 *              '_items' will be invalid
 *-------------------------------------------------------*/
class History  // stores all drawing sections and keeps track of undo and redo
{
    friend class HistoryScreenShotItem;
    friend class HistorySetTransparencyForAllScreenshotsItems;

    HistoryList* _parent;           // needed for copy and paste

    HistoryItemVector _items,           // items in the order added. Items need not be drawables.
                                        // drawables use _drables._pItems
                      _redoList;        // from _items for redo. Items need not be scribbles.
                                        // drawable elements on this list may be either visible or hidden
    DrawableList _drawables;            // contains each type, including screenshots
                                        // drawable elements on this list may be either visible or hidden
    QuadTreeDelegate _quadTreeDelegate; // for fast display, set into _drawables

    ZorderStore _zorderStore;            // max. zorder 

    QPointF _topLeft;                   // temporary, top left of the visible part of this history
                                        // document relative
    QString _fileName,                  // file for history
            _loadedName;                // set only after the file is loaded, used to check reloads
    bool _inLoad = false;               // in function Load() / needed for correct z- order settings
    int _readCount = 0;                 // undo works until this index is reached
                                        // unscribble items have no indices in here
    bool _isSaved = false;              // clear after every change!
    QRectF _clpRect;                     // clipping rectangle for selecting points to draw
                                        // before searching operations set this when it is changed
    QStack<QRectF> _savedClps;           // clipRect saves

    int _indexOfFirstVisible = -1;      // in _yxorder

                // drawable selection

    DrawableIndexVector _driSelectedDrawables,    // indices into '_drawables', that are completely inside the rubber band (includes screenshots - zorder < DRAWABLE_ZORDER_BASE)
              _driSelectedDrawablesAtRight,       // -"- for elements that were at the right of the rubber band
              _driSelectedDrawablesAtLeft;        // -"- for elements that were at the left of the rubber band
    QRectF _selectionRect;              // bounding rectangle for selected items OR rectangle in which there are no items
                                        // when _driSelectedDrawables is empty

    bool _modified = false;

    HistoryItem* _AddItem(HistoryItem* p);

    bool _IsSaveable(int i);

    void _SaveClippingRect();
    void _RestoreClippingRect();
    void _ClearSelectLists() 
    { 
        _driSelectedDrawables.clear();
        _driSelectedDrawablesAtRight.clear();
        _driSelectedDrawablesAtLeft.clear();
    }

    int _LoadV1(QDataStream &ifs, qint32 version, bool force = false);    // load version 1.X files
    int _LoadV2(QDataStream &ifs, bool force = false);    // load version 2.X files

public:
    History(HistoryList* parent) noexcept;
    History(const History& o);
    History(History&& o) noexcept;
    virtual ~History();

    QSizeF UsedArea();   // of all points and images
    int CountOnPage(int px, int py, QSize pageSize, bool &getAreaSize); // -1: invalid page for px, -2: invalid page for py i.e. outside used area. First call with getAreaSize=true, others with false

    HistoryItemPointer Item(int index) const 
    { 
        return _items[index]; 
    }

    int GetZorder(bool isscreenshot, bool increment=true) { return _zorderStore.GetZorder(isscreenshot, increment); }
    DrawableList* Drawables()  { return &_drawables; }
    DrawableItem* Drawable(int i) { return _drawables[i]; }
    DrawableIndexVector SelectedDrawables() const { return _driSelectedDrawables; }

    constexpr QPointF TopLeft() const { return _topLeft; }
    void SetTopLeft(QPointF& topLeft) { _topLeft = topLeft; }

    int RightMostInBand(QRectF rect);    // only top and bottom of rect is used

    void SetClippingRect(const QRectF& rect) { _clpRect = rect;  }

    //DrawableItemIndex AddToDrawables(DrawableItem* pdr)
    int AddToDrawables(DrawableItem* pdr)
    {
        return _drawables.AddDrawable(pdr);
    }

    DrawableScreenShot* ScreenShot(int index) { return (DrawableScreenShot*)_drawables[index]; }
    int /*DrawableItemIndex*/ ScreenShotAtPos(QPointF& p) const { return _drawables.IndexOfTopMostItemUnder(p, 1, DrawableType::dtScreenShot); } // -1: no such image else index in '_drawables'
    DrawableScreenShot* FirstVisibleScreenShot(const QRectF& canvasRect) { return _drawables.FirstVisibleScreenShot(canvasRect); }
    DrawableScreenShot* NextVisibleScreenShot() { return _drawables.NextVisibleScreenShot(); }

    void Clear();
    void ClearName() { if (_items.isEmpty()) _fileName.clear(); }

    int Size() const;   // _items's size
    int CountOfVisible(); // visible 
    int CountButScreenShots();
    int SelectedSize() const { return _driSelectedDrawables.size(); }
    DrawableIndexVector& SelectedItemsList() { return _driSelectedDrawables; }


    QString Name() const 
    { 
        return _fileName; 
    }
    SaveResult Save(QString name);

    void SetName(QString name)
    {
        _fileName = name;
        Clear();
    }

    int Load(bool force=false);       // from '_fileName', returns _items.size() when Ok, -items.size()-1 when read error
    bool IsModified() const 
    { 
        return _modified & CanUndo(); 
    }
    bool CanUndo() const { return _items.size() > _readCount; } // only undo until last element read
    bool CanRedo() const { return _redoList.size(); }
    void ClearUndo() { _readCount = _items.size(); }

    HistoryItem* LastScribble() const;
    HistoryItem* operator[](int index);   // index: absolute index
    HistoryItem* operator[](int index) const { return _items[index]; }  // index: physical index in _items
    HistoryItem* operator[](DrawableItemIndex dri);   // index: absolute index
    HistoryItem* operator[](DrawableItemIndex dri) const { return _items[dri.index]; }  // index: physical index in _items

    QPointF BottomRightLimit(QSize &screenSize);      // returns bottom right coordinate of last visible item

//--------------------- Add Items ------------------------------------------
    int AddExistingDrawableItem(DrawableItem* pdrh);  // only add drawable Items here
    HistoryItem* AddClearRoll();
    HistoryItem* AddClearVisibleScreen();
    HistoryItem* AddClearDown();
    HistoryItem* AddDrawableItem(DrawableItem& dri);
    HistoryItem* AddDeleteItems(Sprite* pSprite = nullptr);                  // using 'this' and _driSelectedDrawables a
    HistoryItem* AddPastedItems(QPointF topLeft, Sprite *pSprite);    // using 'this' and either '_copiedList'  or  pSprite->... lists
    HistoryItem* AddRecolor(FalconPenKind pk);
    HistoryItem* AddInsertVertSpace(int y, int heightInPixels);       // height < 0: delete space
    HistoryItem* AddRotationItem(MyRotation rot);
    HistoryItem* AddRemoveSpaceItem(QRectF &rect);
    HistoryItem* AddScreenShotTransparencyToLoadedItems(QColor trColor, qreal fuzzyness);
    // --------------------- drawing -----------------------------------
    void Rotate(HistoryItem *forItem, MyRotation withRotation); // using _selectedRect
    void InserVertSpace(int y, int heightInPixels);

    HistoryItem* Undo();        // returns top item after undo or nullptr
    HistoryItem* Redo();

    int GetDrawablesInside(QRectF rect, IntVector& hv);

    void AddToSelection(int index=-1);
    QRectF SelectDrawablesUnder(QPointF& p, bool addToPrevious);      // selects clicked (if any) into _driSelectedDrawables, and clears right and left items list
    int /*DrawableItemIndex*/ SelectTopmostImageUnder(QPointF p);
    int CollectItemsInside(QRectF rect);
    void CopySelected(Sprite *forThisSprite = nullptr);      // copies selected scribbles into array. origin will be relative to (0,0)
                                                             // do the same with images
    void SetSelectionRect(QRectF& rect)
    {
        _selectionRect = rect;
    }

    void CollectPasted(const QRectF &rect);   // if items pasted copies them into '_driSelectedDrawables'

    const QRectF BoundingRect() const { return _selectionRect; }
    const DrawableIndexVector& Selected() const { return _driSelectedDrawables;  }
};

            //--------------------------------------------
            //      HistoryList - for each tab a different history
            //--------------------------------------------

class HistoryList : public std::vector<History*>
{
    friend class History;
    QClipboard* _pClipBoard;
    QUuid _copyGUID;
    DrawableList _copiedItems;          // from actual History's driSelectedDrawables 
    QRectF _copiedRect;                 // bounding rectangle for copied items used for paste operation
    int _actualHistoryIndex = -1;

public:
    HistoryList() { _pClipBoard = QApplication::clipboard(); }
        // called to initialize pointers to  data in 'DrawArea'
    const DrawableList& CopiedItems() const { return _copiedItems;  }
    QRectF const CopiedRect() const { return _copiedRect; }
    History*& operator[](int index)     // will throw if index < 0 && _actualHistoryIndex < 0
    {
        if (index < 0)
            return std::vector<History*>::operator[](_actualHistoryIndex);
		else
		{
			_actualHistoryIndex = index;
			return std::vector<History*>::operator[](_actualHistoryIndex);
		}
    }

    void CopyToClipboard();    // uses _pCopiedItems and _pCopiedImages so these must be setup first
    void PasteFromClipboard(); // only if the clipboard data is not the same as data in memory gets data from clipboard
};

QBitmap MyCreateMaskFromColor(QPixmap& pixmap, QColor color, qreal fuzzyness = 0.0, Qt::MaskMode mode = Qt::MaskInColor);

extern HistoryList historyList;

#endif