#pragma once
#ifndef _PDFSETUP_H
#define _PDFSETUP_H

#include <QtWidgets/QDialog>

#include "ui_pdfsetup.h"

class PdfSetupDialog : public QDialog
{
	Q_OBJECT
public:
	enum UnitIndex { uiInch, uiCm, uiMm };
public:
	int resolutionIndex;		// screen resolution in resolution combobox (pixels)
	int horizPixels;			// horizontal pixel count
	int screenDiagonal;			// from text in 'edtScreenDiag' in inches
	UnitIndex unitIndex;		// index of units for margins (in ini file 0: inch, 1: cm, 2: mm)
	int flags;					// ORed from PrinterFlags (common.h)
				// for PDF
	int pdfIndex;				// in page size combo box
	double pdfWidth, pdfHeight,	// page size in inches
		printMarginLR,			// left and right in inches
		printMarginTB,			// top and bottom -"-
		gutterMargin,			// left on odd, right on even pages
		pdfMaxLR,				// these 2 are not saved
		pdfMaxTB;
		

	UnitIndex pdfUnitIndex;

	int	pdfDpi;					// 0,1,2: dots / inch = (pdfDpi * 300 + 300)


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

	void _ChangeprintMarginsUnit(); 
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
	void on_sbGutterMargin_valueChanged(double val);
};

#endif