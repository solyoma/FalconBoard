#include <QMessageBox>
#include "drawables.h"
#include "history.h"
#include "smoother.h"

#if !defined _VIEWER && defined _DEBUG
	bool isDebugMode = false;
#endif

static bool __IsLineNearToPoint(QPointF p1, QPointF p2, QPointF& ccenter, qreal r)   // line between p2 and p1 is inside circle w. radius r around point 'ccenter'
{
#define SQR(x)  ((x)*(x))
#define DIST2(a,b) (SQR((a).x() - (b).x()) + SQR((a).y() - (b).y()))
	// simple checks:
	if (DIST2(p1, ccenter) > r && DIST2(p2, ccenter) > r && DIST2(p1, p2) < 2 * r)
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
	else if (EqZero(discr))  // floating point, no abs. 0 test
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
		if (EqZero(abs(p2.y() - p1.y())))      // vertical line
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


	// static member
qreal	DrawableItem::yOffset = 0.0;


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

qreal Round(qreal number, int dec_digits)
{
	return round(number * pow(10.0f, (float)dec_digits)) * pow(10.0f, (float)-dec_digits);
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


/*=============================================================
 * TASK:   Rotate an enclosing rectangle
 * PARAMS:	rot - rotation kind
 *			rect - rectangle to rotate input and output
 *			alphaInDegrees - in degrees, meaningful only with 'rot == MyRotation::rotAngle'
 *					otherwise 0
 * GLOBALS:
 * RETURNS: if rotation is allowed (i.e. no point of rotated
 *			rectangle has any negative coordinates) returns true
 *			and sets modified rectangle into rect. Otherwise
 *			returns false and rect is unchanged;
 * REMARKS: - this rectangle is not inside any other 
 *				rectangle 
 *------------------------------------------------------------*/


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

/* *********************** MyRotation ********************/

constexpr qreal MyRotation::AngleForType(Type myrot)
{

	qreal alpha = 0.0;
	switch (myrot)
	{
	case rotL90: alpha = -90; break;	// top left -> bottom left
	case rotR90: alpha = 90; break;
	case rot180: alpha = 180; break;
	default:                        // for no rotation and flips
		break;
	}
	return alpha;
}

constexpr MyRotation::Type MyRotation::TypeForAngle(qreal angle)
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

MyRotation& MyRotation::operator=(Type typ)
{
	_SetRot((angle = AngleForType(typ)));

	if (IsFlipType(typ))
		flipType = typ;
	return *this;
}

MyRotation& MyRotation::operator=(qreal anAngle)
{
	_SetRot(anAngle);
	flipType = flipNone;
	return *this;
}

void MyRotation::AddAngle(qreal anAngle)
{
	if (flipType != flipNone)
		anAngle = -anAngle;         // flip + angle = -angle + flip

	_SetRot(angle + anAngle);
}

void MyRotation::AddType(Type typ)
{
	if (!IsFlipType(typ))   // then rotation
		_SetRot(angle + AngleForType(typ));
	else                    // then flip or no flip
	{
		if (flipType == flipNone)     // none + flip = flip
			flipType = typ;
		else if (typ != flipNone)      // flip1 + flip2 =   +0 rotation and flipNone for flip1==flip2
		{                             //               = +180 rotation and flipnone for flip1 != flip2
			if (flipType != typ)
				_SetRot(angle + 180);
			flipType = flipNone;
		}
	}
}

MyRotation MyRotation::AddRotation(MyRotation arot)
{
	if (arot.flipType != flipNone)   // then no rotation
		AddType(arot.flipType);
	else                             // pure rotation, no flip
		AddAngle(arot.angle);
	return *this;
}

bool MyRotation::ResultsInNoRotation(MyRotation& arot, bool markIfNoRotation)
{
	MyRotation tmpRot = *this;
	tmpRot.AddRotation(arot);
	return tmpRot.flipType == flipNone && !tmpRot.angle;
}

bool MyRotation::RotateRect(QRectF& rect, QPointF center, bool noCheck) const
{
	bool result = true;
	QRectF r = rect;
	r.translate(-center);

	if (angle)   // angle == 0.0 check is ok
		r = _tr.map(r).boundingRect();

	if (flipType != flipNone)
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

bool MyRotation::RotatePoly(QPolygonF& points, QPointF center, bool noCheck) const
{
	QRectF bndRect = points.boundingRect();

	points.translate(-center);
	if (!noCheck && !RotateRect(bndRect, center, noCheck)) // must or could rotate
		return false;

	if (angle)
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

bool MyRotation::RotateSinglePoint(QPointF& pt, QPointF center, bool noCheck, bool invertRotation) const
{
	QPointF p = pt;
	MyRotation mr = *this;
	p -= center;
	if (invertRotation)
		mr.InvertAngle();
	p = mr._tr.map(p);

	if (flipType == rotFlipH)
	{
		p.setX(-p.x());
	}
	else if (flipType == rotFlipV)
	{
		p.setY(-p.y());
	}
	p = p + center;

	if (!noCheck && (p.x() < 0 || p.y() < 0))
	{
		_CantRotateWarning();
		return false;
	}

	pt = p;
	return true;
}

bool MyRotation::RotatePixmap(QPixmap& img, QPointF topLeft, QPointF center, bool noCheck) const
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

bool MyRotation::RotateLine(QLineF& line, QPointF center, bool noCheck)
{
	QLineF l = line;

	l.translate(-center);
	l = _tr.map(l);       // always rotation first

	if (flipType == rotFlipH)
	{
		l.setP1(QPointF(-l.p1().x(), l.p1().y()));
		l.setP2(QPointF(-l.p1().x(), l.p1().y()));
	}
	else if (flipType == rotFlipV)
	{
		l.setP1(QPointF(l.p1().x(), -l.p1().y()));
		l.setP2(QPointF(l.p1().x(), -l.p1().y()));
	}
	l.translate(center);
	if (noCheck || (l.p1().x() >= 0 && l.p1().y() >= 0 && l.p2().x() >= 0 && l.p2().y() >= 0))
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

void MyRotation::_CantRotateWarning() const
{
	QMessageBox::warning(nullptr, QObject::tr("FalconG - Warning"), QObject::tr("Can't rotate, as part of rotated area would be outside 'paper'"));
}

void MyRotation::_SetRot(qreal alpha)
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

QDataStream& operator<<(QDataStream& ofs, const MyRotation& mr)
{
	ofs << mr.angle << (byte)mr.flipType;
	return ofs;
}
QDataStream& operator>>(QDataStream& ifs, MyRotation& mr)
{
	byte ch;
	qreal alpha;
	ifs >> alpha >> ch;
	mr = alpha;		// sets transformation matrix too and fliptype to flipNone
	mr.flipType = (MyRotation::Type)ch;	// sets angle to 0.0

	return ifs;
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
	refPoint = other.refPoint;
	zOrder = other.zOrder;
	rot = other.rot;
	isVisible = other.isVisible;
	erasers = other.erasers;

	return *this;
}
// add eraser strokes cut up to as many parts as required
// the area of the Drawable, which already contains the 2 x (half pen width +1) width 
// and height extension is extended further by the 
// half width of the eraser pen, therefore the eraser strokes need not be adjusted
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
	if (refPoint.y() > minY)
	{
		refPoint += dr;
		for (auto &e : erasers)
			e.eraserStroke.translate(dr);
	}
}

/*=============================================================
 * TASK:	rotates eraser strokes
 * PARAMS:	rot - what kind
 *			center - original (not rotated) bounding rectangle
 *			alpha - for 'MyRotation::rotAngle' rotational angle in degrees
 * GLOBALS:
 * RETURNS:	nothing + erasers rotated
 * REMARKS: - only call this if rotation is allowed!
 *------------------------------------------------------------*/
void DrawableItem::_RotateErasers(MyRotation rot, QPointF center)
{
	if (erasers.isEmpty())
		return;

	for (auto &er : erasers)
		rot.RotatePoly(er.eraserStroke,center, true);
}

void DrawableItem::Rotate(MyRotation arot, QPointF & center)
{
	arot.RotateSinglePoint(refPoint, center, penWidth);	// may show warning and not change refPoint
	rot.AddRotation(arot);
}

// drawables MUST be saved in increasing zOrder, so no need to save the zOrder
// this way all screenshots are saved first, followed by all other drawables

QDataStream& operator<<(QDataStream& ofs, const DrawableItem& di)
{
	ofs << (int)di.dtType << di.refPoint << (int)di.PenKind() /*<< di.PenColor()*/ << di.penWidth << di.rot;
	ofs << di.erasers.size();
	MyRotation arot = di.rot;
	arot.InvertAngle();		// for erasers

	for (auto e : di.erasers)					  // eraser strokes for items that are rotated AFTER read are saved un-rotated
	{											  // flip is its own inverse, no need to invert it
		if (di.dtType != DrawableType::dtDot && di.dtType != DrawableType::dtScribble)
			arot.RotatePoly(e.eraserStroke, di.Area().center(), true);
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
	// special handling for data of not really drawables
	if (n >= (int)DrawableType::dtNonDrawableStart)
		return ifs;

	ifs >> di.refPoint;
	di.refPoint += {0, DrawableItem::yOffset};
	ifs >> n;
	di.SetPenKind((FalconPenKind)n);
	ifs /*>> di.penColor*/ >> di.penWidth >> di.rot;
	ifs >> n;		// number of eraser strokes
	while (n--)
	{
		DrawableItem::EraserData ed;
		ifs >> ed.eraserPenWidth >> ed.eraserStroke;	// saved un-rotated when items read are rotated after, no need to rotate them
		if(DrawableItem::yOffset)
			ed.eraserStroke.translate({ 0, DrawableItem::yOffset } );
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
DrawableCross::DrawableCross(QPointF pos, qreal len, int zOrder, FalconPenKind penKind, qreal penWidth) : 
		length(len), DrawableItem(DrawableType::dtCross, pos, zOrder, penKind, penWidth) 
{
	_Setup();
}

QRectF DrawableCross::Area() const    // includes half of pen width+1 pixel
{
	qreal w = penWidth / 2.0;
	qreal x1 = std::min(_ltrb.x1(), _ltrb.x2()),
		y1 = std::min(_ltrb.y1(), _ltrb.y2()),
		x2 = std::max(_ltrb.x1(), _ltrb.x2()),
		y2 = std::max(_ltrb.y1(), _ltrb.y2());
	x1 = Round(std::min(x1, std::min(_lbrt.x1(), _lbrt.x2())), 3);
	x2 = Round(std::max(x2, std::max(_lbrt.x1(), _lbrt.x2())), 3);
	y1 = Round(std::min(y1, std::min(_lbrt.y1(), _lbrt.y2())), 3);
	y2 = Round(std::max(y2, std::max(_lbrt.y1(), _lbrt.y2())), 3);
	QRectF rect = QRectF(x1 - w, y1 - w, x2 - x1 + 2 * w, y2 - y1 + 2 * w);
	return rect;
}

void DrawableCross::Translate(QPointF dr, qreal minY)
{
	qreal y = _ltrb.p1().y();	// find smallest y for cross
	if (_ltrb.p2().y() < y)
		y = _ltrb.p1().y();
	if (_lbrt.p1().y() < y)
		y = _lbrt.p1().y();
	if (_lbrt.p2().y() < y)
		y = _lbrt.p1().y();

	if (y >= minY)				// if even the smallest y is below minY
	{
		refPoint += dr;
		_ltrb.translate(dr);
		_lbrt.translate(dr);
	}
}

void DrawableCross::Rotate(MyRotation arot, QPointF & center)
{
	QLineF l = _ltrb;
	if (arot.RotateLine(l, center, true) &&
		arot.RotateLine(_lbrt, center, true))
	{
		_ltrb = l;	// maybe ltrb could be rotated, but lbrt not then we are protected
		arot.RotateSinglePoint(refPoint, center, true);
		_RotateErasers(arot, center);
		rot.AddRotation(arot);
	}
}

void DrawableCross::Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)
{
	if (drawStarted)
	{
		SetPainterPenAndBrush(painter, clipR.translated(-topLeftOfVisibleArea));
		painter->drawLine(_ltrb.translated(-topLeftOfVisibleArea));
		painter->drawLine(_lbrt.translated(-topLeftOfVisibleArea));
	}
	else
		DrawWithEraser(painter, topLeftOfVisibleArea, clipR);
}
void DrawableCross::_Setup()
{
	qreal dlen = length / sqrt(2);
	_ltrb = QLine(refPoint.x() - dlen, refPoint.y() - dlen, refPoint.x() + dlen, refPoint.y() + dlen);
	_lbrt = QLine(refPoint.x() - dlen, refPoint.y() + dlen, refPoint.x() + dlen, refPoint.y() - dlen);
}

QDataStream& operator<<(QDataStream& ofs, const DrawableCross& di)
{
	ofs << di.length;
	return ofs;

}
QDataStream& operator>>(QDataStream& ifs, DrawableCross& di)	  // call AFTER header is read in
{
	ifs >> di.length;
	di._Setup();
	if (di.rot.angle || di.rot.flipType != MyRotation::flipNone)
	{
		MyRotation aRot = di.rot;
		di.rot = MyRotation();
		di.Rotate(aRot, di.refPoint);
	}
	return ifs;
}

//=====================================
// DrawableEllipse
//=====================================
DrawableEllipse::DrawableEllipse(QRectF rect, int zOrder, FalconPenKind penKind, qreal penWidth, bool isFilled) : 
		rect(rect), isFilled(isFilled), DrawableItem(DrawableType::dtEllipse, rect.center(), zOrder, penKind, penWidth), _rotatedRect(rect) {}
DrawableEllipse::DrawableEllipse(const DrawableEllipse& o) { *this = o; }
DrawableEllipse::DrawableEllipse(DrawableEllipse&& o) noexcept { *this = o; }
DrawableEllipse& DrawableEllipse::operator=(const DrawableEllipse& di)
{
	*(DrawableItem*)this = (const DrawableItem&)di;

	isFilled = di.isFilled;
	rect = di.rect;
	rot = di.rot;
	_points = di._points;
	_rotatedRect = di._rotatedRect;
	return *this;
}

DrawableEllipse& DrawableEllipse::operator=(DrawableEllipse&& di) noexcept
{
	*(DrawableItem*)this = (DrawableItem&&)di;

	isFilled = di.isFilled;
	rect = di.rect;
	rot = di.rot;
	_rotatedRect = di._rotatedRect;
	return *this;
}

void DrawableEllipse::Translate(QPointF dr, qreal minY)
{
	if (_rotatedRect.top() > minY)
	{
		DrawableItem::Translate(dr, minY);
		rect.moveTo(rect.topLeft() + dr);
		_rotatedRect.moveTo(_rotatedRect.topLeft() + dr);
		if (!_points.isEmpty())
			_points.translate(dr);
	}
}

/*=============================================================
 * TASK:	rotates ellipse in a rectangle
 * PARAMS:	arot - rotation to perform
 *			center - rotate around this point
 * EXPECTS: the original rectangle is rotated
 * GLOBALS:
 * RETURNS: nothing, ellipse is rotated 
 * REMARKS: - Always rotates original ellipse
 *			- When rotated by a degree of integer times 90
 *			  only the rectangle is rotated
 *			- for all other angles the ellipse is rasterized into '_points'
 *			- refPoint is adjusted to be again the center of the rotated
 *			  ellipse
 *------------------------------------------------------------*/
void DrawableEllipse::Rotate(MyRotation arot, QPointF& centerOfRotation)
{
	MyRotation tmpr = rot;		// previous rotation
	QRectF r = rect;			// original rectangle. Modified if rotated by any multiple of 90

	tmpr.AddRotation(arot);		// check if rotation is possible before doing any changes
	if (!tmpr.RotateRect(r, centerOfRotation))
		return;

	rot = tmpr;					// set new rotation
	_points.clear();

	if ( tmpr.HasNoRotation() || tmpr.HasSimpleRotation())
	{
		_rotatedRect = rect = r;
		rot.Reset();
	}
	else
	{
		_ToPolygonF();
		tmpr.RotatePoly(_points, centerOfRotation, true);
		_rotatedRect = _points.boundingRect();
	}
	refPoint = _rotatedRect.center();
	_RotateErasers(arot, centerOfRotation);	// erasers are stored rotated so only apply last rotation to them
}

void DrawableEllipse::Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)
{
	if (drawStarted)
	{
		QColor c;
		if (isFilled)
			c = PenColor();
		SetPainterPenAndBrush(painter, clipR.translated(-topLeftOfVisibleArea), c);
		if (_points.isEmpty())
			painter->drawEllipse(_rotatedRect.translated(-topLeftOfVisibleArea));
		else   // rotation is arbitrary and not a multiple of 90 degrees and not a flip
			painter->drawPolygon(_points.translated(-topLeftOfVisibleArea)); // '_points' and '_rotatedRect' already rotated
	}
	else
		DrawWithEraser(painter, topLeftOfVisibleArea, clipR);
}

QDataStream& operator<<(QDataStream& ofs, const DrawableEllipse& di) // topmost 
{
	ofs << di.rect << di.isFilled;	// always save non-rotated rectangle + rotation +flip
	return ofs;

}

QDataStream& operator>>(QDataStream& ifs, DrawableEllipse& di)	  // call AFTER header is read in
{
	ifs >> di.rect >> di.isFilled;
	di.rect.translate(0, DrawableItem::yOffset);
	// eliipse always stored as having axes parallel to the coord axes
	// but even when not we must set the _rotatedRectangle in it
	MyRotation arot = di.rot;
	di.rot = MyRotation();		// no rotation, clear
	QPointF center = di.rect.center();
	di.Rotate(arot, center);

	return ifs;
}
//=====================================
// DrawableLine
//=====================================


DrawableLine::DrawableLine(QPointF refPoint, QPointF endPoint, int zorder, FalconPenKind penKind, qreal penWidth) : 
	endPoint(endPoint), DrawableItem(DrawableType::dtLine, refPoint, zOrder, penKind, penWidth)
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
	if (refPoint.y() > minY)
	{
		DrawableItem::Translate(dr, minY);
		endPoint += dr;
	}
}

void DrawableLine::Rotate(MyRotation arot, QPointF &center)
{
	_RotateErasers(arot, center);
	QPointF s, e;

	s = refPoint; s -= center;
	e = endPoint; e -= center;
		// first rotation
	QLineF line(s, e);

	QTransform tr;
	tr.rotate(arot.angle);
	line = tr.map(line);
	s = line.p1();
	e = line.p2();

		// then flip

	if (arot.flipType == MyRotation::rotFlipH)
	{
		s.setX(-s.x());
		e.setX(-e.x());
	}
	else if (arot.flipType == MyRotation::rotFlipV)
	{
		s.setY(-s.y());
		e.setY(-e.y());
	}
	refPoint = s + center;
	endPoint = e + center;
	rot.AddRotation(arot);
}

bool DrawableLine::PointIsNear(QPointF p, qreal distance) const
{
	return __IsLineNearToPoint(refPoint, endPoint, p, distance);
}

void DrawableLine::Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)
{
	if (drawStarted)
	{
		SetPainterPenAndBrush(painter, clipR.translated(-topLeftOfVisibleArea), QColor());
		painter->drawLine(refPoint-topLeftOfVisibleArea, endPoint-topLeftOfVisibleArea);
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
	di.endPoint += {0, DrawableItem::yOffset};
	di.rot = MyRotation();	// endpoints were rotated before save
	return ifs;
}


//=====================================
// DrawableRectangle
//=====================================
DrawableRectangle::DrawableRectangle(QRectF rect, int zOrder, FalconPenKind penKind, qreal penWidth, bool isFilled) : 
	rect(rect), _rotatedRect(rect), isFilled(isFilled), DrawableItem(DrawableType::dtRectangle, rect.topLeft(), zOrder, penKind, penWidth) {}
DrawableRectangle::DrawableRectangle(const DrawableRectangle& di) { *this = di; }
DrawableRectangle& DrawableRectangle::operator=(const DrawableRectangle& di)
{
	*(DrawableItem*)this = (DrawableItem&&)di;

	isFilled = di.isFilled;
	rect = di.rect;
	_rotatedRect = di._rotatedRect;
	_points = di._points;
	return *this;
}

DrawableRectangle& DrawableRectangle::operator=(DrawableRectangle&& di)  noexcept
{
	*(DrawableItem*)this = (DrawableItem&&)di;

	isFilled = di.isFilled;
	rect = di.rect;
	_rotatedRect = di._rotatedRect;
	_points = di._points;
	return *this;
}

void DrawableRectangle::Translate(QPointF dr, qreal minY)             // only if not deleted and top is > minY
{
	if (_rotatedRect.top() > minY)
	{
		DrawableItem::Translate(dr, minY);
		_rotatedRect.moveTo(_rotatedRect.topLeft() + dr);
		rect.moveTo(rect.topLeft() + dr);
		if (!_points.isEmpty())
			_points.translate(dr);
	}
}

/*=============================================================
 * TASK:	rotates this rectangle in an enclosing rectangle
 * PARAMS:	rot - rotation Do not use 'MyRotation::rotAngle' here
 *			center - rotate around the center of this
 *			rectangle
 *			alpha - must not be used
 * GLOBALS:
 * RETURNS: nothing, ellipse is rotated by a multiple of 90 degrees
 * REMARKS: - to rotate an ellipse by an angle 'alpha'
 *				create a new DrawableScribble from this and rotate that
 *------------------------------------------------------------*/
void DrawableRectangle::Rotate(MyRotation arot, QPointF & center)
{
	MyRotation tmpr = rot;		// previous rotation
	QRectF r = rect;			// original rectangle. Modified if rotated by any multiple of 90

	tmpr.AddRotation(arot);		// check if rotation is possible before doing any changes
	if (!tmpr.RotateRect(r, center))
		return;

	rot = tmpr;					// set new rotation
	_points.clear();

	if (tmpr.HasNoRotation() || tmpr.HasSimpleRotation())
	{
		_rotatedRect = rect = r;
		rot.Reset();
	}
	else
	{
		_ToPolygonF();
		tmpr.RotatePoly(_points, center, true);
		_rotatedRect = _points.boundingRect();
	}
	refPoint = _rotatedRect.topLeft();
	_RotateErasers(arot, center);	// erasers are stored rotated so only apply last rotation to them

}


void DrawableRectangle::Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)    // example: you must override this using this same structure
{
	if (drawStarted)
	{
		QColor c;
		if (isFilled)
			c = PenColor();
		SetPainterPenAndBrush(painter, clipR.translated(-topLeftOfVisibleArea), c);
		if (_points.isEmpty())
			painter->drawRect(_rotatedRect.translated(-topLeftOfVisibleArea));
		else   // rotation is arbitrary and not a multiple of 90 degrees and not a flip
			painter->drawPolygon(_points.translated(-topLeftOfVisibleArea)); // '_points' and '_rotatedRect' already rotated
	}
	else
		DrawWithEraser(painter, topLeftOfVisibleArea, clipR);
}

QDataStream& operator<<(QDataStream& ofs, const DrawableRectangle& di) // DrawableItem part already saved
{
	ofs << di.rect << di.isFilled;	// always save not rotated rectangle and rotation info
	return ofs;

}
QDataStream& operator>>(QDataStream& ifs, DrawableRectangle& di)		  // call AFTER header is read in
{
	ifs >> di.rect >> di.isFilled;
	di.rect.translate(0, DrawableItem::yOffset);
	// rectangle always stored as having axes parallel to the coord axes
	// but even when not we must set the _rotatedRectangle in it
	MyRotation arot = di.rot;
	di.rot = MyRotation();		// no rotation, clear
	QPointF center = di.rect.center();
	di.Rotate(arot, center);
	return ifs;
}

//=====================================
// DrawableScreenShot
//=====================================

void DrawableScreenShot::SetImage(QPixmap& animage)
{
	_image = animage;
	QSize size = _image.size();
	_rotatedArea = QRectF(refPoint - QPointF(size.width()/2.0, size.height()/2.0), size); // non rotated area this time
}

QRectF DrawableScreenShot::Area() const
{
	return _rotatedArea.boundingRect();
}

QRectF DrawableScreenShot::AreaOnCanvas(const QRectF& canvasRect) const
{
	return Area().intersected(canvasRect);
}

void DrawableScreenShot::Translate(QPointF dr, qreal minY)
{
	QRectF r = _rotatedArea.boundingRect();
	if (r.top() > minY)
	{
		DrawableItem::Translate(dr, minY);	// translates refPoint
		_rotatedArea.translate(dr);
	}
}

void DrawableScreenShot::Rotate(MyRotation arot, QPointF &center)
{
	_RotateErasers(arot, center);

	arot.RotatePoly(_rotatedArea, center, true);
	rot.AddRotation(arot);
	_rotatedImage = _image;
	rot.RotatePixmap(_rotatedImage, refPoint, center, true);
	if (rot.HasSimpleRotation())
	{
		if (abs(rot.angle) < eps)
		{
			rot.angle = 0.0;
			_rotatedImage = QPixmap();	// "delete"
		}
	}

}

void DrawableScreenShot::Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)
{
	if (drawStarted)
	{
		painter->setClipRect(clipR.translated(-topLeftOfVisibleArea));
		painter->drawPixmap(_rotatedArea.boundingRect().topLeft() - topLeftOfVisibleArea, Image());
	}
	else
		DrawWithEraser(painter, topLeftOfVisibleArea, clipR);
}


// ---------------------------------------------
QDataStream& operator<<(QDataStream& ofs, const DrawableScreenShot& bimg) // DrawableItem part already saved
{
	ofs << bimg.Image(true);	// original, non rotated image

	return ofs;
}

QDataStream& operator>>(QDataStream& ifs, DrawableScreenShot& bimg)	  // call AFTER header is read in
{
	QPixmap img;
	ifs >> img;  
	bimg.SetImage(img);

	MyRotation arot = bimg.rot;
	bimg.rot = MyRotation();
	QPointF center = bimg.Area().center();
	bimg.Rotate(arot, center);

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

	// DEBUG
//	bool eq =(EqZero(vpp.y() * vp.x() - vp.y() * vpp.x()) && ((!EqZero(vp.x() - vpp.x()) && vp.x() * vpp.x() > 0) || (!EqZero(vp.y() - vpp.y()) && vp.y() * vpp.y() > 0)));
	// end DEBUG

	// the two vectors point in the same direction when vpp.y()/vpp.x() == vp.y()/vp.x()
	// and both delta has the same sign. (turning back is not an extension)
	// i.e to avoid checking for zeros in divison: 
//	return (vpp.y() * vp.x() == vp.y() * vpp.x()) && ((vp.x() != vpp.x() && vp.x() * vpp.x() > 0) || (vp.y() != vpp.y() && vp.y() * vpp.y() > 0));
	return (EqZero(vpp.y() * vp.x() - vp.y() * vpp.x()) && ((!EqZero(vp.x() - vpp.x()) && vp.x() * vpp.x() > 0) || (!EqZero(vp.y() - vpp.y()) && vp.y() * vpp.y() > 0)) );
}

QPointF DrawableScribble::Add(QPointF p, bool smoothed, bool reset)	// only use smoothed = true when drawing by hand
{																	// neither when reading or copying
	static Smoother<QPointF, qreal, 200> smoother;					// Use reset when new drawing starts
	if (reset)
	{
		points.clear();
		if (p == QPoint(-1, -1))
			return p;
	}

	if (smoothed && bSmoothDebug)
		p = smoother.AddPoint(p, points);

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

void DrawableScribble::Rotate(MyRotation arot, QPointF & center)	// rotate around the center of 'center'
{
	_RotateErasers(arot, center);

	arot.RotatePoly(points, center, penWidth);
	rot.AddRotation(arot);

	//if (rot == flipNone)		// save rotation for undo
	//	rot = rot;
	//else
	//	rot = flipNone;		// undone: no rotation set yet
}
bool DrawableScribble::PointIsNear(QPointF p, qreal distance) const
{
	for (auto i = 0; i < points.size() - 1; ++i)
		if (__IsLineNearToPoint(points[i], points[i + 1], p, distance))
			return true;
	return false;
}
void DrawableScribble::Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)
{
	if (drawStarted)
	{
		SetPainterPenAndBrush(painter, clipR.translated(-topLeftOfVisibleArea), isFilled ? PenColor() : QColor());
		// draw normally using 'painter' and 'topLeftOfVisibleArea'
		QPolygonF pol = points.translated(-topLeftOfVisibleArea);
		if (isFilled)						// then originally this was an ellipse or rectangle that was rotated by not (90 degrees x n)
			painter->drawPolygon(points);
		else
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
	di.points.translate({ 0, DrawableItem::yOffset });
	di.rot = MyRotation();	// rotated points were stored

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

QPointF DrawableList::BottomRightLimit(QSize screenSize)
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
