#include "floodfill.h"

FloodFiller::resultCode FloodFiller::Fill()
{
	QColor thisColor, fillColor, borderColor;
	int minY, minX, MaxY, MaxX, dy;
	int childLeft, childRight;
	int parentLeft, parentRight;

	thisColor = _img.pixel(_ptStart);
	if (_brush.style() == Qt::SolidPattern)
	{
		fillColor = _brush.color();
	}
}