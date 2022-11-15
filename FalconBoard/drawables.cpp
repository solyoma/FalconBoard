#include <QMessageBox>
#include "drawables.h"
#include "history.h"
#include "smoother.h"

#if !defined _VIEWER && defined _DEBUG
	bool isDebugMode = false;
#endif



QRectF QuadAreaToArea(const QuadArea& qarea)
{
	QRectF area = QRectF(qarea.Left(), qarea.Top(), qarea.Right(), qarea.Bottom());
	return area;
}
// QuadArea containes indices into active _history's _drawables
QuadArea AreaForItem(const int& i)
{
	History* ph = historyList[-1];				// pointer to actual history
	assert(ph);
	DrawableItem* pdrwi = ph->Drawable(i);
	QRectF r = pdrwi->Area();
	return QuadArea(r.x(), r.y(), r.width(), r.height());
}
bool IsItemsEqual(const int& i1, const int& i2)
{
	History* ph = historyList[-1];				// pointer to actual history
	assert(ph);
	DrawableItem*pdrwi1 = ph->Drawable(i1),
				*pdrwi2 = ph->Drawable(i2);
	return pdrwi1->dtType == pdrwi2->dtType &&
			pdrwi1->Area() == pdrwi2->Area() &&
			pdrwi1->isVisible == pdrwi2->isVisible &&
			pdrwi1->zOrder == pdrwi2->zOrder
		;
}

QuadArea AreaForQRect(QRectF rect)
{
	return QuadArea(rect.x(), rect.y(), rect.width(), rect.height());
}

qreal RotationAlpha(MyRotation rot, qreal alpha)
{
	switch (rot)
	{
		case rotAlpha:
			break;
		case rotL90: alpha = 90; break;	// top left -> bottom left
		case rotR90: alpha = -90; break;
		case rot180: alpha = 180; break;
		default:
			break;
	}
	return alpha;
}

/*=============================================================
 * TASK:   Rotate a rectangle
 * PARAMS:	rot - rotation kind
 *			rect - rectangle to rotate input and output
 *			alphaInDegrees - in degrees, meaningful only with 'rot == rotAlpha'
 *					otherwise 0
 * GLOBALS:
 * RETURNS: if rotation is allowed (i.e. no point of rotated
 *			rectangle has any negative coordinates) returns true
 *			and sets modified rectangle into rect. Otherwise
 *			returns false and rect is unchanged;
 * REMARKS: - this rectangle is not inside any other 
 *				rectangle 
 *------------------------------------------------------------*/
bool RotateRect(MyRotation rot, QRectF &rect, QRectF inThisRect, qreal alphaInDegrees)
{
	if (rot == rotFlipH || rot == rotFlipV || rot == rot180)
		return true;
	else
	{
		alphaInDegrees = RotationAlpha(rot, alphaInDegrees);
		QPointF c = inThisRect.center();

		QTransform tr;
		tr.rotate(alphaInDegrees);

		auto __RotR = [&](QRectF& r)
		{
			r = r.translated(-c);
			r = tr.map(r).boundingRect();
			r = r.translated(c);
		};

		QRectF r = inThisRect;
		__RotR(r);
		if (r.top() < 0 || r.left() < 0)
		{
			QMessageBox::warning(nullptr, QObject::tr("FalconG - Warning"), QObject::tr("Can't rotate, as part of rotated area would be outside 'paper'"));
			return false;
		}

		__RotR(rect);
		return true;
	}
}


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
// the area of the Drawable, which already contains the 2 x (half pen width +1) width 
// and height extension is extended further by the 
// half width of the eraser pen, therefore th eraser strokes need not be adjusted
// when intersections are examined
int DrawableItem::AddEraserStroke(int eraserWidth, const QPolygonF& stroke)
{
	EraserData erdata;
	erdata.eraserPenWidth = eraserWidth;  	// common for all strokes
	qreal e2 = eraserWidth / 2.0 + 1;
		  
	QRectF rect = Area().adjusted(-e2, -e2, e2, e2),
		   erect = stroke.size() > 1 ? stroke.boundingRect() : QRectF(stroke.at(0).x()-eraserWidth/2.0+1, stroke.at(0).y() - eraserWidth / 2.0 + 1, eraserWidth,eraserWidth);

	if (rect.contains(erect) || (stroke.size() == 1 && rect.intersects(erect) ) )	// if full eraser stroke is inside
	{
		erdata.eraserStroke = stroke;
		erasers.push_back(erdata);
		return 1;
	}
	// find continouos parts of stroke that intersects extended area
	// and add them to erasers
	int countOfStrokes = 0;		

	// cut up eraser strokes to sections inside 'rect'
		// skip those outside
	QPointF prevP, x = stroke[0];		// previous point on stroke curve
	bool inside = rect.contains(prevP), 
		 prevInside;
	int ix = 1;		// index in stroke for next point
	for (; ix < stroke.size(); ++ix)
	{
		prevP = x;
		prevInside = inside;
		x = stroke.at(ix);
		inside = rect.contains(x);				// possibilities:
		if ((!prevInside && inside) ||			//   prevP outside and x inside   => add prevP
			(prevInside && inside))				//   prevP inside and x inside    => add prevP
			erdata.eraserStroke << prevP;
		else if (prevInside && !inside)
			erdata.eraserStroke << prevP << x;	//   prevP inside and x outside   => add prevP AND add x!
		else									//   prevP outside and x outside  => drop prevP maybe start new stroke
		{
			if (!erdata.eraserStroke.isEmpty())
			{
				erasers.push_back(erdata);
				erdata.eraserStroke.clear();
				++countOfStrokes;
			}
		}
	}
	// last point was not added
	if (!(prevInside && !inside))
		erdata.eraserStroke << x;

	if (!erdata.eraserStroke.isEmpty())
	{
		erasers.push_back(erdata);
		++countOfStrokes;
	}
	return countOfStrokes;
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
	{
		startPos += dr;
		for (auto &e : erasers)
			e.eraserStroke.translate(dr);
	}
}

/*=============================================================
 * TASK:	rotates eraser strokes
 * PARAMS:	rot - what kind
 *			inThisRectangle - original (not rotated) bounding rectangle
 *			alpha - for 'rotAlpha' rotational angle in degrees
 * GLOBALS:
 * RETURNS:	nothing + erasers rotated
 * REMARKS: - only call this if rotation is allowed!
 *------------------------------------------------------------*/
void DrawableItem::_RotateErasers(MyRotation rot, QRectF inThisrectangle, qreal alpha)
{
	QTransform tr;
	alpha = RotationAlpha(rot, alpha);
	tr.rotate(alpha); // common for all 
	QPointF c = inThisrectangle.center();

	for (auto &er : erasers)
	{
		er.eraserStroke.translate(-c);
		if (rot == rotFlipH)
		{
			for (auto& pt : er.eraserStroke)
				pt.setX(-pt.x());
		}
		else if (rot == rotFlipV)
		{
			for (auto& pt : er.eraserStroke)
				pt.setY(-pt.y());
		}
		else
			er.eraserStroke = tr.map(er.eraserStroke);

		er.eraserStroke.translate(c);
	}
}
bool DrawableItem::CanRotate(MyRotation rot, QRectF enclosingRectangle, qreal alpha)
{
	return RotateRect(rot, enclosingRectangle, enclosingRectangle, alpha);
}

/*=============================================================
 * TASK:	common for all rotations
 * PARAMS:	rot - what kind
 *			inThisRectangle - original (not rotated) bounding rectangle
 *			alpha - for 'rotAlpha' rotational angle in degrees
 * GLOBALS:
 * RETURNS: if rotation allowed the center point of 'inThisRectangle'
 *			else QPointF(-1,-1)
 * REMARKS: - inThisRectangle and alpha may be modified
 *------------------------------------------------------------*/
QPointF DrawableItem::_RotateCommon(MyRotation rot, QRectF inThisRectangle, qreal& alpha)
{
	if(!CanRotate(rot, inThisRectangle, alpha))
		return QPointF(-1,-1);

	alpha = RotationAlpha(rot, alpha);
	QPointF c = inThisRectangle.center();
	if(!erasers.isEmpty())
		_RotateErasers(rot, inThisRectangle, alpha);
	return c;
}

void DrawableItem::Rotate(MyRotation rot, QRectF &inThisRectangle, qreal alpha)
{
	QPointF c = _RotateCommon(rot, inThisRectangle, alpha);
	if (c.x() < 0)
		return;

	QPointF s = startPos - c;

	if (rot == rotFlipH)
	{
		s.setX(-s.x());
	}
	else if (rot == rotFlipV)
	{
		s.setY(-s.y());
	}
	else
	{
		QTransform tr;
		tr.rotate(alpha);
		s = tr.map(s);
	}
	s = s + c;
	if (s.x() < 0 || s.y() < 0)	// can't rotate
		return;
	startPos = s;
}

// drawables MUST be saved in increasing zOrder, so no need to save the zOrder
// this way all screenshots are saved first, followed by all other drawables

QDataStream& operator<<(QDataStream& ofs, const DrawableItem& di)
{
	ofs << (int)di.dtType << di.startPos << (int)di.PenKind() /*<< di.PenColor()*/ << di.penWidth << di.angle;
	ofs << di.erasers.size();
	for (auto e : di.erasers)
	{
		ofs << e.eraserPenWidth << e.eraserStroke;
	}

	switch (di.dtType)
	{
		case DrawableType::dtCross:			ofs<<(DrawableCross&)di; break;
		case DrawableType::dtDot:			ofs<<(DrawableDot&)di; break;
		case DrawableType::dtEllipse:		ofs<<(DrawableEllipse&)di; break;
		case DrawableType::dtLine:			ofs<<(DrawableLine&)di; break;
		case DrawableType::dtRectangle:		ofs<<(DrawableRectangle&)di; break;
		case DrawableType::dtScreenShot:	ofs<<(DrawableScreenShot&)di; break;
		case DrawableType::dtScribble:		ofs<<(DrawableScribble&)di; break;
		case DrawableType::dtText:			ofs<<(DrawableText&)di; break;
		default: break;
	}

	return ofs;
}
QDataStream& operator>>(QDataStream& ifs, DrawableItem& di)		// zorder was not saved, so not set, must set it after all drawables read in
{
	int n;
	ifs >> n; di.dtType = (DrawableType)n;
	ifs >> di.startPos;
	ifs >> n;
	di.SetPenKind((FalconPenKind)n);
	ifs /*>> di.penColor*/ >> di.penWidth >> di.angle;
	ifs >> n;
	while (n--)
	{
		DrawableItem::EraserData ed;
		ifs >> ed.eraserPenWidth >> ed.eraserStroke;
		di.erasers.push_back(ed);
	}
	// the real (derived class)  data cannot be read here into a DrawableItem
	// that must be handled by the caller of this operator
	return ifs;
}


//=====================================
// DrawableDot
//=====================================

// handled by DrawableItem:
// void DrawableDot::Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR) 

QDataStream& operator<<(QDataStream& ofs, const DrawableDot& di)
{
	return ofs;	// already saved as the common ancestor DrawableItem is saved
}
QDataStream& operator>>(QDataStream& ifs, DrawableDot& di)			  // call AFTER header is read in
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

void DrawableCross::Rotate(MyRotation rot, QRectF &inThisRectangle, qreal alpha)
{
	QPointF c = _RotateCommon(rot, inThisRectangle, alpha);
	if (c.x() < 0)
		return;

	QPointF s = startPos - c;
	if (rot == rotFlipH)
	{
		s.setX(-s.x());
	}
	else if (rot == rotFlipV)
	{
		s.setY(-s.y());
	}
	else
	{
		QTransform tr;
		tr.rotate(alpha);
		s = tr.map(s);
	}
	s = s + c;
	if (s.x() < 0 || s.y() < 0)	// can't rotate
		return;
	startPos = s;
}

void DrawableCross::Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)
{
	if (drawStarted)
	{
		SetPainterPenAndBrush(painter, clipR.translated(-topLeftOfVisibleArea));
		QPointF dist(length / sqrt(2), length / sqrt(2));
		QRectF rect(startPos - dist, startPos + dist);
		rect.translate(-topLeftOfVisibleArea);
		painter->drawLine(rect.topLeft(), rect.bottomRight());
		painter->drawLine(rect.topRight(), rect.bottomLeft());
	}
	else
		DrawWithEraser(painter, topLeftOfVisibleArea, clipR);
}

QDataStream& operator<<(QDataStream& ofs, const DrawableCross& di)
{
	ofs << di.length;
	return ofs;

}
QDataStream& operator>>(QDataStream& ifs, DrawableCross& di)	  // call AFTER header is read in
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
		DrawableItem::Translate(dr, minY);
		rect.moveTo(rect.topLeft() + dr);
	}
}

/*=============================================================
 * TASK:	rotates ellipse in a rectangle
 * PARAMS:	rot - rotation Do not use 'rotAlpha' here
 *			inThisRectangle - rotate around the center of this
 *			rectangle
 *			alpha - must not be used
 * GLOBALS:
 * RETURNS: nothing, ellipse is rotated by a multiple of 90 degrees
 * REMARKS: - to rotate an ellipse by an angle 'alpha'
 *				create a new DrawableScribble from this and rotate that
 *------------------------------------------------------------*/
void DrawableEllipse::Rotate(MyRotation rot, QRectF& inThisRectangle, qreal alpha)
{
	QPointF c = _RotateCommon(rot, inThisRectangle, alpha);
	if (c.x() < 0)
		return;

	rect.translate(-c);
	if (rot == rotFlipH)
	{
		qreal w = rect.width();
		rect.setX(-rect.right());	// top left is replaced by original top right
		rect.setWidth(w);
	}
	else if (rot == rotFlipV)
	{
		qreal h = rect.height();
		rect.setY(-rect.bottom());
		rect.setHeight(h);
	}
	else
	{
		QTransform tr;
		tr.rotate(alpha);
		rect = tr.map(rect).boundingRect();
	}
	rect.translate(c);
	startPos = rect.topLeft();
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

QDataStream& operator<<(QDataStream& ofs, const DrawableEllipse& di) // topmost 
{
	ofs << di.Area() << di.isFilled;
	return ofs;

}

QDataStream& operator>>(QDataStream& ifs, DrawableEllipse& di)	  // call AFTER header is read in
{
	ifs >> di.rect >> di.isFilled;
	return ifs;
}
//=====================================
// DrawableScribble
//=====================================


DrawableLine::DrawableLine(QPointF startPos, QPointF endPoint, int zorder, FalconPenKind penKind, qreal penWidth, bool isFilled) : 
	endPoint(endPoint), DrawableItem(DrawableType::dtLine, startPos, zOrder, penKind, penWidth)
{

}

DrawableLine::DrawableLine(const DrawableLine& ol)
{
	*this = ol;
}

DrawableLine::DrawableLine(DrawableLine&& ol)
{
	*this = ol;
}

DrawableLine& DrawableLine::operator=(const DrawableLine& ol)
{
	*(DrawableItem*)this = (DrawableItem&&)ol;

	endPoint = ol.endPoint;
	return *this;
}

DrawableLine& DrawableLine::operator=(const DrawableLine&& ol)
{
	*(DrawableItem*)this = (DrawableItem&&)ol;

	endPoint = ol.endPoint;
	return *this;
}

void DrawableLine::Translate(QPointF dr, qreal minY)
{
	if (startPos.y() > minY)
	{
		DrawableItem::Translate(dr, minY);
		endPoint += dr;
	}
}

void DrawableLine::Rotate(MyRotation rot, QRectF &inThisRectangle, qreal alpha)
{
	QPointF c = _RotateCommon(rot, inThisRectangle, alpha);
	if (c.x() < 0)
		return;
	QPointF s, e;

	s = startPos; s -= c;
	e = endPoint; e -= c;

	if (rot == rotFlipH)
	{
		s.setX(-s.x());
		e.setX(-e.x());
	}
	else if (rot == rotFlipV)
	{
		s.setY(-s.y());
		e.setY(-e.y());
	}
	else 
	{
		QLineF line(s, e);

		QTransform tr;
		tr.rotate(alpha);
		line = tr.map(line);
		s = line.p1();
		e = line.p2();
	}
	startPos =s + c;
	endPoint =e + c;
}

void DrawableLine::Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)
{
	if (drawStarted)
	{
		SetPainterPenAndBrush(painter, clipR.translated(-topLeftOfVisibleArea), QColor());
		painter->drawLine(startPos-topLeftOfVisibleArea, endPoint-topLeftOfVisibleArea);
	}
	else
		DrawWithEraser(painter, topLeftOfVisibleArea, clipR);
}


QDataStream& operator<<(QDataStream& ofs, const DrawableLine& di) // DrawableItem part already saved
{
	ofs << di.endPoint;
	return ofs;
}

QDataStream& operator>>(QDataStream& ifs, DrawableLine& di)		  // call AFTER header is read in
{
	ifs >> di.endPoint;
	return ifs;
}


//=====================================
// DrawableRectangle
//=====================================
DrawableRectangle::DrawableRectangle(QRectF rect, int zOrder, FalconPenKind penKind, qreal penWidth, bool isFilled) : 
	rect(rect), isFilled(isFilled), DrawableItem(DrawableType::dtRectangle, rect.topLeft(), zOrder, penKind, penWidth) {}
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
		DrawableItem::Translate(dr, minY);
		rect.moveTo(rect.topLeft() + dr);
	}
}

/*=============================================================
 * TASK:	rotates this rectangle in an enclosing rectangle
 * PARAMS:	rot - rotation Do not use 'rotAlpha' here
 *			inThisRectangle - rotate around the center of this
 *			rectangle
 *			alpha - must not be used
 * GLOBALS:
 * RETURNS: nothing, ellipse is rotated by a multiple of 90 degrees
 * REMARKS: - to rotate an ellipse by an angle 'alpha'
 *				create a new DrawableScribble from this and rotate that
 *------------------------------------------------------------*/
void DrawableRectangle::Rotate(MyRotation rot, QRectF &inThisRectangle, qreal alpha)     // alpha used only for 'rotAlpha'
{
	QPointF c = _RotateCommon(rot, inThisRectangle, alpha);
	if (c.x() < 0)
		return;

	rect.translate(-c);
	if (rot == rotFlipH)
	{
		qreal w = rect.width();
		rect.setX(-rect.right());	// top left is replaced by original top right
		rect.setWidth(w);
	}
	else if (rot == rotFlipV)
	{
		qreal h = rect.height();
		rect.setY(-rect.bottom());
		rect.setHeight(h);
	}
	else
	{
		QTransform tr;
		tr.rotate(alpha);
		rect = tr.map(rect).boundingRect();
	}
	rect.translate(c);
	startPos = rect.topLeft();
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

QDataStream& operator<<(QDataStream& ofs, const DrawableRectangle& di) // DrawableItem part already saved
{
	ofs << di.Area() << di.isFilled;
	return ofs;

}
QDataStream& operator>>(QDataStream& ifs, DrawableRectangle& di)		  // call AFTER header is read in
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

QRectF DrawableScreenShot::Area() const
{
	return QRectF(startPos, image.size());
}

QRectF DrawableScreenShot::AreaOnCanvas(const QRectF& canvasRect) const
{
	return Area().intersected(canvasRect);
}

void DrawableScreenShot::Rotate(MyRotation rot, QRectF &inThisRectangle, qreal alpha)
{
	QRectF rect = Area();
	alpha = RotationAlpha(rot, alpha);
	if (!RotateRect(rot, rect, inThisRectangle, alpha))
		return;
	startPos = rect.topLeft();
	QPointF c = inThisRectangle.center();
	_RotateErasers(rot, inThisRectangle, alpha);

	QTransform transform;
	transform.rotate(alpha);
	QImage img;
	bool fliph = false, flipv = true;	// default flip orientations
	switch (rot)
	{
		case rotFlipH:
			fliph = true;
			flipv = false;		// NO break here!
		case rotFlipV:
			img = image.toImage();
			img = img.mirrored(fliph, flipv);
			image = image.fromImage(img);
			break;
		default:
			image = image.transformed(transform, Qt::SmoothTransformation);
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
QDataStream& operator<<(QDataStream& ofs, const DrawableScreenShot& bimg) // DrawableItem part already saved
{
	ofs << bimg.image;

	return ofs;
}

QDataStream& operator>>(QDataStream& ifs, DrawableScreenShot& bimg)	  // call AFTER header is read in
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

void DrawableScribble::Clear()
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
	return (vpp.y() * vp.x() == vp.y() * vpp.x()) && ((vp.x() != vpp.x() && vp.x() * vpp.x() > 0) || (vp.y() != vpp.y() && vp.y() * vpp.y() > 0));
}

QPointF DrawableScribble::Add(QPointF p, bool smoothed, bool reset)	// only use smoothed = true when drawing by hand
{																	// neither when reading or copying
	static Smoother<QPointF, qreal, 200> smoother;					// Use reset when new drawing starts
	if (reset)
	{
		smoother.Reset();
		points.clear();
		if (p == QPoint(-1, -1))
			return p;
	}

	if (smoothed && bSmoothDebug)
		p = smoother.AddPoint(p);

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
	return p;
}

void DrawableScribble::Add(int x, int y, bool smoothed, bool reset)
{
	QPointF p(x, y);
	Add(p, smoothed, reset);
}

void DrawableScribble::Reset()
{
	Add(QPoint(-1,-1), false, true);	// clear 'points' and reset smoother
}

bool DrawableScribble::Intersects(const QRectF& arect) const
{
	return Area().intersects(arect);
}

void DrawableScribble::Translate(QPointF dr, qreal  minY)
{
	if (Area().y() < minY || !isVisible)
		return;

	DrawableItem::Translate(dr, minY);
	points.translate(dr);

}

void DrawableScribble::Rotate(MyRotation rot, QRectF &inThisRectangle, qreal alpha)	// rotate around the center of encRect
{
#if 1
	QPointF c = _RotateCommon(rot, inThisRectangle, alpha);
	if (c.x() < 0)
		return;

	points.translate(-c);

	if (rot == rotFlipH)
	{
		for (auto& pt : points)
			pt.setX(-pt.x());
	}
	else if (rot == rotFlipV)
	{
		for (auto& pt : points)
			pt.setY(-pt.y());
	}
	else
	{
		QTransform tr;
		tr.rotate(alpha);
		points = tr.map(points);
	}

	points.translate(c);
#else
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
#endif
	//if (rot == rotNone)		// save rotation for undo
	//	rot = rot;
	//else
	//	rot = rotNone;		// undone: no rotation set yet
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


QDataStream& operator<<(QDataStream& ofs, const DrawableScribble& di) // after header, type DrawableItem was saved
{
	ofs << di.points;
	return ofs;
}
// reads ONLY after the type is read in!
QDataStream& operator>>(QDataStream& ifs, DrawableScribble& di)	  // call AFTER header is read in
{
	qreal x, y;
	di.points.clear();

	ifs >> di.points;

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

QDataStream& operator<<(QDataStream& ofs, const DrawableText& di) // DrawableItem part already saved
{
	ofs << di.fontAsString << di._text;
	return ofs;
}
QDataStream& operator>>(QDataStream& ifs, DrawableText& di)           // call AFTER header is read in
{
	ifs >> di.fontAsString >> di._text;
	return ifs;
}

//=====================================
// DrawableList
//=====================================
IntVector DrawableList::ListOfItemIndicesInRect(QRectF& r)	const
{
	return _pQTree ? _pQTree->GetValues(&_items, r) : IntVector();
}

IntVector DrawableList::ListOfItemIndicesInQuadArea(QuadArea& q) const
{
	return _pQTree ? _pQTree->GetValues(&_items, q) : IntVector();
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
