#pragma once
#ifndef _PAGESETUP_H
#define _PAGESETUP_H

#include <QtWidgets/QDialog>

#include "ui_pagesetup.h"

class PageSetupDialog : public QDialog
{
	Q_OBJECT
public:
	int resolutionIndex;		// in resolution table
	int screenDiagonal;			// inch
	int unitFactor;				// index of units (0: inch, 1: cm, 2: mm)
	int orientation;			// 0: portrait, 1: landscape
	QString actPrinter;


	PageSetupDialog(QWidget* parent, QString actPrinter);
	~PageSetupDialog();
private:
	Ui::PageSetupClass ui;

private slots:
	void on_cbScreenResolution_currentIndexChanged(int i);
	void on_edtScrenDiag_textChanged(QString& txt);
	void on_cbUnit_currentIndexChanged(int i);
	void on_cbOrientation_currentIndexChanged(int i);
	void on_cbPrinterSelect_currentIndexChanged(int i);
};

#endif