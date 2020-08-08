#include <QSpinBox>
#include <QLabel>
#include <QScreen>
#include <QPrinter>
#include <QSettings>

#include <QtPrintSupport/qtprintsupportglobal.h>
#include <QPrinter>
#include <QPrintDialog>
#include <QPrinterInfo>

#include "common.h"
#include "pdfsetup.h"


int PdfSetupDialog::_w[] = { 3840,
						   3440,
						   2560,
						   2560,
						   2048,
						   1920,
						   1920,
						   1680,
						   1600,
						   1536,
						   1440,
						   1366,
						   1360,
						   1280,
						   1280,
						   1280,
						   1024,
						   800 ,
						   640
					};
int PdfSetupDialog::_h[] = { 2160,
							1440,
							1440,
							1080,
							1152,
							1200,
							1080,
							1050,
							900,
							864,
							900,
							768,
							768,
							1024,
							800,
							720,
							768,
							600,
							360
								};

struct PageSizes
{
	double w, h;
	int index;
  
}  pageSizes[]{    // in inches as resolution is always in dots / inch
				//{ 1.041666667,  1.458333333,  0}, //   A10
				//{ 1.458333333,  2.041666667,  1}, //   A9	
				//{ 2.041666667,  2.916666667,  2}, //   A8	
				{ 2.916666667,  4.125000000,  3}, //   A7	
				{ 4.125000000,  5.833333333,  4}, //   A6	
				{ 5.833333333,  8.250000000,  5}, //   A5	
				{ 8.250000000, 11.708333333,  6}, //   A4	
				{11.708333333, 16.541666667,  7}, //   A3	
				{16.541666667, 23.375000000,  8}, //   A2	
				{23.375000000, 33.125000000,  9}, //   A1	
				{33.125000000, 46.791666667, 10}, //   A0 

				//{ 1.208333333,  1.750000000, 11}, //   B10
				//{ 1.750000000,  2.458333333, 12}, //   B9 
				//{ 2.458333333,  3.458333333, 13}, //   B8 
				{ 3.458333333,  4.916666667, 14}, //   B7 
				{ 4.916666667,  6.916666667, 15}, //   B6 
				{ 6.916666667,  9.833333333, 16}, //   B5 
				{ 9.833333333, 13.916666667, 17}, //   B4 
				{13.916666667, 19.666666667, 18}, //   B3 
				{19.666666667, 27.833333333, 19}, //   B2 
				{27.833333333, 39.375000000, 20}, //   B1 
				{39.375000000, 55.666666667, 21}, //   B0 

				{8.5, 11 , 22}, // US Letter, 
				{2.5, 14 , 23}, // US Legal,     
				{5.5, 8.5, 24}  // US Stationary,
			};

int PdfSetupDialog::_GetScreenSize(int index, QSize& size)
{
	if (index >= 0)
	{
		size = QSize(_w[index], _h[index]);
		return _w[index];
	}
	else
		size = QSize(horizPixels, -1);

	return horizPixels;
}

int PdfSetupDialog::GetScreenSize(QSize& size)
{
	return _GetScreenSize(resolutionIndex, size);
}

PdfSetupDialog::PdfSetupDialog(QWidget* parent) : QDialog(parent)
{
	ui.setupUi(this);
	_busy = true;

	ui.edtScreenDiag->setValidator(new QIntValidator(1, 200, this));

	QSettings s("FalconBoard.ini", QSettings::IniFormat);
	resolutionIndex = s.value("resi", 6).toInt();		// 1920 x 1080
	horizPixels     = s.value("hpxs", 1920).toInt();
	screenDiagonal	= s.value("sdiag", 24).toInt();		// inch
	unitIndex		= s.value("uf", 0).toInt();			// index to determines the multipl. factor for number in edScreenDiag number to inch
	flags			= s.value("pflags", 0).toInt();		// ORed PrinterFlags

	ui.cbScreenResolution->setCurrentIndex(resolutionIndex);
	ui.edtScreenDiag->setText(QString().setNum(screenDiagonal)) ;
	ui.cbUnit->setCurrentIndex(unitIndex);
	ui.cbOrientation->setCurrentIndex(flags & pfLandscape ? 1 : 0);
	ui.chkWhiteBackground->setChecked(flags & pfWhiteBackground);
	ui.chkGrayscale->setChecked(flags & pfGrayscale);
	ui.chkPrintBackgroundImage->setChecked(flags & pfPrintBackgroundImage);
	ui.chkGrid->setChecked(flags & pfGrid);
	ui.chkDontPrintImages->setChecked(flags & pfDontPrintImages);
	// for PDF
	pdfIndex = s.value("pdfpgs", 3).toInt();			// index in paper size combo box (default: A$)
	pdfWidth  = pageSizes[pdfIndex].w;
	pdfHeight = pageSizes[pdfIndex].h;
	pdfMarginLR = s.value("pdfmlr", 1.0).toFloat();		// left - right	in inches
	pdfMarginTB = s.value("pdfmtb", 1.0).toFloat();		// top - bottom	in inches

	pdfMaxLR = pageSizes[pdfIndex].w / 3.0;				// always stored in inches
	pdfMaxTB = pageSizes[pdfIndex].h / 3.0;				// always stored in inches

	pdfUnitIndex = s.value("pdfui", 0).toInt();			// 0: cm, 1: inch
	pdfDpi = s.value("pdfdpi", 2).toInt();				// index: 0: 300, 1: 600, 2: 1200 dpi

	float fact = pdfUnitIndex ? 1.0 : 2.54;
	ui.sbMarginLR->setMaximum(pdfMaxLR*fact);	
	ui.sbMarginTB->setMaximum(pdfMaxTB*fact);
	ui.sbMarginLR->setValue(pdfMarginLR*fact);	
	ui.sbMarginTB->setValue(pdfMarginTB*fact);
	ui.cbPdfPaperSize->setCurrentIndex(pdfIndex);
	ui.cbPdfUnit->setCurrentIndex(pdfUnitIndex);

	switch (pdfDpi)
	{
		case 0: ui.rb300->setChecked(true); break;
		case 1: ui.rb600->setChecked(true); break;
		case 2: 
		default:ui.rb1200->setChecked(true); break;
	}

	_busy = false;
}

void PdfSetupDialog::_SaveParams()
{
	QSettings s("FalconBoard.ini", QSettings::IniFormat);
	s.setValue("resi", ui.cbScreenResolution->currentIndex());
	s.setValue("hpxs", ui.sbHorizPixels->value());
	s.setValue("sdiag", ui.edtScreenDiag->text());
	s.setValue("pflags", flags);
	// for PDF
	s.setValue("pdfpgs", pdfIndex);
	s.setValue("pdfmlr", pdfMarginLR);
	s.setValue("pdfmtb", pdfMarginTB);
	s.setValue("pdfui" , pdfUnitIndex);
	s.setValue("pdfdpi", pdfDpi);
}

PdfSetupDialog::~PdfSetupDialog()
{
}

int PdfSetupDialog::_GetIndexForHorizPixels(int hp)
{
	for (size_t i = 0; i < sizeof(_w) / sizeof(int); ++i)
		if (_w[i] == hp)
			return i;
	return -1;
}

void PdfSetupDialog::on_cbScreenResolution_currentIndexChanged(int i) 
{ 
	if (_busy)
		return;
	resolutionIndex = i;
	_busy = true;
	if (i >= 0)
		ui.sbHorizPixels->setValue(_w[i]);
	_busy = false;
}

void PdfSetupDialog::on_sbHorizPixels_valueChanged(int val)
{
	if (_busy)
		return;
	_busy = true;
	int ix = _GetIndexForHorizPixels(val);
	resolutionIndex = ix;
	ui.cbScreenResolution->setCurrentIndex(ix);
	_busy = false;
}


void PdfSetupDialog::on_edtScreenDiag_textChanged(QString& txt) { screenDiagonal = txt.toInt(); }
void PdfSetupDialog::on_cbUnit_currentIndexChanged(int i) { unitIndex = i; }
void PdfSetupDialog::on_cbOrientation_currentIndexChanged(int landscape) 
{ 
	flags &= ~pfLandscape; 
	if (landscape) 
		flags |= pfLandscape; 
}

void PdfSetupDialog::on_chkPrintBackgroundImage_toggled(bool b)
{
	flags &= ~pfPrintBackgroundImage;
	if (b)
		flags |= pfPrintBackgroundImage;
}

void PdfSetupDialog::on_chkWhiteBackground_toggled(bool b)
{
	flags &= ~pfWhiteBackground;
	if(b)
		flags |= pfWhiteBackground;
}

void PdfSetupDialog::on_chkGrayscale_toggled(bool b)
{
	flags &= ~pfGrayscale;
	if (b)
		flags |= pfGrayscale;
}

void PdfSetupDialog::on_chkGrid_toggled(bool b)
{
	flags &= ~pfGrid;
	if (b)
		flags |= pfGrid;
}

void PdfSetupDialog::on_chkDontPrintImages_toggled(bool b)
{
	flags &= ~pfDontPrintImages;
	if (b)
	{
		flags |= pfDontPrintImages;
		ui.chkPrintBackgroundImage->setChecked(false);
	}
}

void PdfSetupDialog::on_btnExportPdf_clicked()
{
	_SaveParams();
	accept();
//	setResult(QDialog::Accepted);
//	close();
}
void PdfSetupDialog::on_cbPdfPaperSize_currentIndexChanged(int i)
{
	pdfIndex = i;
	_ChangePdfPaperSize();
}

void PdfSetupDialog::on_rb300_toggled(bool b)
{
	if(b)
		pdfDpi = 0;
}
void PdfSetupDialog::on_rb600_toggled(bool b)
{
	if (b)
		pdfDpi = 1;
}
void PdfSetupDialog::on_rb1200_toggled(bool b)
{
	if (b)
		pdfDpi = 2;
}

void PdfSetupDialog::_ChangePdfPaperSize()	
{
	if (_busy)
		return;

	double factM = pdfUnitIndex ? 1.0 : 2.54;	// if 0: numbers are in cm, if 1 in inches
	if (pdfIndex >= 0)
	{
		pdfWidth  = pageSizes[pdfIndex].w;
		pdfHeight = pageSizes[pdfIndex].h;
		pdfMaxLR  = pdfWidth  / 3.0;
		pdfMaxTB  = pdfHeight / 3.0;
		if (pdfMarginLR > pdfMaxLR)
		{
			pdfMarginLR = pdfMaxLR;
			_busy = true;
			ui.sbMarginLR->setValue(pdfMaxLR * factM);
			_busy = false;

		}
		if (pdfMarginTB > pdfMaxTB)
		{
			pdfMarginTB = pdfMaxTB;
			_busy = true;
			ui.sbMarginTB->setValue(pdfMaxTB * factM);
			_busy = false;

		}
		ui.sbMarginLR->setMaximum( pdfMaxLR * factM );
		ui.sbMarginTB->setMaximum( pdfMaxTB * factM );
	}
}
void PdfSetupDialog::_ChangePdfMarginsUnit()
{
	if (_busy)
		return;
	// unit change does not change the stored values
	double factM = pdfUnitIndex ? 1.0 : 2.54;
	ui.sbMarginLR->setMaximum( pdfMaxLR * factM );
	ui.sbMarginTB->setMaximum( pdfMaxTB * factM );
	ui.sbMarginLR->setValue( pdfMarginLR * factM );
	ui.sbMarginTB->setValue( pdfMarginTB * factM );
}


void PdfSetupDialog::on_cbPdfUnit_currentIndexChanged(int i)
{
	if (_busy)
		return;
	pdfUnitIndex = i;
	_ChangePdfMarginsUnit();
}

void PdfSetupDialog::on_sbMarginLR_valueChanged(double val)
{
	if (!pdfUnitIndex)	// cm
		val /= 2.54;
	pdfMarginLR = val;	// always in inches
}

void PdfSetupDialog::on_sbMarginTB_valueChanged(double val)
{
	if (!pdfUnitIndex)	// cm
		val /= 2.54;
	pdfMarginTB = val;	// always in inches
}
