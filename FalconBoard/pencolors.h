#pragma once
#ifndef _PENCOLORS_H
#define _PENCOLORS_H

#include <QtWidgets/QDialog>
#include <QColorDialog>
#include <QToolButton>

#include  "common.h"
#include "ui_pencolors.h"

class PenColorsDialog : public QDialog
{
	Q_OBJECT

public:
	PenColorsDialog(QWidget* parent = nullptr);
	~PenColorsDialog();

	bool GetChanges(DrawColors &drcls) const 
	{
		int changeCount = 0;
		for (int i = 0; i < PEN_COUNT - 2; ++i)
		{
			if ((_colors[i][0] != drcls.Color((FalconPenKind)(i + 2), 0)) ||
				(_colors[i][1] != drcls.Color((FalconPenKind)(i + 2), 1)))
			{
				drcls.SetupPenAndCursor((FalconPenKind)(i + 2), _colors[i][0], _colors[i][1], pe[i][0]->text(), pe[i][1]->text());
				++changeCount;
			}
		}
		return changeCount;
	}
private:
	QColor _colors[4][2];
	QLineEdit *pe[4][2];
	QToolButton* pb[4][2], *pbD[4][2];
	Ui::PenColorDialogClass ui;

	QString _makeStyle(int i, int dark);
	void _SelectColor(int row, int col);

public slots:
	void on_btnL2_clicked();
	void on_btnL3_clicked();
	void on_btnL4_clicked();
	void on_btnL5_clicked();
	void on_btnD2_clicked();
	void on_btnD3_clicked();
	void on_btnD4_clicked();
	void on_btnD5_clicked();
};

#endif