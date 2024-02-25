#pragma once
#ifndef _PDFSETUP_H
#define _PDFSETUP_H

#include <QtWidgets/QDialog>
#include <QPageSize>

#include "ui_pagesetup.h"

QPageSize::PageSizeId PageSizeId(int index);

struct PageParams
{
	enum UnitIndex { uiInch, uiCm, uiMm };
	enum PageSetupType {wtdPageSetup, wtdPrint, wtdExportPdf};

	static int resolutionIndex;		// screen resolution in resolution combobox (pixels)
	static int horizPixels;			// horizontal pixel count
	static bool useResInd;			// use resolution index or horizPixels?

	static int screenDiagonal;		// from text in 'edtScreenDiag' in inches
	static int screenPageWidth;     // width of page on screen in pixels - used to calculate pixel limits for pages (HD: 1920 x 1080)
	static int screenPageHeight;	// height of screen for selected paper size in pixels - for A4 (210mm x 297mm, 8.27 in x 11.7 in) with 300 dpi

	static UnitIndex unitIndex;		// index of units for margins (in ini file 0: inch, 1: cm, 2: mm)
	static unsigned flags;			// ORed from flags (common.h), used bits: 0-6, 8
	static unsigned pgPositionOnPage; //0-5: top-left, top-center, top-right, bottom-left, bottom-center, bottom-right
	static int startPageNumber;	
				// for print/PDF
	static int pageSizeIndex;		// in page size combo box
	static QPageSize::PageSizeId paperSizeId; // set to myPageSizes[pageSizeIndex].pid
	static int pdfDpiIndex;			// 0,1,2 -> dots/inch = (pdfDpi * 300 + 300)
	static double printWidth, printHeight,	// page size in inches, depends on orientation
				hMargin,			// left and right in inches
				vMargin,			// top and bottom -"-
				gutterMargin;		// inches, left on odd, right on even pages

	static UnitIndex pdfUnitIndex;
	static QString actPrinterName;	// if not this is its name



	static void Load();
	static void Save();

	static auto GetUnit(int ix);
	static auto UnitToIndex(UnitIndex ui);

	static void SetScreenWidth();

	PageParams() = delete;
private:
	static bool _isLoaded;
};


class PageSetupDialog : public QDialog
{
	Q_OBJECT
public:
	PageSetupDialog(QWidget* parent, PageParams::PageSetupType whatToDo = PageParams::wtdPageSetup);
	~PageSetupDialog();

	PageParams::PageSetupType whatToDo= PageParams::wtdPageSetup;	// will this be a PDF document?
	double pdfMaxLR,				// these 2 are in inches and not saved
		   pdfMaxTB;

	int GetScreenSize(QSize& size);
private:
	Ui::PageSetupDialog ui;


	static int _h[];
	static int _w[];
	bool _busy = false;
	bool _changed = false;

	int _GetIndexForHorizPixels(int hp);
	int _GetScreenSize(int index, QSize& size);

	void _ChangePrintMarginsUnit(); 
	void _ChangePdfPaperSize(); 

private slots:
	void on_btnOk_clicked();

	void on_cbOrientation_currentIndexChanged(int i);
	void on_cbPdfUnit_currentIndexChanged(int i);
	void on_cbPrinterSelect_currentIndexChanged(int i);
	void on_cbScreenResolution_currentIndexChanged(int i);
	void on_cbUnit_currentIndexChanged(int i);

	void on_chkDontPrintImages_toggled(bool);
	void on_chkGrayscale_toggled(bool b);
	void on_chkGrid_toggled(bool b);
	void on_chkOpenPDFInViewer_toggled(bool);
	void on_chkPageNumbers_toggled(bool);
	void on_cbPdfPaperSize_currentIndexChanged(int i);
	void on_chkPrintBackgroundImage_toggled(bool b);
	void on_chkWhiteBackground_toggled(bool b);

	void on_edtScreenDiag_textChanged(const QString& txt);

	void on_rb300_toggled(bool b);
	void on_rb600_toggled(bool b);
	void on_rb1200_toggled(bool b);

	void on_rbTl_toggled(bool b){if(b) PageParams::pgPositionOnPage = 0;}
	void on_rbTc_toggled(bool b){if(b) PageParams::pgPositionOnPage = 1;}
	void on_rbTr_toggled(bool b){if(b) PageParams::pgPositionOnPage = 2;}
	void on_rbBl_toggled(bool b){if(b) PageParams::pgPositionOnPage = 3;}
	void on_rbBc_toggled(bool b){if(b) PageParams::pgPositionOnPage = 4;}
	void on_rbBr_toggled(bool b){if(b) PageParams::pgPositionOnPage = 5;}

	void on_sbFirstPageNumber_valueChanged(int value);
																
	//void on_rbPgBottom_toggled(bool b);	// b == false => top
	//void on_rbPgLeft_toggled(bool b);
	//void on_rbPgCenter_toggled(bool b);
	//void on_rbPgRight_toggled(bool b);
	void on_rbUseResInd_toggled(bool b);

	void on_sbHorizPixels_valueChanged(int val);
	void on_sbMarginLR_valueChanged(double val);
	void on_sbMarginTB_valueChanged(double val);
	void on_sbGutterMargin_valueChanged(double val);
};

#endif