#ifndef _PDRAW_H
#define _PDRAW_H

#pragma once
// parallel draw processes for scribbles

#include <QColor>
#include <QPoint>
#include <QThread>

#include <thread>
#include <vector>

#define _USE_MATH_DEFINES
#include <cmath>

#include "history.h"    // struct DrawableScribble

// TODO: drawing with delay (for playback of session)
inline void SleepFor(std::chrono::milliseconds mseconds)
{
    std::this_thread::sleep_for(mseconds); // Delay a bit
}

// process data from here

// Algorithm
//      DrawArea::My... functions add the next data point using AddPoint
// 
// Processing and smoothing of user scribble input
// replacing points with smoothed ones
class SmoothPoints
{
public:
    //QSemaphore  write(10000000), 
    //            last;
    SmoothPoints(double sigma = 3) :_sigma(sigma) { _GenerateGaussTable(sigma); }

    enum Orientation {oNone, oHoriz, oVert};

    Orientation AddPoint(QPoint pt, bool setHorizVert = false)    // returns orientation (0:not yet known, 1: horiz, 2:vert)
    {
        _points.push_back( _Smooth(pt));
        ++_cntSmoothedIndex;

        if (!setHorizVert || _points.size() < 2 || _orientation)
            return _orientation;
        
        const int ptLimit = 5;  // this many pixels to determine orientation
        int dx = abs(_points[ptLimit].x() - _points[0].x()),
            dy = abs(_points[ptLimit].y() - _points[0].y());
        if (dx < ptLimit && dy < ptLimit)
            return _orientation;

        _orientation = dx > dy ? oHoriz : oVert;

        return _orientation;
    }
    void Reset() { _points.empty(); _orientation = oNone; _cntSmoothedIndex = 0; }
    int GetSmoothed(QPolygon& qpt)
    {
        qpt = _points;
        return _points.size();
    }
private:
    QPolygon _points;
    int _cntSmoothedIndex = 0;
    Orientation _orientation = oNone;
//    int const _cntGausPts = 10;    // this many prevous points are used
    #define _cntGausPts  10        // this many prevous points are used
    double _sigma;
    double _gaussTable[_cntGausPts][_cntGausPts];   // 1st  index:x, 2nd y
    void _GenerateGaussTable(double sigma)   // only  half of 1D gauss
    {
        _sigma = sigma;
        double gnorm = 1 / sigma / sigma / M_PI;    // 2D norm
        for (int i = 0; i < _cntGausPts; ++i)
            for (int j = 0; j < _cntGausPts; ++j)
                _gaussTable[i][j] = gnorm * exp( (i * i + j * j) / (2 * sigma));
    }


    QPointF _Gauss(QPoint pt)
    {
        double gnorm = _gaussTable[0][0];
        double vx=pt.x()*gnorm , vy=pt.y()*gnorm;
        for (int i = 1; i < _cntSmoothedIndex; ++i)
        {
            double xn = _points[_cntSmoothedIndex - i].x();
            double yn = _points[_cntSmoothedIndex - i].y();
            int nx = fabs(xn - pt.x()),
                ny = fabs(yn - pt.y());
            double g = _gaussTable[nx][ny];

            vx += xn * g;
            vx += yn * g;
        }
        return QPointF(vx, vy);
    }

    QPoint _Smooth(QPoint pt)
    {
        if (_points.size() < 2)
            return pt;
        if (_orientation == oHoriz)
            pt.setY(_points[0].y());
        else if (_orientation == oVert)
            pt.setX(_points[0].x());
        else
            pt = _Gauss(pt).toPoint();
        return pt;
    }
};
extern SmoothPoints pointBuffer;   // in DrawAra.cpp

#endif
