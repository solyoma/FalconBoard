#pragma once
#ifndef _ROTATEINPUT_H
#define _ROTATEINPUT_H

#include <QtWidgets>
#include <QtWidgets/QDialog>

#include "ui_rotateinput.h"

class RotateInputDialog : public QDialog
{
	Q_OBJECT
public:
	RotateInputDialog(QWidget* parent, qreal &initialAndReturnValue) : _retVal(initialAndReturnValue)
	{
		ui.setupUi(this);

		qreal r = _retVal;
		if (r < 0)
		{
			ui.rbLeft->setChecked(true);
			r = -r;
		}
		else
			ui.rbRight->setChecked(true);

		ui.sbdRotAngle->setValue(r);
	}
private:
	Ui::RotateInputDialog ui;
	qreal& _retVal;
private slots:
	void on_btnOk_clicked()
	{
		_retVal = ui.sbdRotAngle->value();
		if (ui.rbLeft->isChecked())
			_retVal = -_retVal;
	}
};

#endif
