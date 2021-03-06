#pragma once
#ifndef _PDFSETUP_H
#define _PDFSETUP_H

#include <QtWidgets/QDialog>

#include "ui_pdfsetup.h"

class PdfSetupDialog : public QDialog
{
	Q_OBJECT
public:
	int resolutionIndex;		// screen resolution in resolution combobox
	int horizPixels;			// horizontal pixel count
	int screenDiagonal;			// from text in inch
	int unitIndex;				// index of units (0: inch, 1: cm, 2: mm)
	int flags;					// ORed from PrinterFlags (common.h)
				// for PDF
	int pdfIndex;				// in page size combo box
	double pdfWidth, pdfHeight,	// page size in inches
		pdfMarginLR,			// left and right in inches
		pdfMarginTB,			// top and bottom -"-
		pdfMaxLR,				// these 2 are not saved
		pdfMaxTB;
		

	int	pdfUnitIndex,			// 0: cm, 1: inch
		pdfDpi;					// 0,1,2: dots / inch = (pdfDpi * 300 + 300)


	PdfSetupDialog(QWidget* parent);
	~PdfSetupDialog();

	int GetScreenSize(QSize& size);
private:
	Ui::PdfExportDialog ui;

	static int _h[];
	static int _w[];
	bool _busy = false;

	int _GetIndexForHorizPixels(int hp);
	int _GetScreenSize(int index, QSize& size);

	void _ChangePdfMarginsUnit(); 
	void _ChangePdfPaperSize(); 
	void _SaveParams();

private slots:
	void on_btnExportPdf_clicked();

	void on_cbScreenResolution_currentIndexChanged(int i);
	void on_sbHorizPixels_valueChanged(int val);
	void on_edtScreenDiag_textChanged(QString& txt);
	void on_cbUnit_currentIndexChanged(int i);
	void on_cbOrientation_currentIndexChanged(int i);
	void on_chkPrintBackgroundImage_toggled(bool b);
	void on_chkWhiteBackground_toggled(bool b);
	void on_chkGrayscale_toggled(bool b);
	void on_chkGrid_toggled(bool b);
	void on_chkDontPrintImages_toggled(bool);
	void on_cbPdfPaperSize_currentIndexChanged(int i);
	void on_rb300_toggled(bool b);
	void on_rb600_toggled(bool b);
	void on_rb1200_toggled(bool b);
	void on_cbPdfUnit_currentIndexChanged(int i);

	void on_sbMarginLR_valueChanged(double val);
	void on_sbMarginTB_valueChanged(double val);
};

#endif