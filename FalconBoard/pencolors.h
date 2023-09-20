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

	void mousePressEvent(QMouseEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
private:
	QColor _colors[4][2];
	QLineEdit *pe[4][2];
	QToolButton* pb[4][2], *pbD[4][2];
	Ui::PenColorDialogClass ui;

	void _SelectColor(int row, int col);
	void _SetColorForButton(int which, int dark);
	void _SetTextForLine(int which, int dark);
};

#endif