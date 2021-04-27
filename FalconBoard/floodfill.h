#pragma once

#include <QColor>
#include <QImage>
#include <QBrush>
#include <QBitmap>
#include <QPoint>
#include <QWidget>

class FloodFiller {
	QImage& _img;
	QPoint _ptStart;
	QBrush &_brush;
public:

	enum resultCode { ffSuccess, ffINvalidStart, fdfunknownError };

	FloodFiller(QImage& img, QPoint ptStart, QColor &color) : _img(img),_ptStart(ptStart), _brush(QBrush(color) ) { }
	FloodFiller(QImage& img, QPoint ptStart, QBrush &brush) : _img(img),_ptStart(ptStart), _brush(brush) { }
	resultCode Fill();
};