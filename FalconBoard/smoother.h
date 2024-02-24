#pragma once
#ifndef _SMOTTHER_H
#define _SMOOTHER_H

#include <vector>


/* A class to smooth a somewhat noisy hand drawn 2D line */
/*  width = 4
    h
    x x x x x   -
    t

    h
    1 2 3 4 x
            t
      h
    x 2 3 4 5
    t
*/    
// T must have a default constructor, operator=() operator+(), operator-() and operator/(type N)
template <typename T, typename N, int wwidth>   
class Smoother
{

#if 0
    class Averager
    {
        int _head = 0, _tail = 0;

        T _average;
        std::vector<T> _dat;
        bool IsEmpty() const { return _head == _tail; }
        bool IsFull() const { return _Size() == wwidth; }
        int _Size() const
        {
            if (IsEmpty()) return 0;
            return _tail > _head ? _tail - _head : wwidth + 1 + _tail - _head;
        }

        void _In(T& data)
        {
            _dat[_tail] = data;

            _tail = (_tail + 1) % (wwidth + 1);

            if (_tail == _head)
                _head = (_head + 1) % (wwidth+1);
        }
        T _Out()
        {
            T data;

            if (_head != _tail)
                data = _dat[_head];
            else
                throw "Empty";
            _head = (_head + 1) % (wwidth + 1);
            return data;
        }
    public:
        Averager()
        {
            _dat.resize(wwidth + 1);
        }

        void Reset()
        {
            _head = _tail = 0;
        }

        T Average(T& newData)
        {
            T prevData;
            int siz = _Size();
            if (siz >= wwidth)
            {
                prevData = _Out();
                _In(newData);
                _average = _average + (newData - prevData) / siz;
            }
            else
            {
                _In(newData);
                if (siz)
                    _average = (_average * (siz - 1) + newData) / siz;
                else
                    _average = newData;
            }
            // DEBUG
//            qDebug("in:(%4.1f, %4.1f), prev:(%4.1f, %4.1f), out:(%4.1f, %4.1f)", newData.x(),newData.y(), prevData.x(),prevData.y(),_average.x(),_average.y());
            // end DEBUG

            return _average;
        }

    };
    Averager _averager;
#endif

#if 1
    class Exponenter
    {
        double alpha;
        T _prevAver;
    public:
        Exponenter()
        {
            alpha = wwidth / 1000.0;
        }
        T Average(T& newData, QVector<T>&points)
        {
            int n = points.size(); // may be less than 5 (e.g. horizontal line), but never more
            if (n < 5)    // so a dot, a 2 point line or a 4 points rectangle does not get smoothed
                _prevAver = newData;
            else
            {
                // DEBUG
                //if (points.size() == 10)
                //{
                //    qDebug("_______________________");
                //    int n = -1;
                //    for (auto pt : points)
                //        qDebug("%d. (%6.2f, %6.2f)", ++n, pt.x(), pt.y());
                //}
                // end DEBUG

                _prevAver = alpha * newData + (1 - alpha) * _prevAver;
            }
            return _prevAver;
        }
        //void SmoothUnsmoothed(QVector<T>& points) // first 4 points smoothed (how to do it backwards?)
        //{
        //    int n = points.size(); 
        //    if (n > 4)
        //    {
        //        for (int i = 1; i < 5; ++i)
        //        {
        //            points[i] = alpha * points[i] + (1 - alpha) * points[i - 1];
        //            points[i - 1] = (points[i] *)
        //        }
        //    }
        //}
    };
    Exponenter _averager;
#endif
public:
    Smoother()  {}
    ~Smoother() {}
    T AddPoint(T p, QVector<T> &pOrigD) // returns smoothed point when Fifo is full
    {               // else the average of all points
        return _averager.Average(p, pOrigD);
    }
};

#endif