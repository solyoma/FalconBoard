#include <QMouseEvent>
#include "snipper.h"

Snipper::Snipper(QWidget* parent) : QLabel(parent, Qt::FramelessWindowHint)
{
	setWindowModality(Qt::ApplicationModal);
}

void Snipper::_Pressed(QPoint pos, Qt::MouseButton button)
{
	if( (button == Qt::LeftButton) /*|| (button == Qt::RightButton) */)
	{
		_rubber_origin = pos;
		if (!_rubberBand)
			_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
		_rubberBand->setGeometry(QRect(_rubber_origin, QSize()));
		_rubberBand->show();
	}
}

void Snipper::_Moved(QPoint pos)
{
	if (_rubberBand)
		_rubberBand->setGeometry(QRect(_rubber_origin, pos).normalized()); // means: top < bottom, left < right
}

void Snipper::_Released(QPoint pos)
{
	if (_rubberBand)
	{
		_rubberBand->hide();
		QRect rect;
		if (_rubberBand->geometry().width() > 10 && _rubberBand->geometry().height() > 10)
			rect = _rubberBand->geometry();
		//		event->accept();
		delete _rubberBand;
		_rubberBand = nullptr;
		if(rect.isValid())
			emit SnipperReady(rect);
	}
}

void Snipper::mousePressEvent(QMouseEvent* mevent)
{
	if (_tabletInput)
		return;
	_Pressed(mevent->pos(), mevent->button());
}

void Snipper::mouseMoveEvent(QMouseEvent* event)
{
	if (_tabletInput)
		return;
	_Moved(event->pos());

}

void Snipper::mouseReleaseEvent(QMouseEvent* event)
{
	if (_tabletInput)
		return;
	_Released(event->pos());
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
			_Pressed(event->pos(), Qt::LeftButton);
			event->accept();
			break;
		case QEvent::TabletMove:
			_Moved(event->pos());
			event->accept();
			break;
		case QEvent::TabletRelease:
			_Released(event->pos());
			event->accept();
			break;
		default:
			break;
	}
}


