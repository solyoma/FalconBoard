#include <QMouseEvent>
#include "snipper.h"

Snipper::Snipper(QWidget* parent) : QLabel(parent, Qt::FramelessWindowHint)
{
	setWindowModality(Qt::ApplicationModal);
}

void Snipper::mousePressEvent(QMouseEvent* mevent)
{
	if (_tabletInput)
		return;

	if (mevent->button() == Qt::LeftButton)
	{
		_rubber_origin = mevent->pos();
		if (!_rubberBand)
			_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
		_rubberBand->setGeometry(QRect(_rubber_origin, QSize()));
		_rubberBand->show();
	}
}

void Snipper::mouseMoveEvent(QMouseEvent* event)
{
	if (_tabletInput)
		return;

	QPoint pos = event->pos();

	if (_rubberBand)
		_rubberBand->setGeometry(QRect(_rubber_origin, pos).normalized()); // means: top < bottom, left < right
}

void Snipper::mouseReleaseEvent(QMouseEvent* event)
{
	if (_tabletInput)
		return;

	if (_rubberBand)
	{
		_rubberBand->hide();
		QRect rect;
		if (_rubberBand->geometry().width() > 10 && _rubberBand->geometry().height() > 10)
		{
			rect = _rubberBand->geometry();
		}
		event->accept();
		delete _rubberBand;
		_rubberBand = nullptr;
		emit SnipperReady(rect);
	}
}

void Snipper::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_Escape)
		emit SnipperCancelled();
	else
		QLabel::keyPressEvent(event);
}

void Snipper::tabletEvent(QTabletEvent* event)
{
	QPoint pos;

	switch (event->type())
	{
		case QEvent::TabletPress:
			_tabletInput = true;
			_rubber_origin = event->pos();
			if (!_rubberBand)
				_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
			_rubberBand->setGeometry(QRect(_rubber_origin, QSize()));
			_rubberBand->show();
			event->accept();
			break;
		case QEvent::TabletMove:
			pos = event->pos();

			if (_rubberBand)
				_rubberBand->setGeometry(QRect(_rubber_origin, pos).normalized()); // means: top < bottom, left < right
			event->accept();
			break;
		case QEvent::TabletRelease:
			if (_rubberBand)
			{
				_rubberBand->hide();
				QRect rect;
				if (_rubberBand->geometry().width() > 10 && _rubberBand->geometry().height() > 10)
				{
					rect = _rubberBand->geometry();
				}
				delete _rubberBand;
				_rubberBand = nullptr;
				_tabletInput = false;
				event->accept();
				emit SnipperReady(rect);
			}
			break;
		default:
			break;
	}
}


