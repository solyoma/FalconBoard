#pragma once
#ifndef _PAGESETUP_H
#define _PAGESETUP_H

#include <QtWidgets/QDialog>

#include "ui_pagesetup.h"

class PageSetupDialog : public QDialog
{
	Q_OBJECT
public:
	int resolutionIndex;		// screen resolution in resolution combobox
	int horizPixels;			// horizontal pixel count
	int screenDiagonal;			// from text in inch
	int unitIndex;				// index of units (0: inch, 1: cm, 2: mm)
	int flags;					// ORed from PrinterFlags (common.h)
	bool bExportPdf = false;
	QString _actPrinterName;

	PageSetupDialog(QWidget* parent, QString _actPrinterName);
	~PageSetupDialog();

	int GetScreenSize(QSize& size);
private:
	Ui::PageSetupClass ui;

	static QSize _pgsize[];
	bool _busy = false;

	int _GetIndexForHorizPixels(int hp);
	int _GetScreenSize(int index, QSize& size);

	void _SaveParams();

private slots:
	void on_okButton_clicked();

	void on_cbScreenResolution_currentIndexChanged(int i);
	void on_sbHorizPixels_valueChanged(int val);
	void on_edtScreenDiag_textChanged(QString& txt);
	void on_cbUnit_currentIndexChanged(int i);
	void on_cbOrientation_currentIndexChanged(int i);
	void on_cbPrinterSelect_currentIndexChanged(int i);
	void on_chkPrintBackgroundImage_toggled(bool b);
	void on_chkWhiteBackground_toggled(bool b);
	void on_chkGrayscale_toggled(bool b);
	void on_chkGrid_toggled(bool b);
	void on_chkDontPrintImages_toggled(bool);
};

#endif