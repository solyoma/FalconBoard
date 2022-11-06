#pragma once
#ifndef _SNIPPER_H
#define SNIPPER_H

#include <QRubberBand>
#include <QLabel>
#include <QTabletEvent>


class Snipper : public QLabel
{
	Q_OBJECT


	QRubberBand* _rubberBand = nullptr;	// mouse selection with right button
	QPoint   _rubber_origin;
	bool _tabletInput = false;

	void _Pressed(QPoint pos, Qt::MouseButton button);
	void _Moved(QPoint pos);
	void _Released(QPoint pos);

protected:
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void tabletEvent(QTabletEvent* event) override;

public:
	Snipper(QWidget* parent = nullptr);
	~Snipper() {}
	void show()
	{
		setCursor(Qt::CrossCursor);
		QLabel::show();  
	}
signals:
	void SnipperCancelled();
	void SnipperReady(QRect geometry);
};

#endif
