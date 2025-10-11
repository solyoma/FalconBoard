#include <QMessageBox>
#include "drawables.h"
#include "history.h"
#include "smoother.h"

#if !defined _VIEWER && defined _DEBUG
	bool isDebugMode = false;
#endif

DrawablePen DrawablePen::_savedPen;

	// static member
qreal	DrawableItem::yOffset = 0.0;
int DrawableScribble::autoCorrectLimit = 6;	// in pixels, set from DrawArea

// *----------- helpers --------------
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
		// the 4 points are on the same line
		// If the points are ordered by their x or y coordinates
		// for intersection the order of points on the line must be 
		// p1 -> ip1 -> ip2 -> p2         
		if (EqZero(p2.y() - p1.y()))      // vertical line
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
// Draws an arrow at the end of a line from 'start' to 'end'
// - too short lines are ignored
// - if direction is 0 (out) the arrow looks like: ----->
// - if direction is 1 (in ) the arrow looks like: -----<
// - to draw an arrow to the other end of the line simply exchange start and end
// - The point of the arrow head is at 'end'
static void __DrawArrow(QPainter*painter, QPointF start, QPointF end, int direction, qreal arrowSize)
{
	QLineF line(end, start);
	if (line.length() < 2 * arrowSize)
		return; // too short to draw an arrow
	painter->save();
	painter->setBrush(painter->pen().color());
	painter->translate(end);
	painter->rotate(-line.angle() + direction*180);
	QPolygonF arrowHead;
	arrowHead << QPointF(0, 0)
		<< QPointF(-arrowSize, arrowSize / 2)
		<< QPointF(-arrowSize, -arrowSize / 2);
	painter->drawPolygon(arrowHead);
	painter->restore();
}

// ------------ Quad are helper functions ------------

QuadArea AreaForItem(const int& i)
{
	History* ph = historyList[-1];				// pointer to actual history
	assert(ph);
	DrawableItem* pdrwi = ph->Drawable(i);
	QRectF r = pdrwi->Area();
	return QuadArea(r.x(), r.y(), r.width(), r.height());
}

// *----------- helpers --------------
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


/* *********************** Zoomer ********************/

// static members and functions
constexpr const int Zoomer::maxZoomLevel = 10;
bool Zoomer::_initted = false;
QVector<qreal> Zoomer::zoomFactors;  // zoom in and out factors
const double Zoomer::dFactor = 1.05;  // zoom in step, zoom out is 1/dFactor

// functions

Zoomer::Zoomer()
{
	_Init();
}

Zoomer::Zoomer(const Zoomer& o) : _pOwnerItem(_pOwnerItem), _zoomParams(o._zoomParams), oRefPoint(o.oRefPoint)
{
	_Init();
}

Zoomer& Zoomer::operator=(const Zoomer& o)
{
	_Init();
	_pOwnerItem = o._pOwnerItem;
	_zoomParams = o._zoomParams;
	oRefPoint  = o.oRefPoint;
	return *this;
}

Zoomer::Zoomer(Zoomer&& o) noexcept
{
	_Init();
	*this = std::move(o);
}

Zoomer& Zoomer::operator=(Zoomer&& o) noexcept
{
	_pOwnerItem = o._pOwnerItem;
	_zoomParams = std::move(o._zoomParams);
	oRefPoint  = std::move(o.oRefPoint);
	return *this;
}

void Zoomer::_Init()
{
	if (!_initted)
	{
		zoomFactors.resize(2 * maxZoomLevel);


		zoomFactors[0] = dFactor;
		zoomFactors[maxZoomLevel] = 1.0 / dFactor;
		for (int i = 1; i < maxZoomLevel; ++i)
		{
			zoomFactors[i] = zoomFactors[i - 1] * dFactor;
			zoomFactors[maxZoomLevel + i] = 1.0 / zoomFactors[i];
		}
		_initted = true;
	}
}

/*
*  the 'zoomFactor' array holds the powers of the zoom coefficient 'dFactor'
*  zoomFactors[0] is the first zoomed state, items till .. (maxZoomLevel-1) are for zooming in, 
*  items maxZoomLevel .. (2*maxZoomLevel-1) are for zooming out
*  The zoom centers are stored in 'oRefPoints' and the original data in 'oPoints', 'oRect', 'oRData' and 'oPixmap'
*  The x coordinate of a point with the initial coordinate x0 zoomed n-times is
*    (using A[0] instead of 'dFactor' and A[n] for zoomFactors[n], the zoom factor and C[n] for oRefPoints[n].x() ):
*    x = x0 * A^n + (A^n-A)*C[n] + (A^(n-1)- A)*C[n-1] + ... + (A - A)*C[1] + C[0]
*/
inline qreal Zoomer::_ZoomFactor(int level) const
{ 
	return zoomFactors[(_zoomParams.zoomDir >= 0 ? level : level + maxZoomLevel)-1];
}

QRectF Zoomer::ZoomARect(const QRectF& r, const ZoomParams& params) const
{
	qreal fact = _ZoomFactor(params.level);
	return QRectF( fact*r.topLeft(), fact * r.bottomRight());
}

QPointF Zoomer::ZoomAPoint(const QPointF& p, const ZoomParams& params) const		  // to 'level'-th level
{
	qreal fact = _ZoomFactor(params.level);
	return fact * p;
}

void ZoomParams::Reset()
{
	level = 0;
	zoomDir = 0;
}
void Zoomer::SetOwner(DrawableItem* pOwner)
{
	_Init();
	_pOwnerItem = pOwner;
	_zoomParams.Reset();
}

void Zoomer::Setup()
{
	static bool wasSetup;
	if (!wasSetup)
	{
		_Init();
		_zoomParams.Reset();
		oRefPoint = _pOwnerItem->refPoint;
		wasSetup = true; 
	}
}

bool Zoomer::CanZoom(const ZoomParams& params) const
{
	DrawableDot ddot(*dynamic_cast<DrawableDot*>(_pOwnerItem));
	ddot.refPoint = ZoomAPoint(ddot.refPoint, params);
	if (ddot.CanRotate(ddot.rot,params.zoomCenter) )
		return false;
	return true;
}

void Zoomer::_PerformZoom()
{
	Setup();
	_pOwnerItem->refPoint = ZoomAPoint(oRefPoint, _zoomParams);
	++_zoomParams.level;
}

int Zoomer::Zoom()
{
	GetZoomParams(); 
	_PerformZoom();	 // increases level to the next zoom level
	return _zoomParams.level;
}

// subcalsses for Zoomer

// overwritten methods
//void PointZoomer::Setup()
//{
//	Zoomer::Setup();
//	DrawablePoint* pPoint = dynamic_cast<DrawablePoint*>(_pOwnerItem);
//	if (pPoint)
//		oRefPoint = pPoint->point;
//}

// ---------- crossZoomer----------------
void CrossZoomer::Setup()
{
	Zoomer::Setup();
	DrawableCross* pCross = dynamic_cast<DrawableCross*>(_pOwnerItem);
	if (pCross)
		_olength = pCross->length;
}

bool CrossZoomer::CanZoom(const ZoomParams& params) const
{
	DrawableCross* pCross = dynamic_cast<DrawableCross*>(_pOwnerItem);
	if (pCross)
	{
	}
	return true;
}

void CrossZoomer::_PerformZoom()
{
	Setup();
	Zoomer::_PerformZoom();	// reference point
	DrawableCross* pCross = dynamic_cast<DrawableCross*>(_pOwnerItem);
	if (pCross)
	{
		pCross->length = _olength * _ZoomFactor(_zoomParams.level);
		++_zoomParams.level;
	}
}

// ---------- EllipseZoomer----------------
void EllipseZoomer::Setup() 
{
	Zoomer::Setup();
	DrawableEllipse* pEllipse = dynamic_cast<DrawableEllipse*>(_pOwnerItem);
	if (pEllipse)
		pEllipse->rect = ZoomARect(_oRect, _zoomParams);
}

bool EllipseZoomer::CanZoom(const ZoomParams& params) const
{
	DrawableEllipse* pEllipse = dynamic_cast<DrawableEllipse*>(_pOwnerItem);
	if (pEllipse)
	{
	}
	return true;
}

void EllipseZoomer::_PerformZoom()
{
	Setup();
	DrawableEllipse* pEllipse = dynamic_cast<DrawableEllipse*>(_pOwnerItem);
	if (pEllipse)
	{
		pEllipse->rect = ZoomARect(_oRect, _zoomParams);
		++_zoomParams.level;
	}
}

// ---------- LineZoomer----------------
void LineZoomer::Setup()
{
	Zoomer::Setup();
	DrawableLine* pLine = dynamic_cast<DrawableLine*>(_pOwnerItem);
	if (pLine)
		_endPoint = pLine->endPoint;
}

bool LineZoomer::CanZoom(const ZoomParams& params) const
{
	DrawableLine* pLine = dynamic_cast<DrawableLine*>(_pOwnerItem);
	if (pLine)
	{

		if (pLine->refPoint.x() < 0 || pLine->refPoint.y() < 0)
			return false;
	}
	return true;
}

void LineZoomer::_PerformZoom()
{
	Setup();
	DrawableLine* pLine = dynamic_cast<DrawableLine*>(_pOwnerItem);
	if (pLine)
	{
		pLine->endPoint = ZoomAPoint(_endPoint, _zoomParams);
		++_zoomParams.level;
	}
}

// ---------- RectangleZoomer----------------
void RectangleZoomer::Setup()
{
	Zoomer::Setup();
	DrawableRectangle* pRectangle = dynamic_cast<DrawableRectangle*>(_pOwnerItem);
	if (pRectangle)
		pRectangle->rect = ZoomARect(_oRect, _zoomParams);
}

bool RectangleZoomer::CanZoom(const ZoomParams& params) const
{
	DrawableRectangle* pRectangle = dynamic_cast<DrawableRectangle*>(_pOwnerItem);
	if (pRectangle)
	{
	}
	return true;
}

void RectangleZoomer::_PerformZoom()
{
	Setup();
	DrawableRectangle* pRectangle = dynamic_cast<DrawableRectangle*>(_pOwnerItem);
	if (pRectangle)
	{
		pRectangle->rect = ZoomARect(_oRect, _zoomParams);
		++_zoomParams.level;
	}
}

// ---------- ScribbleZoomer----------------
void ScribbleZoomer::Setup()
{
	Zoomer::Setup();
	DrawableScribble* pScribble = dynamic_cast<DrawableScribble*>(_pOwnerItem);
	if (pScribble)
		_oPoints = pScribble->points;
}

bool ScribbleZoomer::CanZoom(const ZoomParams& params) const
{
	DrawableScribble* pScribble = dynamic_cast<DrawableScribble*>(_pOwnerItem);
	if (pScribble)
	{
	}
	return true;
}

void ScribbleZoomer::_PerformZoom()
{
	Zoomer::Setup();
	DrawableScribble* pScribble = dynamic_cast<DrawableScribble*>(_pOwnerItem);
	if (pScribble)
	{
		
		++_zoomParams.level;
	}
}

// ---------- ScreenshotZoomer----------------
void ScreenshotZoomer::Setup()
{
	Zoomer::Setup();
	DrawableScreenShot* pScreenshot = dynamic_cast<DrawableScreenShot*>(_pOwnerItem);
	if (pScreenshot)
	{

	}
}

bool ScreenshotZoomer::CanZoom(const ZoomParams& params) const
{
	DrawableScreenShot* pScreenshot = dynamic_cast<DrawableScreenShot*>(_pOwnerItem);
	if (pScreenshot)
	{
	}
	return true;
}

void ScreenshotZoomer::_PerformZoom()
{
	Setup();
	DrawableScreenShot* pScreenshot = dynamic_cast<DrawableScreenShot*>(_pOwnerItem);
	if (pScreenshot)
	{
		++_zoomParams.level;
	}
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

bool MyRotation::ResultsInNoRotation(MyRotation& arot, bool markIfNoRotation) const
{
	MyRotation tmpRot = *this;
	tmpRot.AddRotation(arot);
	return tmpRot.flipType == flipNone && !tmpRot.angle;
}

bool MyRotation::RotateRect(QRectF& rect, QPointF center, bool noCheck) const
{
	if (IsNull())
		return true;
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
		result = false;
	else
	{
		rect = r;
		result = true;
	}

	return result;
}

bool MyRotation::RotatePoly(QPolygonF& points, QPointF center, bool noCheck) const
{
	QPolygonF tmpPoly = points;
	tmpPoly.translate(-center);
	if (angle)
		tmpPoly = _tr.map(tmpPoly);

	if (flipType == rotFlipH)
	{
		for (auto& pt : tmpPoly)
			pt.setX(-pt.x());
	}
	else if (flipType == rotFlipV)
	{
		for (auto& pt : tmpPoly)
			pt.setY(-pt.y());
	}
	tmpPoly.translate(center);
	QRectF bndRect = tmpPoly.boundingRect();  
	// could rotate?
	if (!noCheck && (bndRect.left() < 0 || bndRect.top() < 0) )
		return false;

	points = tmpPoly;

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
		return false;

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
			return false;
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
		l.setP2(QPointF(-l.p2().x(), l.p2().y()));
	}
	else if (flipType == rotFlipV)
	{
		l.setP1(QPointF(l.p1().x(), -l.p1().y()));
		l.setP2(QPointF(l.p2().x(), -l.p2().y()));
	}
	l.translate(center);
	if (noCheck || (l.p1().x() >= 0 && l.p1().y() >= 0 && l.p2().x() >= 0 && l.p2().y() >= 0))
	{
		QPointF ptmp = l.p1();	// always the start point is at the left of the end point
		if (ptmp.x() > l.x2())
		{
			l.setP1(l.p2());
			l.setP2(ptmp);
		}
		line = l;
		return true;
	}
	else
		return false;
}

void MyRotation::_SetRot(qreal alpha)
{
	alpha = fmod(alpha, 360);   // set alpha into the (-360, 360] range
	//if (alpha > 180.0)
	//	alpha -= 180.0;
	if (EqZero(alpha))
		alpha = 0.0;
	angle = alpha;

	_tr = QTransform();
	_tr.rotate(alpha);
	angle = alpha;
}

QDataStream& operator<<(QDataStream& ofs, const MyRotation& mr)
{
	ofs << mr.angle << (std::byte)mr.flipType;
	return ofs;
}
QDataStream& operator>>(QDataStream& ifs, MyRotation& mr)
{
	std::byte ch;
	qreal alpha;
	ifs >> alpha >> ch;
	mr = alpha;		// sets transformation matrix too and fliptype to flipNone
	mr.flipType = (MyRotation::Type)ch;	// sets angle to 0.0

	return ifs;
}

QDataStream& operator<<(QDataStream& ofs, const DrawablePen& dp)
{
	ofs << dp.penWidth << dp.penAlpha << (std::byte)dp.Kind() 
		<< (std::byte)0;	// 0 means solid line
	return ofs;
}

QDataStream& operator>>(QDataStream& ifs, DrawablePen& dp)
{
	std::byte pk;
	ifs >> dp.penWidth;
	if (file_version_loaded >= 0x56030000)
		ifs >> dp.penAlpha >> dp.penStyle;
	ifs >> pk;
	dp.SetPenKind((FalconPenKind)pk);
	return ifs;
}

/* *********************** Drawable pen ********************/

void DrawablePen::SavePen()
{
	_savedPen = *this;
}

void DrawablePen::RestorePen()
{
	*this = _savedPen;
}

/* *********************** DrawableItem ********************/

void DrawableItem::SetPainterPenAndBrush(QPainter* painter, const QRectF& clipR, QColor brushColor, qreal alpha)
{
	globalDrawColors.SetActualPen(pen._penKind);
	if (clipR.isValid())
		painter->setClipRect(clipR);  // clipR must be DrawArea relative

#if !defined _VIEWER && defined _DEBUG
	if (pencilMode)
	{
		pen.SavePen();
		pen.penWidth = 1.0;
		pen.penAlpha = 1.0;
	}
#endif
	QPen mypen(QPen(pen.Color(), PenWidth(), Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));
	painter->setPen(mypen);
#if !defined _VIEWER && defined _DEBUG
	if (pencilMode)
		pen.RestorePen();
#endif
	if (brushColor.isValid())
	{
		if (alpha < 1.0)
			brushColor.setAlphaF(alpha);
		painter->setBrush(QBrush(brushColor));
	}
	else
		painter->setBrush(QBrush());
	// painter's default compositionmode is CompositionMode_SourceOver
	// painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
	painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
}

// -----------------------------------------------------------------
//									DrawableItem
// -----------------------------------------------------------------
bool DrawableItem::drawStarted;

DrawableItem& DrawableItem::operator=(const DrawableItem& other)
{
	dtType = other.dtType;
	pen = other.pen;
	refPoint = other.refPoint;
	zOrder = other.zOrder;
	rot = other.rot;
	isVisible = other.isVisible;
	erasers = other.erasers;
	zoomer = other.zoomer;		// also copies _pOwnerItem, but 
	zoomer.SetOwner(this);		// owner changes to this

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
		 prevInside = false;
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
	{
		if (erasers.isEmpty())
			andStoreHere->eraserStroke.clear();
		else
			*andStoreHere = erasers[erasers.size() - 1];
	}
	if (!erasers.isEmpty())
		erasers.pop_back();
}


bool DrawableItem::Translate(QPointF dr, qreal minY)	// single point. Only translate if below minY
{
	if (refPoint.y() > minY)
	{
		refPoint += dr;
		if (refPoint.x() < 0 || refPoint.y() < 0)
		{
			refPoint -= dr;
			return false;
		}
		for (auto &e : erasers)
			e.eraserStroke.translate(dr);
	}
	return true;
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

bool DrawableItem::Rotate(MyRotation arot, QPointF center)
{
	MyRotation tmpr = rot;
	tmpr.AddRotation(arot);
	bool res = tmpr.RotateSinglePoint(refPoint, center, PenWidth());	// if res false 'refPoint' did not change
	if (res)
		rot = tmpr;
	return true;
}

// drawables MUST be saved in increasing zOrder, so no need to save the zOrder
// this way all screenshots are saved first, followed by all other drawables

// before V3.0.0 the order was: drawable type, ref. point, pen index, pen width, ref. point  then rotation
// since V3.0.0 drawable type, pen ref. point rotation

QDataStream& operator<<(QDataStream& ofs, const DrawableItem& di)
{
	ofs << (int)di.dtType << di.pen << di.refPoint << di.rot;
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
	ifs >> n; // should be DrawableType
	if ((n > (int)DrawableType::dtDrawableTop && n < (int)DrawableType::dtNonDrawableStart) ||
		n > (int)DrawableType::dtNonDrawableEnd)
		throw("Bad File");
	// special handling for data of not really drawables e.g. pen color data
	di.dtType = (DrawableType)n;
	if (n >= (int)DrawableType::dtNonDrawableStart)
		return ifs;

	if (file_version_loaded >= 0x56030000l)
	{
		ifs >> di.pen;
		ifs >> di.refPoint;
		di.refPoint += {0, DrawableItem::yOffset};
	}
	else
	{
		ifs >> di.refPoint;
		di.refPoint += {0, DrawableItem::yOffset};
		ifs >> n;
		di.pen.SetPenKind((FalconPenKind)n);
		ifs /*>> di.penColor*/ >> di.pen.penWidth >> di.rot;
	}
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
	// therefore neither can the zoom data be set up
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
	zoomer.Setup();
	_Setup();
}

void DrawableCross::_Setup()
{
	qreal dlen = length / sqrt(2);
	_ltrb = QLine(refPoint.x() - dlen, refPoint.y() - dlen, refPoint.x() + dlen, refPoint.y() + dlen);
	_lbrt = QLine(refPoint.x() - dlen, refPoint.y() + dlen, refPoint.x() + dlen, refPoint.y() - dlen);
	zoomer.Setup();
}

QRectF DrawableCross::Area() const    // includes half of pen width+1 pixel
{
	//qreal w = penWidth / 2.0;
	qreal w = PenWidth() / 2.0,
		x1 = std::min(_ltrb.x1(), _ltrb.x2()),
		y1 = std::min(_ltrb.y1(), _ltrb.y2()),
		x2 = std::max(_ltrb.x1(), _ltrb.x2()),
		y2 = std::max(_ltrb.y1(), _ltrb.y2());
		 

		x1 = std::min(x1, std::min(_lbrt.x1(), _lbrt.x2()));
		x2 = std::max(x2, std::max(_lbrt.x1(), _lbrt.x2()));
		y1 = std::min(y1, std::min(_lbrt.y1(), _lbrt.y2()));
		y2 = std::max(y2, std::max(_lbrt.y1(), _lbrt.y2()));
	// 
	// lines are always stored so that x1 <= x2
	//		but no guarantee that y2 > y1!
	//qreal x1 = std::min(_ltrb.x1(), _lbrt.x1()),
	//	y1 = std::min(_ltrb.y1(), _lbrt.y1()),
	//	x2 = std::max(_ltrb.x2(), _lbrt.x2()),
	//	y2 = std::max(_ltrb.y2(), _lbrt.y2()),
	//	w = penWidth/2.0;
	QRectF rect = QRectF(QPointF(x1,y1),QPointF(x2,y2)).adjusted(-w,-w,w,w);
	return rect;
}

bool DrawableCross::CanTranslate(const QPointF dr) const
{
	QRectF a = Area().translated(dr);
	return a.left() >=0 &&  a.top() >= 0;
}

bool DrawableCross::Translate(QPointF dr, qreal minY)	
{
	QRectF a = Area();
	if (a.top() >= minY)			// if even the smallest y is below minY
	{
		if(CanTranslate(dr))
		{
			refPoint += dr;
			_ltrb.translate(dr);
			_lbrt.translate(dr);
			return true;
		}
	}
	return false;
}

bool DrawableCross::CanRotate(MyRotation rot, QPointF center)  const
{
	if (rot.IsFlip() || rot.IsNull())
		return true;

	QRectF a = Area();
	return rot.RotateRect(a,center,false);
}

bool DrawableCross::Rotate(MyRotation arot, QPointF center)
{
	if(CanRotate(arot, center))
	{
		arot.RotateSinglePoint(refPoint, center, true);
		arot.RotateLine(_ltrb, center);
		arot.RotateLine(_lbrt, center);
		_RotateErasers(arot, center);
		rot.AddRotation(arot);
		return true;
	}
	return false;
}

void DrawableCross::Zoom(bool zoomIn, QPointF center, int steps)
{
	QPolygonF p;
	p << QPointF(length, 0);
	DrawableItem::Zoom(zoomIn, center, steps);
	length = p[0].x();
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
		rect(rect), isFilled(isFilled), DrawableItem(DrawableType::dtEllipse, rect.center(), zOrder, penKind, penWidth), _rotatedRect(rect) 
{
	zoomer.Setup();
}
DrawableEllipse::DrawableEllipse(QPointF refPoint, qreal radius, int zOrder, FalconPenKind penKind, qreal penWidth, bool isFilled): 
		rect(QPointF(refPoint.x() - radius, refPoint.y() - radius), QSize(2*radius, 2*radius)), isFilled(isFilled), 
		DrawableItem(DrawableType::dtEllipse, refPoint, zOrder, penKind, penWidth), _rotatedRect(rect) 
{
	zoomer.Setup();
}
DrawableEllipse::DrawableEllipse(QPointF refPoint, qreal a, qreal b, int zOrder, FalconPenKind penKind, qreal penWidth, bool isFilled): 
		rect(QPointF(refPoint.x() - a, refPoint.y() -b), QSize(2*a, 2*b)), isFilled(isFilled), 
		DrawableItem(DrawableType::dtEllipse, refPoint, zOrder, penKind, penWidth), _rotatedRect(rect) 
{
	zoomer.Setup();
}


DrawableEllipse::DrawableEllipse(const DrawableEllipse& o) : DrawableItem(o) { *this = o; }
DrawableEllipse::DrawableEllipse(DrawableEllipse&& o) noexcept :DrawableItem(o) { *this = o; }
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

bool DrawableEllipse::CanTranslate(const QPointF dr) const
{
	QRectF r = _rotatedRect.translated(dr);
	return r.left() >= 0 && r.top() >= 0;
}

bool DrawableEllipse::Translate(QPointF dr, qreal minY)
{
	if (_rotatedRect.top() > minY)
	{
		DrawableItem::Translate(dr, minY);
		rect.moveTo(rect.topLeft() + dr);
		_rotatedRect.moveTo(_rotatedRect.topLeft() + dr);
		if (!_points.isEmpty())
			_points.translate(dr);
	}
	return false;
}

bool DrawableEllipse::CanRotate(MyRotation arot, QPointF centerOfRotation) const
{
	if (rot.IsFlip() || rot.IsNull())	// flips are always good
		return true;

	MyRotation tmpr = rot;		// previous rotation
	tmpr.AddRotation(arot);		// check if rotation is possible before doing any changes

	if (IsCircle())
	{
		QPointF rp = refPoint;	// center of circle
		qreal r = rect.width() / 2.0;
		if (!tmpr.RotateSinglePoint(rp, centerOfRotation))
			return false;
		rp = rp - QPointF(r, r);
		if (rp.x() < 0 || rp.y() < 0)
			return false;
		return true;
	}
	else
	{
		QRectF r = rect;			// original rectangle. Modified if rotated by any multiple of 90

		return tmpr.RotateRect(r, centerOfRotation, true);
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
bool DrawableEllipse::Rotate(MyRotation arot, QPointF centerOfRotation)
{
	if (arot.IsNull())
	{
		_rotatedRect = rect;
		return true;
	}

	MyRotation tmpr = rot;		// previous rotation

	tmpr.AddRotation(arot);		// check if rotation is possible before doing any changes
	if (IsCircle())				// only move center
	{
		QPointF rp = refPoint;	// center of circle
		qreal r = rect.width() / 2.0;
		if (!tmpr.RotateSinglePoint(rp, centerOfRotation))
			return false;
		refPoint = rp;
		rp = rp - QPointF(r, r);   // left top
		if (rp.x() < 0 || rp.y() < 0)
			return false;
		_rotatedRect = rect = QRectF(rp, QSize(2 * r, 2 * r));
		return true;
	}

	QRectF r = rect;			// original rectangle. May have been modified if rotated by any multiple of 90
	if (!tmpr.RotateRect(r, centerOfRotation))
		return false;

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
		tmpr.RotatePoly(_points, centerOfRotation, true);
		_rotatedRect = _points.boundingRect();
	}
	refPoint = _rotatedRect.center();
	_RotateErasers(arot, centerOfRotation);	// erasers are stored rotated so only apply last rotation to them
	return true;
}

void DrawableEllipse::Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)
{
	if (drawStarted)
	{
		QColor c;
		if (isFilled)
			c = pen.Color();
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
	QPointF dp = di.refPoint - di.rect.center();
	ofs << di.rect.translated(dp) << di.isFilled;	// always save not rotated rectangle and rotation info

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
	di.Rotate(arot, di.refPoint);

	return ifs;
}
//=====================================
// DrawableLine
//=====================================

DrawableLine::DrawableLine(QPointF refPoint, QPointF endPoint, int zorder, FalconPenKind penKind, qreal penWidth) : 
	endPoint(endPoint), DrawableItem(DrawableType::dtLine, refPoint, zorder, penKind, penWidth)
{
	zoomer.Setup();
}

DrawableLine::DrawableLine(const DrawableLine& ol) :DrawableItem(ol)
{
	*this = ol;
}

DrawableLine::DrawableLine(DrawableLine&& ol) noexcept:DrawableItem(ol)
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

bool DrawableLine::CanTranslate(const QPointF dr) const
{
	QPointF p1 = refPoint + dr, p2 = endPoint + dr;
	return p1.x() >=0 && p1.y() >= 0 && p2.x() >=0 && p2.y() >= 0;
}
bool DrawableLine::Translate(QPointF dr, qreal minY)

{
	if (refPoint.y() > minY)
	{
		if (DrawableItem::Translate(dr, minY))
		{
			endPoint += dr;
			return true;
		}
		return false;
	}
	return true;
}

bool DrawableLine::CanRotate(MyRotation arot, QPointF center) const
{
	if (rot.IsFlip() || rot.IsNull())
		return true;

	arot = arot.AddRotation(rot);
	QLineF line = QLineF(refPoint, endPoint);
	if (arot.RotateLine(line, center, false))
		return true;
	return false;
}

bool DrawableLine::Rotate(MyRotation arot, QPointF center)
{
	QLineF line(refPoint, endPoint);
	if (!arot.RotateLine(line, center, true))
		return false;

	refPoint = line.p1();
	endPoint = line.p2();

	_RotateErasers(arot, center);
	rot.AddRotation(arot);
	return true;
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
		_DrawArrows(painter);
	}
	else
		DrawWithEraser(painter, topLeftOfVisibleArea, clipR);
}

void DrawableLine::_DrawArrows(QPainter* painter)
{
	if(arrowFlags == 0)
		return;

	if(arrowFlags.testFlag(arrowStartOut))
		__DrawArrow(painter, endPoint, refPoint, 0, pen.penWidth*10);
	else if(arrowFlags.testFlag(arrowStartIn))
		__DrawArrow(painter, endPoint, refPoint, 1, pen.penWidth*10);
	else if(arrowFlags.testFlag(arrowEndOut))
		__DrawArrow(painter, refPoint, endPoint, 0, pen.penWidth*10);
	else if(arrowFlags.testFlag(arrowEndIn))
		__DrawArrow(painter, refPoint, endPoint, 1, pen.penWidth*10);
}

QDataStream& operator<<(QDataStream& ofs, const DrawableLine& di) // DrawableItem part already saved
{
	ofs << di.endPoint << di.arrowFlags;
	return ofs;
}

QDataStream& operator>>(QDataStream& ifs, DrawableLine& di)		  // call AFTER header is read in
{
	ifs >> di.endPoint;
	di.endPoint += {0, DrawableItem::yOffset};

	if (file_version_loaded >= 0x56030000)
		ifs >> di.arrowFlags;

	di.rot = MyRotation();	// endpoints were rotated before save
	return ifs;
}


//=====================================
// DrawableRectangle
//=====================================
DrawableRectangle::DrawableRectangle(QRectF rect, int zOrder, FalconPenKind penKind, qreal penWidth, bool isFilled) : 
	rect(rect), _rotatedRect(rect), isFilled(isFilled), DrawableItem(DrawableType::dtRectangle, rect.center(), zOrder, penKind, penWidth) 
{
	zoomer.Setup();
}
DrawableRectangle::DrawableRectangle(const DrawableRectangle& di) :DrawableItem(di) { *this = di; }
DrawableRectangle& DrawableRectangle::operator=(const DrawableRectangle& di)
{
	*(DrawableItem*)this = (DrawableItem&)di;

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

bool DrawableRectangle::CanTranslate(const QPointF dr) const
{
	QRectF r = _rotatedRect.translated(dr);
	return r.left() >= 0 && r.top() >= 0;
}

bool DrawableRectangle::Translate(QPointF dr, qreal minY)             // only if not deleted and top is > minY
{
	if (_rotatedRect.top() > minY)
	{
		DrawableItem::Translate(dr, minY);
		_rotatedRect.moveTo(_rotatedRect.topLeft() + dr);
		rect.moveTo(rect.topLeft() + dr);
		if (!_points.isEmpty())
			_points.translate(dr);
		return true;
	}
	return false;
}

bool DrawableRectangle::CanRotate(MyRotation arot, QPointF center) const
{
	if (arot.IsFlip() || arot.IsNull())
		return true;

	MyRotation tmpr = rot;		// previous rotation
	QRectF r = rect;			// original rectangle. Modified if rotated by any multiple of 90

	tmpr.AddRotation(arot);		// check if rotation is possible before doing any changes
	return tmpr.RotateRect(r, center, false);
}

/*=============================================================
 * TASK:	rotates this rectangle in an enclosing rectangle
 * PARAMS:	arot - rotation Do not use 'MyRotation::rotAngle' here
 *			center - rotate around the center of this
 *			rectangle
 *			alpha - must not be used
 * GLOBALS:
 * RETURNS: nothing, ellipse is rotated by a multiple of 90 degrees
 * REMARKS: - to rotate an ellipse by an angle 'alpha'
 *				create a new DrawableScribble from this and rotate that
 *------------------------------------------------------------*/
bool DrawableRectangle::Rotate(MyRotation arot, QPointF center)
{
	if (arot.IsNull())
	{
		_rotatedRect = rect;
		return true;
	}

	MyRotation tmpr = rot;		// previous rotation
	QRectF r = rect;			// original rectangle. Modified if rotated by any multiple of 90

	tmpr.AddRotation(arot);		// check if rotation is possible before doing any changes
	if (!tmpr.RotateRect(r, center))
		return false;

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
	refPoint = _rotatedRect.center();
	// DEBUG
	//qDebug("refPoint: (%g,%g)", refPoint.x(), refPoint.y());
	// /DEBUG
	_RotateErasers(arot, center);	// erasers are stored rotated so only apply last rotation to them
	return true;
}


void DrawableRectangle::Draw(QPainter* painter, QPointF topLeftOfVisibleArea, const QRectF& clipR)    // example: you must override this using this same structure
{
	if (drawStarted)
	{
		QColor c;
		if (isFilled)
			c = pen.Color();
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
	QPointF dp = di.refPoint - di.rect.center();
	ofs << di.rect.translated(dp) << di.isFilled;	// always save not rotated rectangle and rotation info

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

bool DrawableScreenShot::CanTranslate(const QPointF dr) const
{
	QRectF r = _rotatedArea.boundingRect().translated(dr);
	return r.left() >= 0 && r.top() >= 0;
}

bool DrawableScreenShot::Translate(QPointF dr, qreal minY)
{
	QRectF r = _rotatedArea.boundingRect();
	if (r.top() > minY)
	{
		
		DrawableItem::Translate(dr, minY);	// translates refPoint
		_rotatedArea.translate(dr);
	}
	return true;
}

bool DrawableScreenShot::CanRotate(MyRotation arot, QPointF center) const
{
	if (rot.IsFlip() || rot.IsNull())
		return true;

	MyRotation tmpr = rot;		// previous rotation

	QRectF r = QRectF(refPoint - QPointF(_image.size().width() / 2, _image.size().height() / 2), _image.size());

	tmpr.AddRotation(arot);		// check if rotation is possible before doing any changes
	if (!tmpr.RotateRect(r, center, false))
		return false;

	return true;
}

bool DrawableScreenShot::Rotate(MyRotation arot, QPointF center)
{
	_RotateErasers(arot, center);

	if(!arot.RotatePoly(_rotatedArea, center, true))
		return false;

	rot.AddRotation(arot);
	_rotatedImage = _image;
	rot.RotatePixmap(_rotatedImage, refPoint, center, true);
	if (rot.HasSimpleRotation())
	{
		if (!rot.IsFlip() && EqZero(rot.angle))
		{
			rot.angle = 0.0;
			_rotatedImage = QPixmap();	// "delete"
		}
	}
	return true;
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

DrawableScribble::DrawableScribble(FalconPenKind penKind, qreal penWidth, int zorder) noexcept : DrawableItem(DrawableType::dtScribble, points.boundingRect().topLeft(), zorder, penKind, penWidth) 
{
	zoomer.Setup();
}
DrawableScribble::DrawableScribble(const DrawableScribble& di):DrawableItem(di) { *this = di; }
DrawableScribble::DrawableScribble(DrawableScribble&& di) noexcept :DrawableItem(di) { *this = di; }
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

/*=============================================================
 * TASK   : determines if the points are almost or exactly on 
 *			a straight line between the start point and the point
 *			furthest from it
 * PARAMS :
 * EXPECTS:
 * GLOBALS:
 * RETURNS: true: points are on almost a straight line and the line
 *					is set into 'lin'
 *			false: not a straight line, lin isn't touched
 * REMARKS: - if this is a line it must go through two opposite
 *			corner points of the bounding rectangle of the points.
 *			- if the last point of the scribble is nearer to the 
 *			first point than half of the length of the greater side
 *			of the bounding rectangle of the points then this isn't
 *			a line. Otherwise
 *			- calculate the distances of the points of the scribble
 *			from this line. If the maximum distance is greater than
 *			either 3 times the pen width or the 1/10th of the 
 *			length of the line(?) this is not a line
 *------------------------------------------------------------*/
bool DrawableScribble::IsAlmostAStraightLine(DrawableLine& lin)
{
	if (points.size() < 3)
		return false;

	QRectF brect = points.boundingRect();	// no pen width used here
	// If the reference point is not in _points then this isn't a 
	// straight line It may be a circle or a rectangle though
	if (points.indexOf(refPoint) < 0)
		return false;
	// start point has the smallest, end point the largest x coordinate
	// taking in account the y coordinates too
	qreal xmin = 999999.0, xmax = 0,	 // value of x minimum and maximum coordinate
		ymin = 999999.0, ymax = 0;		 // value of y minimum and maximum coordinate
	int mix = -1, mxx = -1, miy=-1, mxy=-1;				 // index of these value in 'points[]'
	for (int i = 0; i < points.size(); ++i)
	{
		if (points[i].x() < xmin)
		{
			mix = i;
			xmin = points[i].x();
		}
		if (points[i].x() > xmax)
		{
			mxx = i;
			xmax = points[i].x();
		}
		if (points[i].y() < ymin)
		{
			miy = i;
			ymin = points[i].y();
		}
		if (points[i].y() > ymax)
		{
			mxy = i;
			ymax = points[i].y();
		}
	}
	if(xmax-xmin < ymax-ymin)	// if the x range is smaller than the y range
	{	// then we have a vertical line, so use ymin and ymax
		mix = miy;
		mxx = mxy;
	}

	QPointF pfStart, pfEnd; // start and end points
	pfStart = points[mix];
	pfEnd = points[mxx];

	//qDebug("brect: (%d,%d),(%d,%d)", (int)brect.topLeft().x(), (int)brect.topLeft().y(), (int)brect.bottomRight().x(), (int)brect.bottomRight().y());
	// now we have a line going through the start and end points of our scrible
	// the equation for the distance between points 
	//		|(y2-y1) x[n] - (x2- x1) y[n] + x2*y1-y2*x1|
	// d = ---------------------------------------------
	//		length of line betweenR2 and r1
	qreal r = std::sqrt((pfEnd.y() - pfStart.y()) * (pfEnd.y() - pfStart.y()) + 
						(pfEnd.x() - pfStart.x()) * (pfEnd.x() - pfStart.x())),		// length of line between start and finish
		  dly = pfEnd.y() - pfStart.y(),											// delta y = y2-y1
		  dlx = pfEnd.x() - pfStart.x(),											// delta x = x2-x1
		  dd  = pfEnd.x() * pfStart.y() - pfEnd.y() * pfStart.x();					// = x2 y1 - y2 x1

	auto dist = [&](const QPointF& pt)	   // disance from straight line
		{
			qreal d = std::abs(dly * pt.x() - dlx * pt.y() + dd);
			return d/r;
		};
	// get the point distances from this line and determine the maximum one
	qreal maxd = 0;
	for (auto &pt:points)	// could be sped up by leaving intermediate points out as neighboring points
	{						// must not differ by too large a distance
		qreal dst = dist(pt);
		if (maxd < dst)
			maxd = dst;
	}
	//constexpr const qreal pwm = 2.0,			// pen width limit factor
	//	                  lendiv = 1 / 10.0;	// length limit factor
	// if (r != 0.0 && (/*(r > pwm * penWidth && maxd <= r * lendiv) ||*/ maxd <= pwm * penWidth))
	if (maxd != 0.0 &&  maxd <= autoCorrectLimit)
	{	// set up 'the 'lin'
		lin = DrawableLine(pfStart, pfEnd, zOrder, PenKind(), PenWidth());
//		qDebug("TRUE: maxd: %g, start:(%d,%d), end:(%d,%d), len/: %g,  2 x penWidth: %d", maxd, (int)pfStart.x(), (int)pfStart.y(), (int)pfEnd.x(), (int)pfEnd.y(), (r*lendiv), pwm * (int)penWidth);
		return true;
	}
//		qDebug("FALSE: maxd: %g, start:(%d,%d), end:(%d,%d), len/: %g,  2 x penWidth: %d", maxd, (int)pfStart.x(), (int)pfStart.y(), (int)pfEnd.x(), (int)pfEnd.y(), (r*lendiv), pwm * (int)penWidth);
	return false;
}

/*=============================================================
 * TASK   :	is it a closed or self intersecting curve or an almost 
 *			closed one?
 * PARAMS :
 * EXPECTS:
 * GLOBALS:
 * RETURNS: true: yes they are and a circle (ellipse) that fits into 
 *					a square with sides of length of the longer side of
 *					the enclosing rectangle is returned in 'circ'
 *			false: not a circle, circ isn't touched
 * REMARKS:
 *------------------------------------------------------------*/
bool DrawableScribble::IsAlmostACircle(DrawableEllipse& circ)
{
	return false;
}

/*=============================================================
 * TASK   : is this scribble at least approximately a rectangle?
 * PARAMS :
 * EXPECTS:
 * GLOBALS:
 * RETURNS:	true: yes and the rectangle is returned in 'rect'
 *			false: not a rectangle, rect isn't touched
 * REMARKS: - the scribble is considered a rectangle if it has
 *				4 almost straight and pairwise almost parallel sides
 *				and it is closed or almost closed
 *			- only this single scribble is used. if you have 4 
 *				separate, almost straight scribble, which may visually
 *				form a rectangle this function returns false for each
 *				of them.
 *			- rect may be a rotated rectangle, in which case its
 *				rotation is also stored
 *			- if the smallest angle between the sides and the
 *				horizontal is smaller than 5 degrees then the
 *				rectangle is not rotated. Otherwise
 *------------------------------------------------------------*/
bool DrawableScribble::IsAlmostARectangle(DrawableRectangle& rect)
{
	return false;
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

bool DrawableScribble::CanTranslate(const QPointF dr) const
{
	QRectF r = Area().translated(dr);
	return r.left() >= 0 && r.top() >= 0;
}

bool DrawableScribble::Translate(QPointF dr, qreal  minY)
{
	if (Area().y() < minY || !isVisible)
		return false;
	QRectF r = points.boundingRect().translated(dr);
	bool res =  r.left() >= 0 && r.top() >= 0;
	if (res)
	{
		DrawableItem::Translate(dr, minY);
		points.translate(dr);
	}
	return res;
}

bool DrawableScribble::CanRotate(MyRotation arot, QPointF center) const
{
	if (rot.IsFlip() || rot.IsNull())
		return true;

	MyRotation tmpr = rot;		// previous rotation
	QRectF r = Area();			// original rectangle. Modified if rotated by any multiple of 90

	tmpr.AddRotation(arot);		// check if rotation is possible before doing any changes
	if (!tmpr.RotateRect(r, center, false))
		return false;

	return true;
}

bool DrawableScribble::Rotate(MyRotation arot, QPointF center)	// rotate around the center of 'center'
{
	if(!arot.RotatePoly(points, center, PenWidth()))
		return false;
	_RotateErasers(arot, center);

	rot.AddRotation(arot);
	return true;
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
		SetPainterPenAndBrush(painter, clipR.translated(-topLeftOfVisibleArea), isFilled ? pen.Color() : QColor());
		// draw normally using 'painter' and 'topLeftOfVisibleArea'
		// DEBUG	@
		/*if (points[0] == points[1])
		{
			points.clear();
			points.push_back(QPoint(698, 406));
			points.push_back(QPoint(651, 406));
			points.push_back(QPoint(652, 406));
		}
		*/// /DEBUG

		//painter->translate(-topLeftOfVisibleArea);
		if (isFilled)						// then originally this was an ellipse or rectangle that was rotated by not (90 degrees x n)
			painter->drawPolygon(points.translated(-topLeftOfVisibleArea));
		else
		{
			QVector<QLineF> lfv(points.size());
			QPolygonF ptr = points.translated(-topLeftOfVisibleArea);
			for (int i = 0; i < ptr.size()-1; ++i)
				lfv[i] = (QLineF(ptr[i], ptr[i + 1]));
			
			painter->drawLines(lfv);
		}
		//else
		//	painter->drawPolyline(points.translated(-topLeftOfVisibleArea));
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



//****************************************************************************************
// DrawableList
//****************************************************************************************
DrawableList::DrawableList(const DrawableList& other)
{
	*this = other;
}

DrawableList::DrawableList(DrawableList&& other) noexcept
{
	*this = other;
}

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

DrawableList& DrawableList::operator=(const  DrawableList& other)
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
DrawableList& DrawableList::operator=(DrawableList&& other)   noexcept
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

DrawableList& DrawableList::SetZorderStore(ZorderStore* pzorders)
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
DrawableList& DrawableList::SetQuadTreeDelegate(QuadTreeDelegate* pqtree)
{
	_pQTree = pqtree;
	return *this;
}

DrawableList::~DrawableList()              // pQTree is deleted elsewhere!
{
	Clear();
	_pQTree = nullptr;
}

void DrawableList::ResetZorder()
{
	_pZorderStore->Reset();
}

void DrawableList::Clear(bool andDeleteQuadTree)
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
	if (_pZorderStore)
		ResetZorder();
}
// these two used only when a new drawable is just added or the top one is remeoved
void DrawableList::Undo()    // moves last item to _redoItems w.o. deleting them
{                       // and removes them from the quad tree
	int i = _items.size() - 1;
	if (_pQTree)
		_pQTree->Remove(i);
	_redoItems.push_back(_items.at(i));
	_items.pop_back();
}

void DrawableList::Redo()    // moves last item from _redoItems 
{                       // and adds them from the quad tree
	int i = _items.size(), k = _redoItems.size() - 1;
	_items.push_back(_redoItems.at(k));
	_redoItems.pop_back();
	if (_pQTree)
		_pQTree->Add(i);
}

void DrawableList::ClearRedo()
{
	for (auto a : _redoItems)
		delete a;
	_redoItems.clear();
}
/*
	void Pop_back_N(int n)    // Deletes last n items. Used only when history's undo list is discarded
	{
		for (int i = _items.size() - 1; i >= 0 && n--; --i)
		{
			SetVisibility(i, false);    // removes them from QuadTree if they were there
			delete _items[i];
			_items.pop_back();
		}
	}
 */
int DrawableList::Count(const QuadArea& area)  // count of visible items
{
	return _pQTree->Count(area);
}

int DrawableList::CountOfScreenShots()
{
	int cnt = 0;
	for (auto p : _items)
		if (p->dtType == DrawableType::dtScreenShot && p->isVisible)
			++cnt;
	return cnt;
}

DrawableItem* DrawableList::FirstVisibleDrawable(QRectF& r)  // smallest in zOrder
{
	_indicesInRect = ListOfItemIndicesInRect(r);

	_runningIndex = -1; //  _itemIndices.begin();
	return NextVisibleDrawable();
}

DrawableItem* DrawableList::NextVisibleDrawable()  // smallest in zOrder
{
	DrawableItem* pdh;

	for (++_runningIndex; _runningIndex < _indicesInRect.size(); ++_runningIndex)
	{
		pdh = (*this)[_indicesInRect[_runningIndex]];
		if (pdh->isVisible)
			return pdh;
	}
	return nullptr;
}
DrawableItem* DrawableList::operator[](const DrawableItemIndex& drix) const
{
	return _items.operator[](drix.index);
}
// must redefine this 
DrawableItem* DrawableList::operator[](int ix) const
{
	Q_ASSERT_X(ix >= 0 && ix < _items.size(), "DrawableItem::operator[]", "index out of range");
	return  _items[ix];
}

void DrawableList::SetVisibility(DrawableItemIndex drix, bool visible)
{
	SetVisibility(drix.index, visible);
}
void DrawableList::SetVisibility(int index, bool visible)
{                     // _pQTree must not be null
	if (_items[index]->isVisible != visible)    // when it does change
		_items[index]->isVisible = visible;
	if (visible)
		_pQTree->Add(index);
	else
		_pQTree->Remove(index);
}


//    IndexVectorIterator Push_back(DrawableItem* pdrwi)
int DrawableList::Push_back(DrawableItem* pdrwi)
{
	int ix = _items.size();  // index of items to add
	if (pdrwi->zOrder < 0)
		pdrwi->zOrder = _pZorderStore->GetZorder(pdrwi->dtType == DrawableType::dtScreenShot);
	_items.push_back(pdrwi);
	// return _itemIndices.Insert(ix, pdrwi);
	return ix;
}

int DrawableList::CopyDrawable(DrawableItem& dri, QPointF topLeft) // adds copy of existing object on the heap
{
	if (_pZorderStore) // then the copy's zOrder must be increased'
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
int DrawableList::AddDrawable(DrawableItem* pdrh) // add original, 'pdrh' points to complete object (subclass) which MUST be created on the heap!
{
	int ix = _items.size();  // index of items to add
	if (pdrh->zOrder < 0 && _pZorderStore)
		pdrh->zOrder = _pZorderStore->GetZorder(pdrh->dtType == DrawableType::dtScreenShot);
	_items.push_back(pdrh);
	if (_pQTree)
		_pQTree->Add(ix);

	_prevLargestXY = _largestXY; // save previous largest XY
	QPointF r = pdrh->Area().bottomRight();
	if(r.x() >_largestXY.x())
		_largestXY.setX(r.x());
	if(r.y() >_largestXY.y())
		_largestXY.setY(r.y());

	return ix;
}

void DrawableList::RemoveDrawable(int which)
{
	if (_pQTree)
		_pQTree->Remove(which);                 // uses _pItems[which]

	_items.erase(_items.begin() + which);   // remove after not in use

}
QPointF DrawableList::CalcLargestXY()
{
	_prevLargestXY = _largestXY; // save previous largest XY
	_largestXY = QPointF();
	for (auto& item : _items)
	{
		if (item->IsVisible())
		{
			int x = item->Area().bottom();
			if (x > _largestXY.x())
				_largestXY.setX(x);
			x = item->Area().right();
			if (x > _largestXY.y())
				_largestXY.setY(x);
		}
	}
	return _largestXY;
}

// create new object of given parameters on the heap
									// pimage must be set for dtScreenShot
int DrawableList::AddDrawable(DrawableType dType, QPixmap* pimage, FalconPenKind penKind, qreal penWidth, QPointF topLeft, QSizeF sizef, bool isFilled)
{
	int ivi = -1;    // _pZorderStore must exist

	switch (dType)
	{
		case DrawableType::dtDot:       ivi = Push_back(new DrawableDot(topLeft, _pZorderStore->GetZorder(false), penKind, penWidth)); break;
		case DrawableType::dtCross:     ivi = Push_back(new DrawableCross(topLeft, sizef.width(), _pZorderStore->GetZorder(false), penKind, penWidth)); break;
		case DrawableType::dtEllipse:   ivi = Push_back(new DrawableEllipse(QRectF(topLeft, sizef), _pZorderStore->GetZorder(false), penKind, penWidth, isFilled)); break;
		case DrawableType::dtLine:      ivi = Push_back(new DrawableLine(topLeft, QPointF(sizef.width(), sizef.height()), _pZorderStore->GetZorder(false), penKind, penWidth)); break;
		case DrawableType::dtRectangle: ivi = Push_back(new DrawableRectangle(QRectF(topLeft, sizef), _pZorderStore->GetZorder(false), penKind, penWidth, isFilled)); break;
		case DrawableType::dtScreenShot:ivi = Push_back(new DrawableScreenShot(topLeft, _pZorderStore->GetZorder(true), *pimage)); break;
		case DrawableType::dtScribble:  ivi = Push_back(new DrawableScribble(penKind, penWidth, _pZorderStore->GetZorder(false))); break;    // add points later
		case DrawableType::dtText:      ivi = Push_back(new DrawableText(topLeft, _pZorderStore->GetZorder(false))); break;                  // add text later
		default:  break;
	}
	if (_pQTree)
		_pQTree->Add(ivi);
	return ivi;

	//return ivi->second;
}
int DrawableList::AddDrawable(DrawableType dType, FalconPenKind penKind, qreal penWidth, QPointF topLeft, QSizeF sizef, bool isFilled)
{
	return AddDrawable(dType, nullptr, penKind, penWidth, topLeft, sizef, isFilled);
}

int DrawableList::Size(DrawableType type) const
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
void DrawableList::Erase(IntVector& iv)
{
	for (int i = iv.size() - 1; i >= 0; --i)
	{
		delete _items.at(iv[i]);
		_items.removeAt(iv[i]);
	}
}
void DrawableList::Erase(int from)
{
	for (int i = _items.size() - 1; i >= from; --i)
	{
		delete _items.at(i);
		_items.removeAt(i);
	}
}
QRectF DrawableList::ItemsUnder(QPointF point, IntVector& iv, DrawableType type)
{
	// copies indices for visible drawables under cursor into list
	// for non filled drawables when we are near the line, else even when inside
	// point is document relative
	QRectF rect;
	auto addIfNear = [&, this](DrawableItem* pdrw, int i)
		{
			if (pdrw->isVisible && pdrw->PointIsNear(point, pdrw->pen.penWidth / 2.0 + 3))
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
			auto pdrw = _items[iv1.at(i)];
			addIfNear(pdrw, iv1[i]);
		}
	}
	else
	{
		for (int i = 0; i < siz; ++i)
		{
			auto pdrw = _items[iv1.at(i)];

			if (pdrw->dtType == type)
				addIfNear(pdrw, iv1[i]);
		}
	}
	return rect;
}
int/*DrawableItemIndex*/ DrawableList::IndexOfTopMostItemInlist(IntVector& iv) const
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

int/*DrawableItemIndex*/ DrawableList::IndexOfTopMostItemUnder(QPointF point, int penWidth, DrawableType type) const
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
		DrawableItem* pdrw = nullptr;
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

bool DrawableList::MoveItems(QPointF dr, const DrawableIndexVector& drl)
{
	for (auto& a : drl)
		if (!(*this)[a]->CanTranslate(dr))
			return false;
	for (auto& a : drl)
		(*this)[a]->Translate(dr, -1);
	return true;
}
bool DrawableList::TranslateDrawable(int index, QPointF dr, qreal minY)
{
	return (*this)[index]->Translate(dr, minY);

}
void DrawableList::RotateDrawable(int index, MyRotation rot, QPointF& center)    // alpha used only for 'rotAngle'
{
	(*this)[index]->Rotate(rot, center);
}

void DrawableList::VertShiftItemsBelow(int thisY, int dy) // using the y and z-index ordered index '_yOrder'
{														  // _pQTree must not be NULL
	QRectF area;
	for (auto& p : _items)
	{
		if (p->Area().top() >= thisY)
			p->Translate({ 0, (qreal)dy }, -1);

		area = area.united(p->Area());
	}
	_pQTree->ResizeWhenCoordinateOfValuesChanges(area);	// items changed
}
