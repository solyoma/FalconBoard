#pragma once
// common event handling for mouse and tablet(pen)

// others included in DrawArea
#include <QEvent>
class MyPointerEvent : public QEvent
{
public:
	QEvent* pevent;
	bool fromPen = false;
	Qt::MouseButton button = Qt::NoButton;
	Qt::MouseButtons buttons;
	Qt::KeyboardModifiers mods;
	QPoint pos;
	QPoint globalPos;
	qreal pressure = 1.0;	// always maximum for mouse
	QTabletEvent::PointerType pointerT = QTabletEvent::UnknownPointer;

	MyPointerEvent(bool fromPen, QEvent* pevent) : QEvent(pevent->type()), pevent(pevent), fromPen(fromPen)
	{
		QMouseEvent* pme;
		QWheelEvent* pwe;
		QTabletEvent* ptble;

		if (fromPen)
		{
			ptble = dynamic_cast<QTabletEvent*>(pevent);
			pos = ptble->pos();
			globalPos = ptble->globalPos();
			button = ptble->button();
			buttons = ptble->buttons();
			mods = ptble->modifiers();
			pressure = ptble->pressure();
			pointerT = ptble->pointerType();
		}
		else
		{
			if (pevent->type() == QEvent::Wheel)
			{
				pwe = dynamic_cast<QWheelEvent*>(pevent);
				buttons = pwe->buttons();
				pos = pwe->position().toPoint();
				globalPos = pwe->globalPosition().toPoint();

			}
			else
			{
				pme = dynamic_cast<QMouseEvent*>(pevent);
				pos = pme->pos();
				globalPos = pme->globalPos();
				button = pme->button();
				buttons = pme->buttons();
				mods = pme->modifiers();
			}
		}
	}
};
