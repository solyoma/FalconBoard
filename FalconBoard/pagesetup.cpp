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
#include "pagesetup.h"



QPageSize::PageSizeId PageId(int index)
{ 
	return myPageSizes[index].pid; 
}


int PageSetupDialog::_GetScreenSize(int index, QSize& size)
{
	if (ui.rbUseResInd->isChecked())
	{
		size = QSize(myScreenSizes[index].w, myScreenSizes[index].h);
		return myScreenSizes[index].w;
	}
	else	// use horiz. pixels
		size = QSize(horizPixels, -1);

	return horizPixels;
}

int PageSetupDialog::GetScreenSize(QSize& size)
{
	return _GetScreenSize(resolutionIndex, size);
}

/*======== conversion ========*/
auto getUnit(int ix) { PageSetupDialog::UnitIndex ui[] = { PageSetupDialog::uiInch,PageSetupDialog::uiCm, PageSetupDialog::uiMm }; return ui[ix]; };
auto unitToIndex(PageSetupDialog::UnitIndex ui) { return ui == PageSetupDialog::uiInch ? 0 : ui == PageSetupDialog::uiCm ? 1 : 2; };


PageSetupDialog::PageSetupDialog(QWidget* parent, QString actPrinterName, WhatToDo whatToDo) : whatToDo(whatToDo), actPrinterName(actPrinterName), QDialog(parent)
{

	ui.setupUi(this);
	_busy = true;

	ui.edtScreenDiag->setValidator(new QIntValidator(1, 200, this));

	QSettings *s = FBSettings::Open();
	resolutionIndex = s->value("resi", 6).toInt();		// 1920 x 1080
	ui.cbScreenResolution->setCurrentIndex(resolutionIndex);

	horizPixels     = s->value("hpxs", 1920).toInt();
	useResInd = s->value("useri", true).toBool();

	ui.rbUseResInd->setChecked(useResInd);
	ui.cbScreenResolution->setEnabled(useResInd);
	ui.rbUseHorPixels->setChecked(!useResInd);
	ui.sbHorizPixels->setEnabled(!useResInd);

	screenDiagonal	= s->value("sdiag", 24).toInt();		// inch
	ui.edtScreenDiag->setText(QString().setNum(screenDiagonal)) ;

	unitIndex		= getUnit(s->value("uf", 0).toInt());			// index to determines the multipl. factor for number in edScreenDiag number to inch
	ui.cbUnit->setCurrentIndex(unitIndex);

	flags			= s->value("pflags", 0).toInt();		// ORed PrinterFlags

	ui.cbOrientation->setCurrentIndex(flags & pfLandscape ? 1 : 0);
	ui.chkWhiteBackground->setChecked(flags & pfWhiteBackground);
	ui.chkGrayscale->setChecked(flags & pfGrayscale);
	ui.chkPrintBackgroundImage->setChecked(flags & pfPrintBackgroundImage);
	ui.chkGrid->setChecked(flags & pfGrid);
	ui.chkDontPrintImages->setChecked(flags & pfDontPrintImages);
	// for PDF
	pdfIndex = s->value("pdfpgs", 3).toInt();			// index in paper size combo box (default: A4)

	pdfWidth  = myPageSizes[pdfIndex].w;					// w.o. margins in inches
	pdfHeight = myPageSizes[pdfIndex].h;

	pdfMaxLR = myPageSizes[pdfIndex].w / 3.0;				// always stored in inches
	pdfMaxTB = myPageSizes[pdfIndex].h / 3.0;				// always stored in inches

	pdfDpi = s->value("pdfdpi", 0).toInt();				// index: 0: 300, 1: 600, 2: 1200 dpi
	pdfUnitIndex = getUnit(s->value("pdfui", 0).toInt() );

	float fact = pdfUnitIndex == uiInch ? 1.0 : 2.54;				// to convert cm-s to inches

	hMargin = s->value("pdfmlr", 1.0).toFloat();	// left - right	in inches
	ui.sbMarginLR->setMaximum(pdfMaxLR*fact);		// this may be in inches or cm
	ui.sbMarginLR->setValue(hMargin*fact);	

	vMargin = s->value("pdfmtb", 1.0).toFloat();	// top - bottom	in inches
	ui.sbMarginTB->setMaximum(pdfMaxTB*fact);		// this may be in inches or cm
	ui.sbMarginTB->setValue(vMargin*fact);

	gutterMargin = s->value("pdfgut", 0.0).toFloat();	// gutter in inches
	ui.sbGutterMargin->setMaximum(pdfMaxTB * fact);		// this may be in inches or cm
	ui.sbGutterMargin->setValue(gutterMargin * fact);

	ui.cbPdfPaperSize->setCurrentIndex(pdfIndex);
	ui.cbPdfUnit->setCurrentIndex(unitToIndex(pdfUnitIndex) );

	switch (pdfDpi)
	{
		case 0: ui.rb300->setChecked(true); break;
		case 1: ui.rb600->setChecked(true); break;
		case 2: 
		default:ui.rb1200->setChecked(true); break;
	}

	switch (whatToDo)
	{
		case wtdExportPdf:
				ui.btnOk->setText("&Eport Pdf");
				ui.gbPrinter->setVisible(false);
			break;
		case wtdPageSetup:		// page setup needs printer to set printable page size
				ui.gbPDFRes->setVisible(false);
		case wtdPrint:
		default:
			ui.gbPDFRes->setVisible(false);
			QStringList sl = QPrinterInfo::availablePrinterNames();
			ui.cbPrinterSelect->addItems(sl);
			if (actPrinterName.isEmpty())
			{
				QPrinterInfo pdf = QPrinterInfo::defaultPrinter();
				if (!pdf.isNull())
					ui.cbPrinterSelect->setCurrentText(pdf.printerName());
			}
			else
				ui.cbPrinterSelect->setCurrentText(actPrinterName);
		break;
	}
	_busy = false;
	FBSettings::Close();
}

void PageSetupDialog::_SaveParams()
{
	QSettings *s = FBSettings::Open();
	s->setValue("resi", ui.cbScreenResolution->currentIndex());
	s->setValue("useri", ui.rbUseResInd->isChecked());

	s->setValue("hpxs", ui.sbHorizPixels->value());
	s->setValue("sdiag", ui.edtScreenDiag->text());
	s->setValue("pflags", flags);
	// for PDF
	s->setValue("pdfpgs", pdfIndex);
	s->setValue("pdfmlr", hMargin);
	s->setValue("pdfmtb", vMargin);
	s->setValue("pdfgut", gutterMargin);
	s->setValue("pdfui" , unitToIndex(pdfUnitIndex));
	s->setValue("pdfdpi", pdfDpi);
	FBSettings::Close();
}

PageSetupDialog::~PageSetupDialog()
{
}

int PageSetupDialog::_GetIndexForHorizPixels(int hp)
{
	for (size_t i = 0; i < sizeof(myScreenSizes) / sizeof(MyScreenSizes); ++i)
		if (myScreenSizes[i].w == hp)
			return i;
	return -1;
}

void PageSetupDialog::on_cbScreenResolution_currentIndexChanged(int i) 
{ 
	if (_busy)
		return;
	resolutionIndex = i;
	_busy = true;
	if (i >= 0)
		ui.sbHorizPixels->setValue(myScreenSizes[i].w);
	_busy = false;
}

void PageSetupDialog::on_sbHorizPixels_valueChanged(int val)
{
	if (_busy)
		return;
	_busy = true;
	int ix = _GetIndexForHorizPixels(val);
	resolutionIndex = ix;
	ui.cbScreenResolution->setCurrentIndex(ix);
	_busy = false;
}


void PageSetupDialog::on_edtScreenDiag_textChanged(QString& txt) { screenDiagonal = txt.toInt(); }
void PageSetupDialog::on_cbUnit_currentIndexChanged(int i) { unitIndex = getUnit(i); }
void PageSetupDialog::on_cbOrientation_currentIndexChanged(int landscape) 
{ 
	flags &= ~pfLandscape; 
	if (landscape) 
		flags |= pfLandscape; 
}

void PageSetupDialog::on_chkPrintBackgroundImage_toggled(bool b)
{
	flags &= ~pfPrintBackgroundImage;
	if (b)
		flags |= pfPrintBackgroundImage;
}

void PageSetupDialog::on_chkWhiteBackground_toggled(bool b)
{
	flags &= ~pfWhiteBackground;
	if(b)
		flags |= pfWhiteBackground;
}

void PageSetupDialog::on_chkGrayscale_toggled(bool b)
{
	flags &= ~pfGrayscale;
	if (b)
		flags |= pfGrayscale;
}

void PageSetupDialog::on_chkGrid_toggled(bool b)
{
	flags &= ~pfGrid;
	if (b)
		flags |= pfGrid;
}

void PageSetupDialog::on_chkDontPrintImages_toggled(bool b)
{
	flags &= ~pfDontPrintImages;
	if (b)
	{
		flags |= pfDontPrintImages;
		ui.chkPrintBackgroundImage->setChecked(false);
	}
}

void PageSetupDialog::on_btnOk_clicked()
{
	_SaveParams();
	accept();
//	setResult(QDialog::Accepted);
//	close();
}
void PageSetupDialog::on_cbPdfPaperSize_currentIndexChanged(int i)
{
	pdfIndex = i;
	_ChangePdfPaperSize();
}

void PageSetupDialog::on_rbUseResInd_toggled(bool b)
{
	ui.sbHorizPixels->setEnabled(!b);
	ui.cbScreenResolution->setEnabled(b);
	useResInd = b;
}

void PageSetupDialog::on_rb300_toggled(bool b)
{
	if(b)
		pdfDpi = 0;
}
void PageSetupDialog::on_rb600_toggled(bool b)
{
	if (b)
		pdfDpi = 1;
}
void PageSetupDialog::on_rb1200_toggled(bool b)
{
	if (b)
		pdfDpi = 2;
}

void PageSetupDialog::_ChangePdfPaperSize()	
{
	if (_busy)
		return;

	double factM = pdfUnitIndex == uiInch ? 1.0 : 2.54;	// if 0: numbers are in cm, if 1 in inches
	if (pdfIndex >= 0)
	{
		pdfWidth  = myPageSizes[pdfIndex].w;
		pdfHeight = myPageSizes[pdfIndex].h;
		pdfMaxLR  = pdfWidth  / 3.0;
		pdfMaxTB  = pdfHeight / 3.0;
		if (hMargin > pdfMaxLR)
		{
			hMargin = pdfMaxLR;
			_busy = true;
			ui.sbMarginLR->setValue(pdfMaxLR * factM);
			_busy = false;

		}
		if (vMargin > pdfMaxTB)
		{
			vMargin = pdfMaxTB;
			_busy = true;
			ui.sbMarginTB->setValue(pdfMaxTB * factM);
			_busy = false;

		}
		ui.sbMarginLR->setMaximum( pdfMaxLR * factM );
		ui.sbMarginTB->setMaximum( pdfMaxTB * factM );
	}
}
void PageSetupDialog::_ChangeprintMarginsUnit()
{
	if (_busy)
		return;
	// unit change does not change the stored values
	double factM = pdfUnitIndex == uiInch ? 2.54 : 1;
	ui.sbMarginLR->setMaximum( pdfMaxLR * factM );
	ui.sbMarginTB->setMaximum( pdfMaxTB * factM );
	ui.sbMarginLR->setValue( hMargin * factM );
	ui.sbMarginTB->setValue( vMargin * factM );
}


void PageSetupDialog::on_cbPdfUnit_currentIndexChanged(int i)
{
	if (_busy)
		return;
	pdfUnitIndex = getUnit(i);
	_ChangeprintMarginsUnit();
}

void PageSetupDialog::on_cbPrinterSelect_currentIndexChanged(int i)
{
	actPrinterName.clear();
	if (i >= 0)
	{
		actPrinterName = ui.cbPrinterSelect->itemText(i);
		ui.lblPrinter->setText(actPrinterName);
	}
}

void PageSetupDialog::on_sbMarginLR_valueChanged(double val)
{
	if (pdfUnitIndex == uiCm)
		val /= 2.54;
	hMargin = val;	// always in inches
}

void PageSetupDialog::on_sbMarginTB_valueChanged(double val)
{
	if (pdfUnitIndex == uiCm)
		val /= 2.54;
	vMargin = val;	// always in inches
}

void PageSetupDialog::on_sbGutterMargin_valueChanged(double val)
{
	if (pdfUnitIndex == uiCm)
		val /= 2.54;
	gutterMargin = val;		// always in inches
}
