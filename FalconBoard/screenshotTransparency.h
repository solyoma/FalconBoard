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
	ScreenShotTransparencyDialog(QWidget* parent, QColor trcolor, bool usetr, bool applyToLoaded);
	~ScreenShotTransparencyDialog() {};

	void GetResult(QColor &trcolor, bool& usetr, bool &applyToLoaded);

private:
	Ui::dlgScreenShotTrasparencyClass ui;

	QColor	_trcolor;

private slots:
	void on_btnColor_clicked();
	void on_chkUse_toggled(bool b);
};
#endif // _not VIEWER

#endif