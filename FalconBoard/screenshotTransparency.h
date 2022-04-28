#pragma once
#ifndef _SCRSHTRANSP_H
#define _SCRSHTRANSP_H

#include <QtWidgets/QDialog>

#ifndef _VIEWER
	#include "ui_screenshotTransparency.h"

class ScreenShotTransparencyDialog : public QDialog
{
	Q_OBJECT
public:
	ScreenShotTransparencyDialog(QWidget* parent, QColor trcolor, bool usetr, qreal fuzzyness);
	~ScreenShotTransparencyDialog() {};

	void GetResult(QColor &trcolor, bool& usetr, qreal &fuzzyness);

private:
	Ui::dlgScreenShotTrasparencyClass ui;

	QColor	_trcolor;
	qreal _fuzzyness;

private slots:
	void on_btnColor_clicked();
	void on_hsFuzzyness_valueChanged(int value);
};
#endif // _not VIEWER

#endif