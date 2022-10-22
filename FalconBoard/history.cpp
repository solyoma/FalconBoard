#include <QApplication>
#include <QMessageBox>
#include <QMainWindow>
#include <algorithm>

#include "history.h"
//#include <cmath>

HistoryList historyList;       // many histories are possible

QRectF QuadAreaToArea(const QuadArea& qarea)
{
	QRectF area = QRectF(qarea.Left(), qarea.Top(), qarea.Right(), qarea.Bottom());
	return area;
}

QuadArea AreaForItem(const int &i) 
{ 
	History* ph = historyList[-1];
	assert(ph);
	DrawableItem *pdrwi = ph->Drawable(i);
	QRectF r = pdrwi->Area();  
	return QuadArea(r.x(), r.y(), r.width(), r.height()); 
}
bool IsItemsEqual(const int &i1, const int &i2)
{
	History* ph = historyList[-1];
	assert(ph);
	HistoryItemPointer phi1 = ph->Item(i1),
					   phi2 = ph->Item(i2);
	return phi1->type == phi2->type &&
		phi1->Area() == phi2->Area() &&
		phi1->IsHidden() == phi2->IsHidden() &&
		phi1->ZOrder() == phi2->ZOrder()
		;
}

QuadArea AreaForQRect(QRectF rect)
{
	return QuadArea(rect.x(), rect.y(), rect.width(), rect.height() );
}

struct _SortFunc
{			  
	History &history;
	_SortFunc(History & h) : history(h) {}
	bool operator()(int i, int j) 				// returns true if item i comes before item j
	{											// no two items may have the same z-order
		bool zOrderOk = history.Item(i)->ZOrder() < history.Item(j)->ZOrder();

		if (zOrderOk)	// zOrder OK
			return true;
		return false;
	}
};


/*=============================================================
 * TASK:    based on QImage createMaskFromColor, but using
 *          fuzzyness for matchineg near colors
 * PARAMS:  pixmap: create mask for this pixmap
 *          color:  these color defines the mask, but has
 *          fuzzyness:  0..1.0, 1.0: all colors match
 * GLOBALS:
 * RETURNS:
 * REMARKS:
 *------------------------------------------------------------*/
QBitmap MyCreateMaskFromColor(QPixmap& pixmap, QColor color, qreal fuzzyness, Qt::MaskMode mode)
{
	QImage img = pixmap.toImage();

	qreal hc, sc, lc;

	// DEBUG
	//QColor red = "red", green="green", blue="blue",mix = "#ffffff";
	//red.toHsl().getHslF(&hc, &sc, &lc);
	//green.toHsl().getHslF(&hc, &sc, &lc);
	//blue.toHsl().getHslF(&hc, &sc, &lc);
	//mix.toHsl().getHslF(&hc, &sc, &lc);
	// /DEBUG

	color.toHsl().getHslF(&hc, &sc, &lc);

	auto SameColor = [&](QColor clr) // clr is hsl
	{
		qreal h, s, l;
		clr.getHslF(&h, &s, &l);
		return qAbs(h - hc) <= fuzzyness && qAbs(s - sc) <= fuzzyness && qAbs(l - lc) <= fuzzyness;
	};

	QImage maskImage(pixmap.size(), QImage::Format_MonoLSB);
	maskImage.fill(0);
	uchar* s = maskImage.bits();

	if (img.depth() == 32) {
		for (int h = 0; h < img.height(); h++) 
		{
			const uint* sl = (const uint*)img.scanLine(h);
			for (int w = 0; w < img.width(); w++) 
			{
				if (SameColor(sl[w]))
					*(s + (w >> 3)) |= (1 << (w & 7));
			}
			s += maskImage.bytesPerLine();
		}
	}
	else {
		for (int h = 0; h < img.height(); h++) {
			for (int w = 0; w < img.width(); w++) {
				if (SameColor((uint)img.pixel(w, h)))
					*(s + (w >> 3)) |= (1 << (w & 7));
			}
			s += maskImage.bytesPerLine();
		}
	}
	if (mode == Qt::MaskOutColor)
		maskImage.invertPixels();

	// copyPhysicalMetadata(maskImage.d, d);
	return QBitmap::fromImage(maskImage);


}

/* *********************** Drawables ********************/
			// DrawableItem
bool DrawableItem::drawStarted;

DrawableItem& DrawableItem::operator=(const DrawableItem& other)
{
	*(DrawablePen*)this = (DrawablePen&)other;
	//penColor = other.penColor;
	//SetPenKind( other.penKind);
	//penWidth = other.penWidth;

	dtType = other.dtType;
	startPos = other.startPos;
	zOrder = other.zOrder;
	angle = other.angle;
	isVisible = other.isVisible;
	erasers = other.erasers;

	return *this;
}
// add eraser strokes cut up to as many parts as required
void DrawableItem::AddEraserStroke(int eraserWidth, const QPolygonF& stroke)
{
	EraserData erdata;
	erdata.eraserPenWidth = eraserWidth;
	qreal pw2 = penWidth / 2.0,
		e2 = eraserWidth / 2.0;
	pw2 += e2;

	QRectF rect = Area().adjusted(-pw2,-pw2, pw2, pw2),
		   erect;	// eraser pen width sized rectangle

	EraserData tmp;
	// cut up eraser strokes to sections inside 'rect'
		// skip those outside
	bool inside = true;
	for (int i = 0; i < stroke.size(); ++i)
	{	
		QPointF x = stroke.at(i);
		erect = QRect(x.x() - e2, x.y() - e2, eraserWidth, eraserWidth);

		if (rect.intersects(erect))
			erdata.eraserStroke << x;
		else
		{
			if (!erdata.eraserStroke.isEmpty())	// end of previous eraser section inside?
			{
				erasers.push_back(erdata);
				erdata.eraserStroke.clear();
			}
		}
	}
	if(!erdata.eraserStroke.isEmpty())
		erasers.push_back(erdata);
}

void DrawableItem::RemoveLastEraserStroke(EraserData* andStoreHere)
{
	if (andStoreHere)
		if (erasers.isEmpty())
			andStoreHere->eraserStroke.clear();
		else
			*andStoreHere = erasers[erasers.size() - 1];
	if (!erasers.isEmpty())
		erasers.pop_back();
}


void DrawableItem::Translate(QPointF dr, qreal minY)	// where translating topLeft is enough
{
	if (startPos.y() > minY)
		startPos += dr;
	for (auto e : erasers)
		for (auto& es : e.eraserStroke)
			es += dr;
}
void DrawableItem::Rotate(MyRotation rot, QRectF insideThisRectangle, qreal alpha)
{
}

// drawables MUST be saved in increasing zOrder, so no need to save the zOrder
// this way all screenshots are saved first, followed by all other drawables

inline QDataStream& operator<<(QDataStream& ofs, const DrawableItem& di)
{
	ofs << (int)di.dtType << di.startPos << (int)di.PenKind() /*<< di.PenColor()*/ << di.penWidth << di.angle;
	ofs << di.erasers.size();
	for (auto e : di.erasers)
	{
		ofs << e.eraserPenWidth << e.eraserStroke;
	}

	return ofs;
}
inline QDataStream& operator>>(QDataStream& ifs, DrawableItem& di)		// zorder was not saved, so not set, must set it after all drawables read in
{
	int n;
	ifs >> n; di.dtType = (DrawableType)n;
	ifs >> n;
	di.SetPenKind( (FalconPenKind)n);
	ifs >> di.startPos /*>> di.penColor*/ >> di.penWidth >> di.angle;
	ifs >> n;
	while (n--)
	{
		DrawableItem::EraserData ed;
		ifs >> ed.eraserPenWidth >> ed.eraserStroke;
		di.erasers.push_back(ed);
	}

	return ifs;
}



			//=====================================
			// DrawableDot
			//=====================================

void DrawableDot::Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)
{
	if (drawStarted)
	{
		SetPainterPenAndBrush(painter, clipR.translated(-topLeftOfVisibleArea));
		QPointF pt = startPos - topLeftOfVisibleArea;
		QPen pen(PenColor());
		pen.setWidth(penWidth);
		painter->setPen(pen);

		painter->drawPoint(startPos);
	}
	else
		DrawWithEraser(painter, topLeftOfVisibleArea, clipR);
}


inline QDataStream& operator<<(QDataStream& ofs, const DrawableDot& di)
{
	ofs << (DrawableItem&)di;
	return ofs;
}
inline QDataStream& operator>>(QDataStream& ifs, DrawableDot& di)			  // call AFTER header is read in
{
	return ifs;
}

			//=====================================
			// DrawableCross
			//=====================================
DrawableCross::DrawableCross(QPointF pos, qreal len, int zOrder, FalconPenKind penKind, qreal penWidth) : length(len), DrawableItem(DrawableType::dtCross, pos, zOrder, penKind, penWidth) {}
//DrawableCross::DrawableCross(const DrawableCross& o) { *this = o; }
//DrawableCross& DrawableCross::operator=(const DrawableCross& other)
//{
//	*(DrawableItem*)this = (const DrawableItem&)other;
//	length = other.length;
//	return *this;
//}

void DrawableCross::Rotate(MyRotation rot, QRectF inThisrectangle, qreal alpha)
{
}

void DrawableCross::Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)
{
	if (drawStarted)
	{
		SetPainterPenAndBrush(painter, clipR.translated(-topLeftOfVisibleArea) );
		QPointF dist(length / sqrt(2), length / sqrt(2));
		QRectF rect(startPos - dist, startPos + dist);
		rect.translate(-topLeftOfVisibleArea);
		painter->drawLine(rect.topLeft(), rect.bottomRight());
		painter->drawLine(rect.topRight(), rect.bottomLeft());
	}
	else
		DrawWithEraser(painter, topLeftOfVisibleArea, clipR);
}

inline QDataStream& operator<<(QDataStream& ofs, const DrawableCross& di)
{
	ofs << dynamic_cast<const DrawableItem&>(di) << di.length;
	return ofs;
	
}
inline QDataStream& operator>>(QDataStream& ifs, DrawableCross& di)	  // call AFTER header is read in
{
	ifs >> di.length;
	return ifs;
}

			//=====================================
			// DrawableEllipse
			//=====================================
DrawableEllipse::DrawableEllipse(QRectF rect, int zOrder, FalconPenKind penKind, qreal penWidth, bool isFilled) : rect(rect), isFilled(isFilled), DrawableItem(DrawableType::dtEllipse, rect.topLeft(), zOrder, penKind, penWidth) {}
DrawableEllipse::DrawableEllipse(const DrawableEllipse& o) { *this = o; }
DrawableEllipse::DrawableEllipse(DrawableEllipse&& o) noexcept { *this = o; }
DrawableEllipse& DrawableEllipse::operator=(const DrawableEllipse& di)
{
	*(DrawableItem*)this = (const DrawableItem&)di;

	isFilled = di.isFilled;
	rect = di.rect;
	return *this;
}

DrawableEllipse& DrawableEllipse::operator=(DrawableEllipse&& di) noexcept
{
	*(DrawableItem*)this = (DrawableItem&&)di;

	isFilled = di.isFilled;
	rect = di.rect;
	return *this;
}

void DrawableEllipse::Translate(QPointF dr, qreal minY)
{
	if (rect.top() > minY)
	{
		rect.moveTo(rect.topLeft() + dr);
		for (auto e : erasers)
			for (auto& es : e.eraserStroke)
				es += dr;
	}
}

void DrawableEllipse::Rotate(MyRotation rot, QRectF inThisrectangle, qreal alpha)
{
}

void DrawableEllipse::Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)    // example: you must override this using this same structure
{
	if (drawStarted)
	{
		QColor c;
		if (isFilled)
			c = PenColor();
		SetPainterPenAndBrush(painter, clipR.translated(-topLeftOfVisibleArea), c);
		painter->drawEllipse(rect.translated(-topLeftOfVisibleArea));
	}
	else
		DrawWithEraser(painter, topLeftOfVisibleArea, clipR);
}


inline QDataStream& operator<<(QDataStream& ofs, const DrawableEllipse& di)
{
	ofs << dynamic_cast<const DrawableItem&>(di) << di.Area() << di.isFilled;
	return ofs;
	
}
inline QDataStream& operator>>(QDataStream& ifs, DrawableEllipse& di)	  // call AFTER header is read in
{
	ifs >> di.rect >> di.isFilled;
	return ifs;
}

			//=====================================
			// DrawableRectangle
			//=====================================
DrawableRectangle::DrawableRectangle(QRectF rect, int zOrder, FalconPenKind penKind, qreal penWidth, bool isFilled) : rect(rect), isFilled(isFilled), DrawableItem(DrawableType::dtRectangle, rect.topLeft(), zOrder, penKind, penWidth) {}
DrawableRectangle::DrawableRectangle(const DrawableRectangle& di) { *this = di; }
DrawableRectangle& DrawableRectangle::operator=(const DrawableRectangle& di)
{
	*(DrawableItem*)this = (DrawableItem&&)di;

	isFilled = di.isFilled;
	rect = di.rect;
	return *this;
}

DrawableRectangle& DrawableRectangle::operator=(DrawableRectangle&& di)  noexcept
{
	*(DrawableItem*)this = (DrawableItem&&)di;

	isFilled = di.isFilled;
	rect = di.rect;
	return *this;
}

void DrawableRectangle::Translate(QPointF dr, qreal minY)             // only if not deleted and top is > minY
{
	if (rect.top() > minY)
	{
		rect.moveTo(rect.topLeft() + dr);
		for (auto e : erasers)
			for (auto& es : e.eraserStroke)
				es += dr;
	}
}

void DrawableRectangle::Rotate(MyRotation rot, QRectF inThisrectangle, qreal alpha)     // alpha used only for 'rotAlpha'
{

}


void DrawableRectangle::Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)    // example: you must override this using this same structure
{
	if (drawStarted)
	{
		QColor c;
		if (isFilled)
			c = PenColor();
		SetPainterPenAndBrush(painter, clipR.translated(-topLeftOfVisibleArea), c);
		painter->drawRect(rect.translated(-topLeftOfVisibleArea));
	}
	else
		DrawWithEraser(painter, topLeftOfVisibleArea, clipR);
}

inline QDataStream& operator<<(QDataStream& ofs, const DrawableRectangle& di)
{
	ofs << dynamic_cast<const DrawableItem&>(di) << di.Area() << di.isFilled;
	return ofs;
	
}
inline QDataStream& operator>>(QDataStream& ifs, DrawableRectangle& di)		  // call AFTER header is read in
{
	ifs >> di.rect >> di.isFilled;
	return ifs;
}

			//=====================================
			// DrawableScreenShot
			//=====================================

//-------------------------------------------------------------


void DrawableScreenShot::AddImage(QPixmap& animage)
{
	image = animage;
}

inline QRectF DrawableScreenShot::Area() const
{
	return QRectF(startPos, image.size());
}

QRectF DrawableScreenShot::AreaOnCanvas(const QRectF& canvasRect) const
{
	return Area().intersected(canvasRect);
}

void DrawableScreenShot::Rotate(MyRotation rot, QRectF encRect, qreal alpha)
{
	QTransform transform;
	QImage img;
	bool fliph = false, flipv = true;	// defaullt flip orientations
	switch (rot)
	{
		case rotR90: transform.rotate(270); image = image.transformed(transform, Qt::SmoothTransformation); break;
		case rotL90: transform.rotate(90);  image = image.transformed(transform, Qt::SmoothTransformation); break;
		case rot180: transform.rotate(180); image = image.transformed(transform, Qt::SmoothTransformation); break;
		case rotFlipH:
			fliph = true;
			flipv = false;		// NO break here!										// CHECK IF WORKING!
		case rotFlipV:
			img = image.toImage();
			img = img.mirrored(fliph, flipv);
			image = image.fromImage(img);
			break;
		case rotAlpha:
			transform.rotate(alpha); image = image.transformed(transform, Qt::SmoothTransformation);
			break;
		default: 
			break;
	}
}

void DrawableScreenShot::Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)
{
	if (drawStarted)
	{
		SetPainterPenAndBrush(painter, clipR.translated(-topLeftOfVisibleArea)); // this painter paints on screenshot layer!
		painter->drawPixmap(startPos - topLeftOfVisibleArea, Image());
	}
	else
		DrawWithEraser(painter, topLeftOfVisibleArea, clipR);
}


// ---------------------------------------------
inline QDataStream& operator<<(QDataStream& ofs, const DrawableScreenShot& bimg)
{
	ofs << (DrawableItem&)bimg << bimg.image;

	return ofs;
}

inline QDataStream& operator>>(QDataStream& ifs, DrawableScreenShot& bimg)	  // call AFTER header is read in
{	 
	ifs >> bimg.image;
	return ifs;
}

			//=====================================
			// DrawableScribble
			//=====================================

DrawableScribble::DrawableScribble(FalconPenKind penKind, qreal penWidth, int zorder) noexcept : DrawableItem(DrawableType::dtScribble, points.boundingRect().topLeft(), zOrder, penKind, penWidth) {}
DrawableScribble::DrawableScribble(const DrawableScribble& di) { *this = di; }
DrawableScribble::DrawableScribble(DrawableScribble&& di) noexcept { *this = di; }
DrawableScribble& DrawableScribble::operator=(const DrawableScribble& di)
{
	*(DrawableItem*)this = (DrawableItem&)di;
	//SetPenKind( di.penKind);
	//penWidth = di.penWidth;
	dtType = di.dtType;
	zOrder = di.zOrder;
	points = di.points;
	return *this;
}

DrawableScribble& DrawableScribble::operator=(DrawableScribble&& di)  noexcept
{
	*(DrawableItem*)this = (DrawableItem&&)di;
	//SetPenKind( di.penKind);
	//penWidth = di.penWidth;
	dtType = di.dtType;
	zOrder = di.zOrder;
	points = di.points;
	return *this;
}

void DrawableScribble::clear()
{
	points.clear();
}

bool DrawableScribble::IsExtension(const QPointF& p, const QPointF& p1, const QPointF& p2) // vectors p->p1 and p1->p point to same direction?
{
	//return false;       // DEBUG as it is not working yet

	if (p == p1)
		return true;    // nullvector may point in any direction :)

	QPointF vp = p - p1,  // not (0, 0)
		vpp = p1 - p2;

	// the two vectors point in the same direction when vpp.y()/vpp.x() == vp.y()/vp.x()
	// and both delta has the same sign. (turning back is not an extension)
	// i.e to avoid checking for zeros in divison: 
	return (vpp.y() * vp.x() == vp.y() * vpp.x()) && ( (vp.x() != vpp.x() && vp.x() * vpp.x() > 0) || (vp.y() != vpp.y() && vp.y() * vpp.y() > 0));
}

void DrawableScribble::add(QPointF p)
{
	int n = points.size() - 1;
	// we need at least one point already in the array
// if a point just extends the line in the same direction (IsExtension())
// as the previous point was from the one before it
// then do not add a new point, just modify the coordinates of the middle point
// (n 0: 1 point, >0: at least 2 points are already in the array
	if (n > 0 && IsExtension(p, points[n], points[n - 1]))  // then the two vector points in the same direction
		points[n] = p;                            // so continuation
	else
		points.push_back(p);
}

void DrawableScribble::add(int x, int y)
{
	QPointF p(x, y);
	add(p);
}

void DrawableScribble::Smooth()
{
	// smoothing points so that small variations in them vanish
	// ???
}

bool DrawableScribble::intersects(const QRectF& arect) const
{
	return Area().intersects(arect);
}

void DrawableScribble::Translate(QPointF dr, qreal  minY)
{
	if (Area().y() < minY || !isVisible)
		return;

	for (int i = 0; i < points.size(); ++i)
		points[i] = points[i] + dr;
}

void DrawableScribble::Rotate(MyRotation rot, QRectF encRect, qreal alpha)	// rotate around the center of encRect
{
	int erx = encRect.x(), ery = encRect.y(),
		erw = encRect.width(),
		erh = encRect.height();
	// item bndRectangle
	QRectF bndRect = Area();
	int rx = bndRect.x(), ry = bndRect.y(), rw = bndRect.width(), rh = bndRect.height();
	// modify encompassing rectangle and create a big and a little square
	// for rotation with sides erb and erl respectively

// if h >= w d+ = erh else d+ = erw
// if h >= w d- = erw else d- = erh
// if rotR90  -> x' = erx + ery + (d+) - y,	y' = ery - erx + x
// if rotL90  -> x' = erx - ery + y,		y' = erx + ery + (d-) - x
// rot180     -> x' = 2*erx + erw - x,		y' = 2*ery + erh - y

	int x, y, d;
	int A = erx + ery,
		B = erx - ery;

	auto RotR90 = [&](QPointF& p)
	{
		d = (erh >= erw) ? (rot == rotNone ? erh : erw) : (rot == rotNone ? erw : erh);
		x = A + d - p.y();
		y = -B + p.x();
		p.setX(x); p.setY(y);
	};

	auto RotRectR90 = [&](QRectF& r)		  // get new rectangle for transform
	{
		QPointF p = QPointF(r.x(), r.y() + r.height()); // bottom left will be top left after rotation
		RotR90(p);
		QSize size = QSize(r.height(), r.width());
		r = QRectF(p, size);		// swap height and with
	};
	auto RotL90 = [&](QPointF& p)
	{
		d = (erh >= erw) ? (rot == rotNone ? erw : erh) : (rot == rotNone ? erh : erw);
		x = B + p.y();
		y = A + erw - p.x();
		p.setX(x); p.setY(y);
	};
	auto RotRectL90 = [&](QRectF& r)
	{
		QPointF p = QPointF(r.x() + r.width(), r.y()); // top right will be top left after rotation
		RotL90(p);
		QSize size = QSize(r.height(), r.width());
		r = QRectF(p, size);		// swap height and with
	};

	QPointF tl;	// top left of transformed item rectangle

	switch (rot)
	{
		case rotR90:
			for (QPointF& p : points)
				RotR90(p);
			RotRectR90(bndRect);
			break;
		case rotL90:
			for (QPointF& p : points)
				RotL90(p);
			RotRectL90(bndRect);
			break;
		case rot180:
			for (QPointF& p : points)
			{
				x = 2 * erx + erw - p.x();
				y = 2 * ery + erh - p.y();
				p.setX(x), p.setY(y);
			}
			break;
		case rotFlipH:
			for (QPointF& p : points)
				p.setX(erw + 2 * erx - p.x());
			bndRect = QRectF(erw + 2 * erx - rx - rw, ry, rw, rh);		  // new item x: original top righ (rx + rw)
			break;
		case rotFlipV:
			for (QPointF& p : points)
				p.setY(erh - p.y() + 2 * ery);
			bndRect = QRectF(rx, erh + 2 * ery - ry - rh, rw, rh);		  // new item x: original bottom left (ry + rh)
			break;
		default:
			break;
	}

	if (rot == rotNone)		// save rotation for undo
		rot = rot;
	else
		rot = rotNone;		// undone: no rotation set yet
}
void DrawableScribble::Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)
{
	if (drawStarted)
	{
		SetPainterPenAndBrush(painter, clipR.translated(-topLeftOfVisibleArea));
		// draw normally using 'painter' and 'topLeftOfVisibleArea'
		QPolygonF pol = points.translated(-topLeftOfVisibleArea);
		painter->drawPolyline(pol);
	}
	else
		DrawWithEraser(painter, topLeftOfVisibleArea, clipR);
}


inline QDataStream& operator<<(QDataStream& ofs, const DrawableScribble& di)
{
	ofs << (DrawableItem&)di << (qint32)di.points.size();
	for (auto pt : di.points)
		ofs << pt.x() << pt.y();
	return ofs;
}
// reads ONLY after the type is read in!
inline QDataStream& operator>>(QDataStream& ifs, DrawableScribble& di)	  // call AFTER header is read in
{
	qreal x, y;
	di.points.clear();

	qint32 n;
	ifs >> n;
	while (n--)
	{
		ifs >> x >> y;
		di.add(x, y);
	}

	return ifs;
}

			//=====================================
			// DrawableText
			//=====================================
void DrawableText::Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)
{
	if (drawStarted)
	{
		SetPainterPenAndBrush(painter, clipR.translated(-topLeftOfVisibleArea)); // ???
		// draw normally using 'painter' and 'topLeftOfVisibleArea'
	}
	else
		DrawWithEraser(painter, topLeftOfVisibleArea, clipR);
}

inline QDataStream& operator<<(QDataStream& ofs, const DrawableText& di)
{
	ofs << (DrawableItem&)di << di.fontAsString << di._text;
	return ofs;
}
inline QDataStream& operator>>(QDataStream& ifs, DrawableText& di)           // call AFTER header is read in
{
	ifs >> di.fontAsString >> di._text;
	return ifs;
}

			//=====================================
			// DrawableList
			//=====================================
IntVector DrawableList::ListOfItemIndicesInRect(QRectF &r)	const
{
	return _pQTree ? _pQTree->GetValues(&_items, r) : IntVector();
}

IntVector DrawableList::ListOfItemIndicesInQuadArea(QuadArea& r) const
{
	QRectF rect = QuadAreaToArea(r);
	return ListOfItemIndicesInRect(rect);
}

QPointF DrawableList::BottomRightLimit(QSize& screenSize)
{
	QPointF pt;
	int ixBottom = 0;

	if (_pQTree && _pQTree->Count())
	{
		ixBottom = _pQTree->BottomItem();
		DrawableItem* pdri = (*this)[ixBottom];
		pt = pdri->Area().topLeft();
		pt.setY(pt.y() - screenSize.height() / 2);
		pt.setX(pt.x() - screenSize.width());
		if (pt.x() < 0)
			pt.setX(0);
		if (pt.y() < 0)
			pt.setY(0);
	}
	return pt;
}


DrawableScreenShot* DrawableList::FirstVisibleScreenShot(const QRectF& canvasRect)
{
	_imageIndex = -1;
	_canvasRect = canvasRect;
	return NextVisibleScreenShot();
}

DrawableScreenShot* DrawableList::NextVisibleScreenShot()
{
	if (_canvasRect.isNull())
		return nullptr;

	while (++_imageIndex < _items.size())
	{
		if ((*this)[_imageIndex]->dtType == DrawableType::dtScreenShot && (*this)[_imageIndex]->isVisible)
		{
			if (!((DrawableScreenShot*)(*this)[_imageIndex])->AreaOnCanvas(_canvasRect).isNull())
				return (DrawableScreenShot*)(*this)[_imageIndex];
		}
	}
	return nullptr;
}

void DrawableList::VertShiftItemsBelow(int thisY, int dy) // using the y and z-index ordered index '_yOrder'
{														  // _pQTree must not be NULL
	// DEBUG
#if !defined _VIEWER && defined _DEBUG
	_pQTree->pItemTree->DebugPrint();
#endif
	// /DEBUG
	QuadArea area = _pQTree->Area();
	QRectF r = QRectF(area.Left(), thisY, area.Width(), area.Height());
	IntVector iv = _pQTree->GetValues(&_items, r);

	for (auto ind : iv)
	{
		if ((*this)[ind]->Area().top() >= thisY)
			(*this)[ind]->Translate({ 0, (qreal)dy }, -1);
	}

	_pQTree->Resize(_pQTree->Area());
}


//*****************************************************************************************************
// Histoy 
// ****************************************************************************************************


/*========================================================
 * One history element
 *-------------------------------------------------------*/

HistoryItem::HistoryItem(History* pHist, HistEvent typ) : pHist(pHist), type(typ), pDrawables(pHist->Drawables()) {}


bool HistoryItem::operator<(const HistoryItem& other)
{
	if (!IsSaveable() || IsHidden())
		return false;
	if (!other.IsSaveable() || other.IsHidden())
		return true;

	if (TopLeft().y() < other.TopLeft().y())
		return true;
	else if (TopLeft().y() == other.TopLeft().y())
	{
		if (TopLeft().x() < other.TopLeft().x())
			return true;
	}
	return false;
}



//-------------------------------------------- 
// HistoryDrawableItem
//--------------------------------------------
// expects a complete subclass cast as DrawableItem and adds a copy of it
HistoryDrawableItem::HistoryDrawableItem(History* pHist, DrawableItem& dri) : HistoryItem(pHist, HistEvent::heDrawable)
{
	DrawableItem* _pDrawable;
	switch (dri.dtType)
	{
		case DrawableType::dtDot:		_pDrawable = new DrawableDot(dynamic_cast<DrawableDot&>(dri));				break;
		case DrawableType::dtCross:		_pDrawable = new DrawableCross(dynamic_cast<DrawableCross&>(dri));			break;
		case DrawableType::dtEllipse:	_pDrawable = new DrawableEllipse(dynamic_cast<DrawableEllipse&>(dri));		break;
		case DrawableType::dtRectangle:	_pDrawable = new DrawableRectangle(dynamic_cast<DrawableRectangle&>(dri));	break;
		case DrawableType::dtScribble:	_pDrawable = new DrawableScribble(dynamic_cast<DrawableScribble&>(dri));		break;
		case DrawableType::dtScreenShot:_pDrawable = new DrawableScreenShot(dynamic_cast<DrawableScreenShot&>(dri));	break;
		case DrawableType::dtText:		_pDrawable = new DrawableText(dynamic_cast<DrawableText&>(dri));				break;
	}
	indexOfDrawable = pHist->AddToDrawables(_pDrawable);
}

HistoryDrawableItem::HistoryDrawableItem(History* pHist, DrawableItem* pdri) : HistoryItem(pHist, HistEvent::heDrawable)
{
	indexOfDrawable = pHist->AddToDrawables(pdri);
}

HistoryDrawableItem::HistoryDrawableItem(const HistoryDrawableItem& other) : HistoryItem(other.pHist)
{
	*this = other;
}
HistoryDrawableItem::HistoryDrawableItem(HistoryDrawableItem&& other) noexcept : HistoryItem(other.pHist)
{
	*this = other;
}

HistoryDrawableItem::~HistoryDrawableItem()
{
}

void HistoryDrawableItem::SetVisibility(bool visible)
{
	pDrawables->SetVisibility(indexOfDrawable,visible);	// also adds to or removes from quad tree
}

bool HistoryDrawableItem::IsVisible() const
{
	return _Drawable()->isVisible;
}

HistoryDrawableItem& HistoryDrawableItem::operator=(const HistoryDrawableItem& other)
{
	pHist = other.pHist;
	type = other.type;
	indexOfDrawable = other.indexOfDrawable;
	return *this;
}
HistoryDrawableItem& HistoryDrawableItem::operator=(HistoryDrawableItem&& other) noexcept
{
	pHist = other.pHist;
	type = other.type;
	indexOfDrawable = other.indexOfDrawable;
	return *this;
}
int HistoryDrawableItem::ZOrder() const
{
	return _Drawable()->zOrder;
}

int HistoryDrawableItem::Undo()
{
	pHist->Drawables()->Undo();	// also removes from QuadTree
	return 1;
}

int HistoryDrawableItem::Redo()
{
	pHist->Drawables()->Redo(); // also adds to quadtree
	return 1;
}

//-------------------------------------------- 
// HistoryDeleteItem
//--------------------------------------------
DrawableItem* HistoryDrawableItem::GetDrawable(bool onlyVisible, int* pIndex) const
{
	if (pIndex)
		*pIndex = indexOfDrawable;
	return onlyVisible && IsHidden() ? nullptr : const_cast<DrawableItem*>(_Drawable());
}

QRectF HistoryDrawableItem::Area() const
{
	return _Drawable()->Area();
}

void HistoryDrawableItem::Translate(QPointF p, int minY)
{
	pDrawables->TranslateDrawable(indexOfDrawable,p, minY);
}

void HistoryDrawableItem::Rotate(MyRotation rot, QRectF encRect, float alpha)
{
	pDrawables->RotateDrawable(indexOfDrawable,rot, encRect,alpha);
}

//--------------------------------------------
int  HistoryDeleteItems::Undo()	  // reveal items
{
	for (auto drix : deletedList)
		pDrawables->SetVisibility(drix, true);
	return 1;
}
int  HistoryDeleteItems::Redo()	// hide items
{
	for (auto drix : deletedList)
		pDrawables->SetVisibility(drix, false);
	return 0;
}

HistoryDeleteItems::HistoryDeleteItems(History* pHist, DrawableIndexVector& selected) : HistoryItem(pHist), deletedList(selected)
{
	type = HistEvent::heItemsDeleted;
	Redo();         // hide them
}
HistoryDeleteItems::HistoryDeleteItems(HistoryDeleteItems& other) : HistoryDeleteItems(other.pHist, other.deletedList) {}
HistoryDeleteItems& HistoryDeleteItems::operator=(const HistoryDeleteItems& other)
{
	type = HistEvent::heItemsDeleted;
	pHist = other.pHist;
	deletedList = other.deletedList;
	//        hidden = other.hidden;
	return *this;
}
HistoryDeleteItems::HistoryDeleteItems(HistoryDeleteItems&& other)  noexcept : HistoryDeleteItems(other.pHist, other.deletedList) {}
HistoryDeleteItems& HistoryDeleteItems::operator=(const HistoryDeleteItems&& other) noexcept
{
	type = HistEvent::heItemsDeleted;
	pHist = other.pHist;
	deletedList = other.deletedList;
	//        hidden = other.hidden;
	return *this;
}

//--------------------------------------------

/*========================================================
 * TASK:
 * PARAMS:
 * GLOBALS:
 * RETURNS:
 * REMARKS: -
 *-------------------------------------------------------*/
int  HistoryRemoveSpaceItem::Redo()
{
	if (modifiedList.isEmpty())	 // vertical movement
	{
		pDrawables->VertShiftItemsBelow(y, -delta);	 // -delta < 0 move up
	}
	else	// horizontal movement
	{
		QPointF dr(-delta, 0);								// move left
		for (auto index : modifiedList)
			(*pHist)[index]->Translate(dr, -1);
	}
	return 0;
}

/*========================================================
 * TASK:
 * PARAMS:
 * GLOBALS:
 * RETURNS:
 * REMARKS: -
 *-------------------------------------------------------*/
int  HistoryRemoveSpaceItem::Undo()
{
	if (modifiedList.isEmpty())	 // vertical movement
	{
		pDrawables->VertShiftItemsBelow(y - delta, delta);	  //delta > 0 move down
	}
	else	// horizontal movement
	{
		QPointF dr(delta, 0);								 // delta >0 -> move right
		for (auto index : modifiedList)
			(*pHist)[index]->Translate(dr, -1);
	}
	return 1;
}

HistoryRemoveSpaceItem::HistoryRemoveSpaceItem(History* pHist, DrawableIndexVector& toModify, int distance, int y) :
	HistoryItem(pHist), modifiedList(toModify), delta(distance), y(y)
{
	type = HistEvent::heSpaceDeleted;
	Redo();
}

HistoryRemoveSpaceItem::HistoryRemoveSpaceItem(const HistoryRemoveSpaceItem& other) : HistoryItem(other.pHist)
{
	*this = other;
}

HistoryRemoveSpaceItem& HistoryRemoveSpaceItem::operator=(const HistoryRemoveSpaceItem& other)
{
	modifiedList = other.modifiedList;
	y = other.y;
	delta = other.delta;
	return *this;
}
HistoryRemoveSpaceItem::HistoryRemoveSpaceItem(HistoryRemoveSpaceItem&& other)  noexcept : HistoryItem(other.pHist)
{
	*this = other;
}

HistoryRemoveSpaceItem& HistoryRemoveSpaceItem::operator=(HistoryRemoveSpaceItem&& other)  noexcept
{
	modifiedList = other.modifiedList;
	y = other.y;
	delta = other.delta;
	return *this;
}

//--------------------------------------------

HistoryPasteItemBottom::HistoryPasteItemBottom(History* pHist, int index, int count) :
	HistoryItem(pHist, HistEvent::heItemsPastedBottom), index(index), count(count)
{
}

HistoryPasteItemBottom::HistoryPasteItemBottom(HistoryPasteItemBottom& other) :
	HistoryItem(other.pHist, HistEvent::heItemsPastedBottom), index(other.index), count(other.count)
{
}

HistoryPasteItemBottom& HistoryPasteItemBottom::operator=(const HistoryPasteItemBottom& other)
{
	type = HistEvent::heItemsPastedBottom;
	index = other.index;
	count = other.count;
	return *this;
}

//--------------------------------------------
HistoryPasteItemTop::HistoryPasteItemTop(History* pHist, int index, int count, QRectF& rect) :
	HistoryItem(pHist, HistEvent::heItemsPastedTop), indexOfBottomItem(index), count(count), boundingRect(rect)
{
}

HistoryPasteItemTop::HistoryPasteItemTop(const HistoryPasteItemTop& other) :
	HistoryItem(other.pHist)
{
	*this = other;
}
HistoryPasteItemTop& HistoryPasteItemTop::operator=(const HistoryPasteItemTop& other)
{
	type = HistEvent::heItemsPastedTop;
	indexOfBottomItem = other.indexOfBottomItem;
	count = other.count;
	boundingRect = other.boundingRect;
	return *this;
}
HistoryPasteItemTop::HistoryPasteItemTop(HistoryPasteItemTop&& other)  noexcept : HistoryItem(other.pHist)
{
	*this = other;
}
HistoryPasteItemTop& HistoryPasteItemTop::operator=(HistoryPasteItemTop&& other) noexcept
{
	type = HistEvent::heItemsPastedTop;
	indexOfBottomItem = other.indexOfBottomItem;
	count = other.count;
	boundingRect = other.boundingRect;
	return *this;
}

int  HistoryPasteItemTop::Undo() // elements are in _items
{
	SetVisibility(false);
	if (moved)	// then the first item above the bottom item is a history delete item
		(*pHist)[indexOfBottomItem + 1]->Undo();
	return count + moved + 2;	// decrease actual pointer below bottom item
}

int HistoryPasteItemTop::Redo()		// elements copied back to '_items' already
{
	if (moved)	// then the first item above the bottom item is a history delete item
		(*pHist)[indexOfBottomItem + 1]->Redo();
	SetVisibility(true);

	return count + moved + 1;	// we are at bottom item -> change stack pointer to top item
}

bool HistoryPasteItemTop::IsVisible() const
{
	return (*pHist)[indexOfBottomItem]->IsVisible();
}

void HistoryPasteItemTop::SetVisibility(bool visible)
{
	for (int i = 1; i <= count; ++i)
		pDrawables->SetVisibility(indexOfBottomItem + moved + i, visible);
}

void HistoryPasteItemTop::Translate(QPointF p, int minY)
{
	for (int i = 1; i <= count; ++i)
		(*pHist)[indexOfBottomItem + i]->Translate(p, minY);

	if (boundingRect.y() >= minY)
		boundingRect.translate(p);
}

void HistoryPasteItemTop::Rotate(MyRotation rot, QRectF encRect, float alpha)
{
	for (int i = 1; i <= count; ++i)
		(*pHist)[indexOfBottomItem + i]->Rotate(rot, encRect);
}

DrawableItem* HistoryPasteItemTop::GetNthDrawable(int which) const
{
	if (which < 0 || which >= count)
		return nullptr;
	return (*pHist)[indexOfBottomItem + 1 + which]->GetDrawable();
}

DrawableItem* HistoryPasteItemTop::GetNthVisibleDrawable(int which) const
{
	if (which < 0 || which >= count)
		return nullptr;
	return (*pHist)[indexOfBottomItem + 1 + which]->GetDrawable(true);
}

QRectF HistoryPasteItemTop::Area() const
{
	return boundingRect;
}


//--------------------------------------------------- 
// HistoryReColorItem
//---------------------------------------------------
HistoryReColorItem::HistoryReColorItem(History* pHist, DrawableIndexVector& selectedList, FalconPenKind pk) :
	HistoryItem(pHist), selectedList(selectedList), pk(pk)
{
	type = HistEvent::heRecolor;

	for (auto drix : selectedList)
	{
		DrawableItem* pdrwi = pHist->Drawable(drix);
		boundingRectangle = boundingRectangle.united(pdrwi->Area());
	}
	Redo();		// get original colors and set new color tp pk
}

HistoryReColorItem::HistoryReColorItem(HistoryReColorItem& other) : HistoryItem(other.pHist)
{
	*this = other;
}

HistoryReColorItem& HistoryReColorItem::operator=(const HistoryReColorItem& other)
{
	type = HistEvent::heRecolor;
	pHist = other.pHist;
	selectedList = other.selectedList;
	penKindList = other.penKindList;
	pk = other.pk;
	return *this;
}

HistoryReColorItem::HistoryReColorItem(HistoryReColorItem&& other)  noexcept : HistoryReColorItem(other.pHist, other.selectedList, other.pk)
{
	*this = other;
}

HistoryReColorItem& HistoryReColorItem::operator=(const HistoryReColorItem&& other) noexcept
{
	type = HistEvent::heRecolor;
	pHist = other.pHist;
	selectedList = other.selectedList;
	penKindList = other.penKindList;
	pk = other.pk;
	return *this;
}
int  HistoryReColorItem::Undo()
{
	int iact = 0;
	for (auto drix : selectedList)
	{
		DrawableItem* pdri = pHist->Drawable(drix);
		pdri->SetPenKind( penKindList[iact++]);
	}
	return 1;
}
int  HistoryReColorItem::Redo()
{
	for (auto drix : selectedList)	// selectedlist: indices of drawables in pHist
	{
		DrawableItem* pdri = pHist->Drawable(drix);
		penKindList.push_back(pdri->PenKind());
		pdri->SetPenKind( pk);
	}
	return 0;
}
QRectF HistoryReColorItem::Area() const { return boundingRectangle; }

//---------------------------------------------------

HistoryInsertVertSpace::HistoryInsertVertSpace(History* pHist, int top, int pixelChange) :
	HistoryItem(pHist), y(top), heightInPixels(pixelChange)
{
	type = HistEvent::heVertSpace;
	Redo();
}

HistoryInsertVertSpace::HistoryInsertVertSpace(const HistoryInsertVertSpace& other) : HistoryItem(other.pHist)
{
	*this = other;
}
HistoryInsertVertSpace& HistoryInsertVertSpace::operator=(const HistoryInsertVertSpace& other)
{
	type = HistEvent::heVertSpace;
	y = other.y; heightInPixels = other.heightInPixels; pHist = other.pHist;
	return *this;
}

int  HistoryInsertVertSpace::Undo()
{
	pHist->InserVertSpace(y + heightInPixels, -heightInPixels);
	return 1;
}

int  HistoryInsertVertSpace::Redo()
{
	pHist->InserVertSpace(y, heightInPixels);
	return 0;
}
QRectF HistoryInsertVertSpace::Area() const
{
	return QRectF(0, y, 100, 100);
}

//---------------------------------------------------------

HistoryRotationItem::HistoryRotationItem(History* pHist, MyRotation rotation, QRectF rect, DrawableIndexVector selList, float alpha) :
	HistoryItem(pHist), rot(rotation), rAlpha(alpha), driSelectedDrawables(selList), encRect(rect)
{
	encRect = encRect;
	Redo();
}

HistoryRotationItem::HistoryRotationItem(const HistoryRotationItem& other) :
	HistoryItem(other.pHist), rot(other.rot), rAlpha(other.rAlpha), driSelectedDrawables(other.driSelectedDrawables)
{
	flipV = other.flipV;
	flipH = other.flipH;
	encRect = other.encRect;
}

HistoryRotationItem& HistoryRotationItem::operator=(const HistoryRotationItem& other)
{
	pHist = other.pHist;
	driSelectedDrawables = other.driSelectedDrawables;
	flipV = other.flipV;
	flipH = other.flipH;
	rAlpha = other.rAlpha;
	encRect = other.encRect;

	return *this;
}

// helper
static void SwapWH(QRectF& r)
{
	int w = r.width();
	r.setWidth(r.height());
	r.setHeight(w);
}


int HistoryRotationItem::Undo()
{
	MyRotation rotation = rot;
	float alpha = rAlpha;
	switch (rot)
	{
		case rotR90:
			rotation = rotL90;
			break;
		case rotL90:
			rotation = rotR90;
			break;
		case rot180:
		case rotFlipH:
		case rotFlipV:
			break;
		case rotAlpha:
			alpha = -rAlpha;
			break;
		default:
			break;
	}
	for (auto dri : driSelectedDrawables)
		(*pHist)[dri]->Rotate(rotation, encRect, alpha);
	SwapWH(encRect);

	return 1;
}

int HistoryRotationItem::Redo()
{
	for (auto n : driSelectedDrawables)
		(*pHist)[n]->Rotate(rot, encRect, rAlpha);
	if (rot != rotFlipH && rot != rotFlipV)
		SwapWH(encRect);
	return 0;
}


//****************** HistorySetTransparencyForAllScreenshotsItem ****************
HistorySetTransparencyForAllScreenshotsItems::HistorySetTransparencyForAllScreenshotsItems(History* pHist, QColor transparentColor, qreal fuzzyness) : fuzzyness(fuzzyness), transparentColor(transparentColor), HistoryItem(pHist)
{
	Redo();
}

int HistorySetTransparencyForAllScreenshotsItems::Redo()
{
	DrawableList* pdrbl = &pHist->_drawables;
	int siz = undoBase = pdrbl->Size(DrawableType::dtScreenShot); // for undo this will be the first position
	DrawableItem* psi, *psin;
	DrawableScreenShot* pds;
	//DrawableItemIndex int drix;
	int imgIndex = 0;
		// get indices for visible screenshots, needed for undo
	for (int i = 0; i < undoBase; ++i)
	{
		psi = (*pdrbl)[i];
		if (psi->dtType == DrawableType::dtScreenShot && psi->isVisible)
		{
			//drix = { dtScreenShot, i, psi->zOrder };
			affectedIndexList.push_back(i/*drix*/);

			pds = new DrawableScreenShot(*(DrawableScreenShot*)psi);	// copy of the old, so set new image
			QPixmap image = pds->Image();
			QBitmap bm = MyCreateMaskFromColor(image, transparentColor, fuzzyness);
			image.setMask(bm);
			pds->AddImage(image);
			psi->isVisible = false;			// hide original

			pdrbl->AddDrawable(pds);
		}
	}
	return 1;
}

int HistorySetTransparencyForAllScreenshotsItems::Undo()
{
	DrawableList* pdrbl = &pHist->_drawables;
	// first new image is at the first position saved

	pdrbl->Erase(undoBase);

	DrawableScreenShot* psi;
	for (int i = 0; i < affectedIndexList.size(); ++i)
	{
		psi = (DrawableScreenShot*)(*pdrbl)[affectedIndexList[i]];
		psi->isVisible = true;			// show original
	}
	return 1;
}

//********************************** History class ****************************
History::History(HistoryList* parent) noexcept: _parent(parent) 
{ 
	_quadTreeDelegate.SetUp();
	_drawables.SetQuadTreeDelegate(&_quadTreeDelegate).SetZorderStore(&_zorderStore);	// allocate objects and set them 
	_drawables.SetZorderStore(&_zorderStore);
}

History::History(const History& o)
{
	_parent = o._parent;
	_items = o._items;
	_redoList = o._redoList;
	_drawables = o._drawables;
	_quadTreeDelegate = o._quadTreeDelegate;

	_zorderStore = o._zorderStore;
}

History::History(History&& o) noexcept
{
	_parent = o._parent;
	_items = o._items;
	_redoList = o._redoList;
	_drawables = o._drawables;
	_quadTreeDelegate = o._quadTreeDelegate;
	_zorderStore = o._zorderStore;

	o._items.empty();
	o._parent	= nullptr;
}

History::~History()
{
	Clear();
}

void History::_SaveClippingRect()
{
	_savedClps.push(_clpRect);
}

void History::_RestoreClippingRect()
{
	_clpRect = _savedClps.pop();
}

QSizeF History::UsedArea()
{
	QRectF rect;
	for (auto item : _items)
	{
		if (!item->IsHidden())
			rect = rect.united(item->Area());
	}
	return rect.size();
}

int History::CountOnPage(int px, int py, QSize pageSize, bool &getAreaSize)	// px, py = 0, 1, ...
{
	static QSizeF usedSize;
	if (getAreaSize)
	{
		usedSize = UsedArea();
		getAreaSize = false;
	}
	if (px * pageSize.width() > usedSize.width() || py * pageSize.height() > usedSize.height())
		return -1;
	QuadArea area(px * pageSize.width(), py * pageSize.height(), pageSize.width(), pageSize.height());
	return _drawables.Count(area);
}

/*=============================================================
 * TASK:	get drawable items that are right of rect
 * PARAMS : 'rect' - of which points of items must be at the right
 * GLOBALS : 
 * RETURNS :
 * REMARKS : 
 * ------------------------------------------------------------*/
int History::RightMostInBand(QRectF rect)
{
	IntVector iv;

	QuadArea area( AreaForQRect(rect));	
	// only get areas till right of Quad are
	area = QuadArea(0, area.Top(), _drawables.Area().Right(), area.Height());
	iv = _drawables.ListOfItemIndicesInQuadArea(area);

	if (iv.empty())
		return 0;
	int x = -1;
	for (auto ix : iv)
	{
		HistoryItem* phi = _items[ix];
		if (x < phi->Area().right())
			x = phi->Area().right();
	}
	return x;
}


QPointF History::BottomRightLimit(QSize &screenSize)
{
	return _drawables.BottomRightLimit(screenSize);
}


/*========================================================
 * TASK:	add a new item to the history list
 * PARAMS:	p - item to add. Corresponding drawables already
 *				added to '_drawables'
 * GLOBALS:
 * RETURNS:
 * REMARKS: - all items
 *-------------------------------------------------------*/
HistoryItem* History::_AddItem(HistoryItem* p)
{
	if (!p)
		return nullptr;

	// _driSelectedDrawables.clear();	// no valid selection after new item
	// _selectionRect = QRectF();

	_items.push_back(p);
		  // no undo after new item added
	for(auto a: _redoList)
		if (a->type == HistEvent::heDrawable)
		{
			_drawables.ClearRedo();			// any  Undo()-t drawbles must be removed
			break;
		}
	_redoList.clear();	
	//if (p->Type() == dtScreenShot)
	//{
	//	HistoryDrawableItem* phi = reinterpret_cast<HistoryDrawableItem*>(p);
	//	_screenShotImageList[phi->which].itemIndex = _items.size() - 1;		// always the last element
	//}

	_modified = true;

	int n = _items.size() - 1;
	p = _items[n];

	return p;		// always the last element
}

bool History::_IsSaveable(int i)
{
	return _items[i]->IsSaveable();
}

void History::Clear()		// does not clear lists of copied items and screen snippets
{
	for (auto i : _items)
		delete i;
	for (auto i : _redoList)
		delete i;

	_items.clear();
	_redoList.clear();

	_drawables.Clear();			// images and others
	_readCount = 0;
	_loadedName.clear();

	_modified = false;
}

int History::Size() const
{
	return _items.size();
}

int History::CountOfVisible()
{
	return _drawables.Count();
}

int History::CountButScreenShots()
{
	return _drawables.Count() - _drawables.CountOfScreenShots();
}

HistoryItem* History::LastScribble() const
{
	if (_items.size())
	{
		int ix = _items.size() - 1;
		while (ix >= 0 && !_items[ix]->IsDrawable())
				--ix;
		return ix >= 0 ? _items[ix] : nullptr;
	}
	else
		return nullptr;
}

HistoryItem* History::operator[](int index)	// absolute index
{
	if (index < 0 || index >= _items.size())
		return nullptr;
	return _items[index];
}

HistoryItem* History::operator[](DrawableItemIndex dri)	// absolute index
{
	return operator[](dri.index);
}

/*========================================================
 * TASK:    save actual visible drawable items
 * PARAMS:  name - full path name
 * GLOBALS:
 * RETURNS:
 * REMARKS:
 *-------------------------------------------------------*/
SaveResult History::Save(QString name)
{
	// only save visible items

	if (name != _loadedName)
		_loadedName = _fileName = name;

	if (_drawables.Count() == 0 && !_modified)					// no elements or no visible elements
	{
		QMessageBox::information(nullptr, sWindowTitle, QObject::tr("Nothing to save"));
		return srSaveSuccess;
	}

	QFile f(name + ".tmp");
	f.open(QIODevice::WriteOnly);

	if (!f.isOpen())
		return srFailed;   // can't write file

	QDataStream ofs(&f);
	ofs << MAGIC_ID;
	ofs << MAGIC_VERSION;	// file version


	QRectF area = QuadAreaToArea(_drawables.Area());
	DrawableItem* phi = _drawables.FirstVisibleDrawable(area); // first in zOrder
	while(phi)
	{
		ofs << *phi;

		if (ofs.status() != QDataStream::Ok)
		{
			f.remove();
			return srFailed;
		}
		phi = _drawables.NextVisibleDrawable();	// in increasing zOrder
	}
	_modified = false;
	if (QFile::exists(name + "~"))
		QFile::remove(QString(name + "~"));
	QFile::rename(name, QString(name + "~"));

	f.rename(name);

	return srSaveSuccess;
}


/*========================================================
 * TASK:	Loads saved file whose name is set into _fileName
 * PARAMS:	'force' load it even when already loaded
 *				in which case it overwrites data in memory
 * GLOBALS: _fileName, _inLoad,_readCount,_items,
 *			_screenShotImageList,_modified, 
 * RETURNS:   -1: file does not exist
 *			< -1: file read error number of records read so
 *				  far  is -(ret. value+2)
 *			   0: invalid file
 *			>  0: count of items read
 * REMARKS: - beware of old version files
 *			- when 'force' clears data first
 *			- both _fileName and _loadedName can be empty
 *-------------------------------------------------------*/
int History::Load(bool force)
{
	if (!force && _fileName == _loadedName)	// already loaded?
		return _readCount;

	QFile f(_fileName);
	f.open(QIODevice::ReadOnly);
	if (!f.isOpen())
		return -1;			// File not found

	QDataStream ifs(&f);
	qint32 id, version;
	ifs >> id;
	if (id != MAGIC_ID)
		return 0;			// read error
	ifs >> version;		// like 0x56020101 for V 2.1.1
	if ((version >> 24) != 'V')	// invalid/damaged  file or old version format
		return 0;

	Clear();
	
	int res;
	if (version & 0x00FF0000 <= 0x020000)
		res = _LoadV1(ifs, force);
	else
		res = _LoadV2(ifs, force);

	f.close();
	return res;
}

int History::_LoadV2(QDataStream&ifs, bool force)
{
	DrawableItem dh, *pdrwh;
	DrawableDot dDot;
	DrawableCross dCross;
	DrawableEllipse dEll;
	DrawableRectangle dRct;
	DrawableScribble dScrb;
	DrawableText dTxt;
	DrawableScreenShot dsImg;

	int n = 0;

// z order counters are reset
	HistoryItem* phi;

	while (!ifs.atEnd())
	{
		++n;
		ifs >> dh;
		switch (dh.dtType)
		{
			case DrawableType::dtDot:			ifs >> dDot ;  pdrwh = & dDot ; break;
			case DrawableType::dtCross:			ifs >> dCross; pdrwh = & dCross; break;
			case DrawableType::dtEllipse:		ifs >> dEll ;  pdrwh = & dEll ; break;
			case DrawableType::dtRectangle:		ifs >> dRct ;  pdrwh = & dRct ; break;
			case DrawableType::dtScreenShot:	ifs >> dsImg;  pdrwh = & dsImg; break;
			case DrawableType::dtScribble:		ifs >> dScrb;  pdrwh = & dScrb; break;
			case DrawableType::dtText:			ifs >> dTxt ;  pdrwh = & dTxt ; break;
			default:break;
		}
		*pdrwh = dh;
		(void)AddDrawableItem(*pdrwh); // this will add the drawable to the list and sets its zOrder too
	}
	return _readCount = n;
}


int History::_LoadV1(QDataStream &ifs, qint32 version, bool force)
{
	_inLoad = true;

	DrawableItem dh, * pdrwh;
	DrawableDot dDot;
	DrawableRectangle dRct;
	DrawableScribble dScrb;
	DrawableScreenShot dsImg;

// z order counters are reset
	int i = 0, n;
	while (!ifs.atEnd())
	{
		ifs >> n;	// HistEvent				
		if ((HistEventV1(n) == HistEventV1::heScreenShot))
		{
			int x, y, zo;
			ifs >> dsImg.zOrder >> x >> y;
			dsImg.startPos = QPoint(x, y);

			QPixmap pxm;
			ifs >> pxm;
			dsImg. AddImage(pxm);
			pdrwh = &dsImg;
		}
		else
		{
			// only screenshots and scribble is possible in a ver 1.0 file
			pdrwh = &dScrb;

			dScrb.dtType = DrawableType::dtScribble;

			qint32 n;
			ifs >> n; dScrb.zOrder = n;
			ifs >> n; dScrb.SetPenKind( (FalconPenKind)(n & ~128));
			// dScrb.SetPenColor();	// from penKind
			bool filled = n & 128;
			ifs >> n; dScrb.penWidth = n;

			qint32 x, y;
			dScrb.points.clear();

			ifs >> n;
			while (n--)
			{
				ifs >> x >> y;
				dScrb.add(x, y);
			}

			if (dScrb.points.size() == 2 && dScrb.points[0] == dScrb.points[1])	// then its a Dot
			{
				dDot.penWidth = dScrb.penWidth;
				dDot.SetPenKind(dScrb.PenKind());
				//dDot.penColor = dScrb.penColor;
				dDot.startPos = dScrb.points[0];
				pdrwh = &dDot;
			}
			else
			{
				auto isRectangle = [&](QPolygonF& poly)
				{
					return dScrb.points.size() == 5 &&
						dScrb.points[4] == dScrb.points[0] &&
						dScrb.points[1].y() == dScrb.points[0].y() &&
						dScrb.points[2].x() == dScrb.points[1].x() &&
						dScrb.points[3].y() == dScrb.points[2].y();
				};
				if (isRectangle(dScrb.points))
				{
					pdrwh = &dRct;
					*pdrwh = (DrawableItem&)dScrb;
					dRct.rect = QRectF(dScrb.points[0], QSizeF(dScrb.points[1].x() - dScrb.points[0].x(), dScrb.points[2].y() - dScrb.points[1].y()));
					//dRct.SetPenColor();
				}
			}
			// patch for older versions:
			if (version < 0x56010108)
			{
				if (dScrb.PenKind()== penEraser)
					dScrb.SetPenKind(penYellow);
				else if (dScrb.PenKind()== penYellow)
					dScrb.SetPenKind( penEraser);
			}
			// end patch
			if (ifs.status() != QDataStream::Ok)
			{
				_inLoad = false;
				return -(_items.size() + 2);
			}
		}
		(void)AddDrawableItem(*pdrwh);	// this will add the drawable to the list and sets its zOrder too

		++i;
	}
	_modified = false;
	_inLoad = false;

	_loadedName = _fileName;

	return  _readCount = i ;
}

//--------------------- Add Items ------------------------------------------

HistoryItem* History::AddClearRoll()
{
	_SaveClippingRect();
	_clpRect = QRectF(0, 0, 0x70000000, 0x70000000);
	HistoryItem* pi = AddClearVisibleScreen();
	_RestoreClippingRect();
	return pi;
}

HistoryItem* History::AddClearVisibleScreen() // ???
{
	DrawableIndexVector toBeDeleted;

	HistoryDeleteItems* p = new HistoryDeleteItems(this, toBeDeleted);
	return _AddItem(p);
}

HistoryItem* History::AddClearDown()
{
	_SaveClippingRect();
	_clpRect.setHeight(0x70000000);
	HistoryItem* pi = AddClearVisibleScreen();
	_RestoreClippingRect();
	return pi;
}

HistoryItem* History::AddDrawableItem(DrawableItem& itm)	
{				                                            
	if (itm.dtType == DrawableType::dtScribble)	  // then delete previous item if it was a DrawableDot
	{										      // that started this line
		int n = _items.size()-1;
		HistoryItem* pitem = n < 0 ? nullptr : _items[n];
		if (pitem && pitem->type == HistEvent::heDrawable)
		{
			HistoryDrawableItem* phdi = (HistoryDrawableItem*)pitem;
			DrawableItem* pdrwi = _drawables[phdi->indexOfDrawable];
			if (pdrwi->dtType == DrawableType::dtDot)	// remove dot that was a start of a line
			{
				if (pdrwi->startPos == itm.startPos)
				{
					delete pitem;
					_items.pop_back();
				}
			}
		}
	}
	HistoryDrawableItem* p = new HistoryDrawableItem(this, itm);
	return _AddItem(p);		 // may be after an undo, so
}							 // delete all item after the last visible one (items[lastItem].scribbleIndex)


/*========================================================
 * TASK:	hides visible items on list
 * PARAMS:	pSprite: NULL   - use _driSelectedDrawables
 *					 exists - use its selection list
 * RETURNS: pointer to delete item on _items
 * REMARKS:
 *-------------------------------------------------------*/
HistoryItem* History::AddDeleteItems(Sprite* pSprite)
{
	DrawableIndexVector* pList = pSprite ? &pSprite->driSelectedDrawables : &_driSelectedDrawables;

	if (!pList->size())
		return nullptr;          // do not add an empty list
	HistoryDeleteItems* p = new HistoryDeleteItems(this, *pList);
	return _AddItem(p);
}


/*========================================================
 * TASK:		add items copied or combined into a sprite
 * PARAMS:		topLeft - paste here
 *				pSprite - pointer to sprite	or nullptr
 * GLOBALS:
 * RETURNS:
 * REMARKS: - pasted items are sandwiched between a bootom
 *				and a top marker.
 *			- When the original items were deleted after
 *				added to a sprite a HistoryDeleteItem is at
 *				the top of the item stack. As we want the
 *				Undo for paste a single operation, we must
 *				undo this deleteion too, therefore we NEED
 *				the top and bottom markers even for a single
 *				pasted item
 *-------------------------------------------------------*/
HistoryItem* History::AddPastedItems(QPointF topLeft, Sprite* pSprite)			   // tricky
{
	DrawableList* pCopiedItems = pSprite ? &pSprite->items : &_parent->_copiedItems;
	QRectF* pCopiedRect = pSprite ? &pSprite->rect : &_parent->_copiedRect;

	if (!pCopiedItems->Size())	// any drawable
		return nullptr;          // do not add an empty list
  // ------------add bottom item

	HistoryDeleteItems* phd = nullptr;
	if (pSprite && pSprite->itemsDeleted)	// then on top of _items there is a HistoryDeleteItems
	{
		phd = (HistoryDeleteItems*)_items[_items.size() - 1];	// pointer to HistoryDeleteItems
		_items.pop_back();
	}
	// -------------- add the bottom item --------
	int indexOfBottomItem = _items.size();
	HistoryPasteItemBottom* pb = new HistoryPasteItemBottom(this, indexOfBottomItem, pCopiedItems->Size());
	_AddItem(pb);
	if (pSprite)
	{
		pb->moved = pSprite->itemsDeleted ? 1 : 0;
		_AddItem(phd);
	}

	//----------- add drawables
	for (auto si : *pCopiedItems->Items())
	{
		si->Translate(topLeft, -1);
		HistoryDrawableItem* p = new HistoryDrawableItem(this, si);	   // si's data remains where it was
		_AddItem(p);
	}
	// ------------Add Paste top marker item
	QRectF rect = pCopiedRect->translated(topLeft);
	HistoryPasteItemTop* pt = new HistoryPasteItemTop(this, indexOfBottomItem, pCopiedItems->Size(), rect);
	if (pSprite)
		pt->moved = pSprite->itemsDeleted ? 1 : 0;		// mark it moved

	return _AddItem(pt);
}

HistoryItem* History::AddRecolor(FalconPenKind pk)
{
	if (!_driSelectedDrawables.size())
		return nullptr;          // do not add an empty list

	HistoryReColorItem* p = new HistoryReColorItem(this, _driSelectedDrawables, pk);
	return _AddItem(p);
}

HistoryItem* History::AddInsertVertSpace(int y, int heightInPixels)
{
	HistoryInsertVertSpace* phi = new HistoryInsertVertSpace(this, y, heightInPixels);
	return _AddItem(phi);
}

HistoryItem* History::AddRotationItem(MyRotation rot)
{
	if (!_driSelectedDrawables.size())
		return nullptr;          // do not add an empty list
	HistoryRotationItem* phss = new HistoryRotationItem(this, rot, _selectionRect, _driSelectedDrawables);
	return _AddItem(phss);
}

HistoryItem* History::AddRemoveSpaceItem(QRectF& rect)
{
	int  nCntRight = _driSelectedDrawablesAtRight.size(),	// both scribbles and screenshots
		nCntLeft = _driSelectedDrawablesAtLeft.size();

	if ((!nCntRight && nCntLeft))
		return nullptr;

	HistoryRemoveSpaceItem* phrs = new HistoryRemoveSpaceItem(this, _driSelectedDrawablesAtRight, nCntRight ? rect.width() : rect.height(), rect.bottom());
	return _AddItem(phrs);
}

HistoryItem* History::AddScreenShotTransparencyToLoadedItems(QColor trColor, qreal fuzzyness)
{
	HistorySetTransparencyForAllScreenshotsItems* psta = new HistorySetTransparencyForAllScreenshotsItems(this, trColor, fuzzyness);
	return _AddItem(psta);
}


//********************************************************************* History ***********************************

void History::Rotate(HistoryItem* forItem, MyRotation withRotation)
{
	if (forItem)
		forItem->Rotate(withRotation, _selectionRect);
}

void History::InserVertSpace(int y, int heightInPixels)
{
	_drawables.VertShiftItemsBelow(y, heightInPixels);
	_modified = true;
}

HistoryItem* History::Undo()      // returns top left after undo
{
	int actItem = _items.size();
	if (!actItem || actItem == _readCount)		// no more undo
		return nullptr;

	// ------------- first Undo top item
	HistoryItem* phi = _items[--actItem];
	int count = phi->Undo();		// it will affect this many elements (copy / paste )
    // QRectF rect = phi->Area();		// area of undo

	// -------------then move item(s) from _items to _redoList
	//				and remove them from the quad tree
	// starting at the top of _items
	while (count--)
	{
		phi = _items[actItem];	// here again, index is never negative 
		_redoList.push_back(phi);

		_items.pop_back();	// we need _items for removing the
		--actItem;
	}
	_modified = true;

	return actItem >= 0 ? _items[actItem] : nullptr;
}

int History::GetDrawablesInside(QRectF rect, IntVector& hv)
{
	hv = _drawables.ListOfItemIndicesInRect(rect);
	return hv.size();
}


/*========================================================
 * TASK:	Redo changes to history
 * PARAMS:	none
 * GLOBALS:	_items, _redoList
 * RETURNS:	pointer to top element after redo
 * REMARKS: first element moved back from _redoList list to
 *			_items, then the element Redo() function is called
 *-------------------------------------------------------*/
HistoryItem* History::Redo()   // returns item to redone
{
	int actItem = _redoList.size();
	if (!actItem)
		return nullptr;

	HistoryItem* phi = _redoList[--actItem];

	int count = phi->RedoCount();

	while (count--)
	{
		phi = _redoList[actItem--];	// here again

		_items.push_back(phi);
		_redoList.pop_back();
		phi->Redo();
	}

	_modified = true;

	return phi;
}


/*========================================================
 * TASK:	add index-th drawable used in _items to selected list
 * PARAMS:	index: in _items or -1 If -1 uses last drawn item added
 * GLOBALS:
 * RETURNS:
 * REMARKS: -
 *-------------------------------------------------------*/
void History::AddToSelection(int index)
{
	if (index < 0)
		index = _items.size() - 1;
	const HistoryDrawableItem* pitem = (HistoryDrawableItem*)_items.at(index);
	DrawableItem* pdri = _drawables[pitem->indexOfDrawable];
	int /*DrawableItemIndex*/ drix;
	drix = pitem->indexOfDrawable;
	_driSelectedDrawables.push_back(drix);
	_selectionRect = _selectionRect.united(pitem->Area());

}

/*========================================================
 * TASK:   collects indices for drawable items in _drawables'
 *			that are completely inside a rectangular area
 *				into '_driSelectedDrawables',
 *			that are at the right&left of 'rect' into
 *				'_driSelectedDrawablesAtRight' and '_driSelectedDrawablesAtLeft'
 *
 * PARAMS:	rect: document (0,0) relative rectangle
 * GLOBALS: _drawables,
 * RETURNS:	size of selected item list + lists filled
 * REMARKS: - even pasted items require a single index
 *			- only selects items whose visible elements
 *			  are completely inside 'rect' (pasted items)
 *			- '_selectionRect' is set even, when no items
 *				are in '_driSelectedDrawables'. In that case
 *				it is equal to 'rect'
 *-------------------------------------------------------*/
int History::CollectItemsInside(QRectF rect) // only 
{
	_ClearSelectLists();
	_selectionRect = QRectF();     // minimum size of selection document (0,0) relative!

	// first select all items inside a band whose top and bottom are set from 'rect'
	// but it occupies the whole width of the paper
	QuadArea area = QuadArea(0, rect.top(), _drawables.Area().Width(), rect.height());
	IntVector iv = _drawables.ListOfItemIndicesInQuadArea(area);
	for (int i = iv.size()-1; i >= 0; --i)
	{
		int ix = iv[i];
		if (!rect.contains(_items[ix]->Area()))
			iv.erase(iv.begin() + i);
	}
	if (iv.size())
	{

		_SortFunc sortFunc(*this);
		std::sort(iv.begin(), iv.end(), sortFunc);

		// then separate these into lists
		for (auto ix : iv)
		{
			HistoryItem* phi = _items[ix];
			HistoryDrawableItem* phdi = (HistoryDrawableItem*)_items[ix];
			DrawableItem* pdri = _drawables[phdi->indexOfDrawable];
			int/*DrawableItemIndex*/ drix = phdi->indexOfDrawable;
			if (rect.contains(phi->Area()))
			{
				_driSelectedDrawables.push_back(drix);
				_selectionRect = _selectionRect.united(phi->Area());
			}
			else if (phi->Area().left() < rect.left())
				_driSelectedDrawablesAtLeft.push_back(drix);
			else
				_driSelectedDrawablesAtRight.push_back(drix);
		}
		if (_driSelectedDrawables.isEmpty())		// save for removing empty space
			_selectionRect = rect;
	}
	return _driSelectedDrawables.size();
}

int History::SelectTopmostImageUnder(QPointF p)
{
	_driSelectedDrawables.clear();
	_driSelectedDrawablesAtRight.clear();
	_driSelectedDrawablesAtLeft.clear();

	int /*DrawableItemIndex*/ dri;
	dri = _drawables.IndexOfTopMostItemUnder(p, 1, DrawableType::dtScreenShot);
	if (dri < 0 )
		return dri;

	_driSelectedDrawables.push_back(dri);
	_selectionRect = _drawables[dri]->Area();
	return dri;
}

/*=============================================================
 * TASK:	select drawables under the cursor
 * PARAMS:	p - document relative coordinates of point
 *			addToPrevious: do not clear previous selection
 *					in _selectionRect, merge with these together
 * GLOBALS: 	_driSelectedDrawables		 - input and output
 *				_driSelectedDrawablesAtRight - may be cleared
 *				_driSelectedDrawablesAtLeft	 - may be cleared
 *				_selectionRect				 - input and output
 *
 * RETURNS:	area containing selected items. if invalid, then no items
 * REMARKS: - if no drawable items found then the selection lists
 *			  are not cleared
 *			- even when the area returned has other drawables
 *			  they are not included into the selection!
 *			- a drawable is found if for any of its points
 *			  its pen width sized area intersects the
 *			  area around 'p' 
 *------------------------------------------------------------*/
QRectF History::SelectDrawablesUnder(QPointF& p, bool addToPrevious)
{
	DrawableIndexVector iv;
	QRectF result;
	result = _drawables.ItemsUnder(p, iv);

	if (iv.size())
	{
		if (!addToPrevious)
		{
			_ClearSelectLists();
			_selectionRect = result;
			_driSelectedDrawables = iv;
		}
		else
		{
			_selectionRect = result.united(_selectionRect);
			// merge lists
			for (auto ii : iv)
			{
				if (!_driSelectedDrawables.contains(ii))
					_driSelectedDrawables.push_back(ii);
			}
		}
		return _selectionRect;
	}

	return result;
}


/*========================================================
 * TASK:   copies selected drawables to a sprite or
 *			into '_copiedItems' and to the clipboard
 *
 * PARAMS:	sprite: possible null pointer to sprite
 *				that contains the copied drawables
 *				when null _copiedItems is used
 * GLOBALS: _parent (HistoryList)
 *			_driSelectedDrawables -only used if
 *					the sprite does not yet has its list
 *					filled in
 *			_selectionRect
 * RETURNS:	nothing
 * REMARKS: - if sprite is null copies items into
 *				'_copiedItems' else into the sprite
 *			- origin of drawables in '_copiedItems'
 *				will be (0,0)
 *			- '_selectionRect''s top left will also be (0,0)
 *-------------------------------------------------------*/
void History::CopySelected(Sprite* sprite)
{
	DrawableList& copiedItems = sprite ? sprite->items  : _parent->_copiedItems;
	DrawableIndexVector* pSelectedDri;
	if (sprite)
	{
		if (sprite->driSelectedDrawables.size() == 0)
			sprite->driSelectedDrawables = _driSelectedDrawables;
		pSelectedDri = &sprite->driSelectedDrawables;
	}
	else
		pSelectedDri = &_driSelectedDrawables;

	if (!pSelectedDri->isEmpty())
	{
		copiedItems.Clear();

		for (auto ix : _driSelectedDrawables)  // absolute drawable item indices of visible items selected
		{
			DrawableItem* pdri = _drawables[ix];

			copiedItems.CopyDrawable(*pdri);
		}
		_parent->_copiedRect = _selectionRect.translated(-_selectionRect.topLeft());

		DrawableItemList *pdrl = copiedItems.Items();

		std::sort(pdrl->begin(), pdrl->end(), [](DrawableItem* pl, DrawableItem* pr) {return pl->zOrder, pr->zOrder; });
																			
		if (sprite)
			sprite->rect = _parent->_copiedRect;	// (0,0, width, height)

		_parent->CopyToClipboard();
	}
}


/*========================================================
 * TASK:	if there were items pasted then copies their 
 *			indices to lists
 * PARAMS:	rect - set selectionRect to this
 * GLOBALS:	_driSelectedDrawables - output
 *			_driSelectedDrawablesRight/Left - cleared
 * RETURNS:
 * REMARKS: - after a paste the topmost item on stack
 *				'_items' is historyPasteItemsTop
 *-------------------------------------------------------*/
void History::CollectPasted(const QRectF& rect)
{
	int n = _items.size() - 1;
	HistoryItem* phi = _items[n];
	if (phi->type != HistEvent::heItemsPastedTop)
		return;

	_driSelectedDrawablesAtRight.clear();
	_driSelectedDrawablesAtLeft.clear();
	//      n=4
	//B 1 2 3 T
	//      m = 3       n - m,
	int m = ((HistoryPasteItemTop*)phi)->count;
	_driSelectedDrawables.resize(m);		// because it is empty when pasted w. rubberBand
	for (int j = 0; j < m; ++j)
		_driSelectedDrawables[j] = ((HistoryDrawableItem*)_items[n - m + j])->indexOfDrawable;
	_selectionRect = rect;

}

// ********************************** Sprite *************
Sprite::Sprite(History* ph, const QRectF& rectf, const DrawableIndexVector &dri) : pHist(ph), driSelectedDrawables(dri)
{
	ph->CopySelected(this);
	rect = rectf;	// set in CopySelected() so must overwrite it here
}

Sprite::Sprite(History* ph) : pHist(ph)
{
	ph->CopySelected(this);
}

// ********************************** HistoryList *************

constexpr const char* fbmimetype = "falconBoard-data/mwb";

/*=============================================================
 * TASK:	copies selected images and scribbles to clipboard
 * PARAMS:
 * GLOBALS: _copiedItem, _copiedImages, _copyGUID
 * RETURNS:
 * REMARKS:
 *------------------------------------------------------------*/
void HistoryList::CopyToClipboard()
{
	if (!_copiedItems.Size())
		return;

	// copy to system clipboard
	QByteArray clipData;
	QDataStream data(&clipData, QIODevice::WriteOnly);		  // uses internal QBuffer

	data << _copiedRect;
	data << _copiedItems.Items()->size();
	for (auto sr : *_copiedItems.Items())
		data << sr;

	QMimeData* mimeData = new QMimeData;
	_copyGUID = QUuid().createUuid();

	mimeData->setData(fbmimetype, clipData);

	QString s = "fBClipBoardDataV2.0"+ _copyGUID.toString();
	mimeData->setText(s);
	_pClipBoard->setMimeData(mimeData);
}

void HistoryList::PasteFromClipboard()
{
	const QMimeData* pMime = _pClipBoard->mimeData();
	
	QStringList formats = pMime->formats(); // get correct mime type 
	int formatIndex=-1;	// this is the index in'formats' 
						// for the real name for our format stored as it is not standard
	for (int i = 0; i < formats.size(); ++i)
		if (formats[i].indexOf(fbmimetype) >= 0)
		{
			formatIndex = i;
			break;
		}
	if (formatIndex < 0)	// not our data
		return;

	QString s = _pClipBoard->text();
	if (s.isEmpty() || s.left(19) != "fBClipBoardDataV2.0")
		return;

	if (_copyGUID != s.mid(19))	// new clipboard data
	{
		_copyGUID = s.mid(19);

		_copiedItems.Clear();
		QByteArray pclipData = pMime->data(formats[formatIndex]);
		QDataStream data(pclipData);		  // uses internal QBuffer

		DrawableItem dri;

		QString qs;
		int cnt;

		data >> _copiedRect;
		data >> cnt;
		while (cnt--)
		{
			data >> dri.dtType;	
			data >> dri;			// this must be called aftetr type was read
			_copiedItems.AddDrawable(&dri);
		}
	}
}
