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
            qDebug("in:(%4.1f, %4.1f), prev:(%4.1f, %4.1f), out:(%4.1f, %4.1f)", newData.x(),newData.y(), prevData.x(),prevData.y(),_average.x(),_average.y());
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
        bool _initted = false;
    public:
        Exponenter()
        {
            alpha = wwidth / 1000.0;
        }
        void Reset()
        {
            _initted = false;
        }
        T Average(T& newData)
        {
            if (!_initted)
            {
                _initted = true;
                return _prevAver = newData;
            }
            else
            {
                T tmp = _prevAver;
                _prevAver = alpha * newData + (1 - alpha) * tmp;
                return _prevAver;
            }
        }
    };
    Exponenter _averager;
#endif
public:
    Smoother()  {}
    ~Smoother() {}
    T AddPoint(T p) // returns smoothed point when Fifo is full
    {               // else the average of all points
        return _averager.Average(p);
    }
    void Reset() 
    { 
        _averager.Reset();
    }

};

#endif