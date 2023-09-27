#pragma once
#ifndef _PENCOLORS_H
#define _PENCOLORS_H

#include <QtWidgets/QDialog>
#include <QColorDialog>
#include <QToolButton>

#include  "common.h"
#include "ui_pencolorsdialog.h"
#include "ui_pencolorsdialog.h"

class PenColorsDialog : public QDialog
{
	Q_OBJECT

	struct Data
	{		    
		QString title;
		QColor  colors[4][2];
		QString names[4][2];

		void SetupFrom(const DrawColors& dc, QString schemeName);
		//void CopyInto(DrawColors& dc);
	} _data;
	QVector<Data> _schemes;
public:
	PenColorsDialog(QWidget* parent = nullptr);
	~PenColorsDialog();

	bool GetChanges(DrawColors& drcls) const;
private:
	bool _busy = false;
	bool _changed = false;
	QLineEdit *pe[4][2];
	QToolButton* pb[4][2], *pbD[4][2];
	Ui::PenColorDialogClass ui;

	int _ReadData(bool overWriteActual);	// returns count of stored data
	void _SetupFrom(int which);				// texts and buttons which < 0 => use actual data, else the i-th stored one
	QString _makeStyle(int whichPen, int dark);	// uses 'data', set from 'schemes'
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

	void on_cbSelectScheme_currentIndexChanged(int index);
	void on_btnSaveScheme_pressed();
};

#endif