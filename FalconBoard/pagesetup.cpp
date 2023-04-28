#include <QSpinBox>
#include <QLabel>
#include <QScreen>
#include <QPrinter>
#include <QSettings>

#include <QtPrintSupport/qtprintsupportglobal.h>
#include <QPrinter>
#include <QPrintDialog>
#include <QPrinterInfo>

#include "config.h"
#include "common.h"
#include "pagesetup.h"


// static fields (one pageSetupDialog for the application)
int		PageParams::resolutionIndex;		// screen resolution in resolution combobox (pixels)
int		PageParams::paperId = 3;            // default: A4
int		PageParams::horizPixels;			// horizontal pixel count
bool	PageParams::useResInd = true;		// use resolution index or horizPixels?
int		PageParams::screenDiagonal;			// from text in 'edtScreenDiag' in inches
int		PageParams::screenPageWidth = 1920;      // width of page on screen in pixels - used to calculate pixel limits for pages (HD: 1920 x 1080)
int		PageParams::screenPageHeight = 1920 * 3508 / 2480;  // height of screen for selected paper size in pixels - for A4 (210mm x 297mm, 8.27 in x 11.7 in) with 300 dpi
unsigned PageParams::flags;					// ORed from PrinterFlags (common.h)
			// for PDF
int		PageParams::pdfIndex;				// in page size combo box
double	PageParams::pdfWidth, 			  	// page size in inches
		PageParams::pdfHeight,
		PageParams::hMargin,				// left and right in inches
		PageParams::vMargin,				// top and bottom -"-
		PageParams::gutterMargin;			// left on odd, right on even pages inches
bool	PageParams::_isLoaded = false;		// so we load it only once
int		PageParams::pdfDpiIndex;					// 0,1,2: dots / inch = (pdfDpi * 300 + 300)

QString   PageParams::actPrinterName;		// if not this is its name

PageParams::UnitIndex PageParams::unitIndex;		// index of units for margins (in ini file 0: inch, 1: cm, 2: mm)
PageParams::UnitIndex PageParams::pdfUnitIndex;


auto PageParams::GetUnit(int ix)
{ 
	static PageParams::UnitIndex ui[] = { PageParams::uiInch,PageParams::uiCm, PageParams::uiMm };
	return ui[ix]; 
}

auto PageParams::UnitToIndex(PageParams::UnitIndex ui)
{ 
	return ui == PageParams::uiInch ? 0 : ui == PageParams::uiCm ? 1 : 2;
}

void PageParams::Load()
{
	if (_isLoaded)
		return;

	QSettings *s	= FBSettings::Open();

	s->beginGroup(PSETUP);

	resolutionIndex = s->value(RESI, 6).toInt();					// 1920 x 1080
	horizPixels     = s->value(HPXS, 1920).toInt();					// this many pixels in the width of one page
	useResInd		= s->value(USERI, true).toBool();
	screenDiagonal	= s->value(SDIAG, 24).toInt();					// inch
	unitIndex		= GetUnit(s->value(UNITINDEX, 0).toInt());		// index to determines the multipl. factor for number in edScreenDiag number to inch
	flags			= s->value(PFLAGS, 0).toInt();					// ORed PrinterFlags
	pdfIndex		= s->value(PDFPGS, 3).toInt();					// index in paper size combo box (default: A4)
	pdfDpiIndex		= s->value(PDFDPI, 0).toInt();				// index: 0: 300, 1: 600, 2: 1200 dpi
	pdfUnitIndex	= GetUnit(s->value(PDFUI, 0).toInt() );
	hMargin			= s->value(PDFMLR, 1.0).toFloat();				// left - right	in inches
	vMargin			= s->value(PDFMTB, 1.0).toFloat();				// top - bottom	in inches
	gutterMargin	= s->value(PDFGUT, 0.0).toFloat();				// gutter in inches

	s->endGroup();

	_isLoaded = true;

	FBSettings::Close();
}

void PageParams::Save()
{
	QSettings *s = FBSettings::Open();
	s->beginGroup(PSETUP);

	s->setValue(RESI, resolutionIndex);
	s->setValue(USERI, useResInd);

	s->setValue(HPXS, horizPixels);
	s->setValue(SDIAG, screenDiagonal);
	s->setValue(PFLAGS, flags);	// includes pfOpenPDFInViewer
	// for PDF
	s->setValue(PDFPGS, pdfIndex);
	s->setValue(PDFMLR, hMargin);
	s->setValue(PDFMTB, vMargin);
	s->setValue(PDFGUT, gutterMargin);
	s->setValue(PDFUI , UnitToIndex(pdfUnitIndex));
	s->setValue(PDFDPI, pdfDpiIndex);

	s->endGroup();

	FBSettings::Close();
}

// --------------------------------------------------------

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
		size = QSize(PageParams::horizPixels, -1);

	return PageParams::horizPixels;
}

int PageSetupDialog::GetScreenSize(QSize& size)
{
	return _GetScreenSize(PageParams::resolutionIndex, size);
}

/*======== conversion ========*/

PageSetupDialog::PageSetupDialog(QWidget* parent, PageParams::PageSetupType toDo) :  QDialog(parent)
{
	whatToDo = toDo;
	ui.setupUi(this);
	_busy = true;

	ui.edtScreenDiag->setValidator(new QIntValidator(1, 200, this));

	ui.cbScreenResolution->setCurrentIndex(PageParams::resolutionIndex);


	ui.rbUseResInd->setChecked(PageParams::useResInd);
	ui.cbScreenResolution->setEnabled(PageParams::useResInd);
	ui.rbUseHorPixels->setChecked(!PageParams::useResInd);
	ui.sbHorizPixels->setEnabled(!PageParams::useResInd);

	ui.edtScreenDiag->setText(QString().setNum(PageParams::screenDiagonal)) ;

	ui.cbUnit->setCurrentIndex(PageParams::unitIndex);


	ui.chkOpenPDFInViewer->setChecked(PageParams::flags & openPdfViewerFlag);
	ui.cbOrientation->setCurrentIndex(PageParams::flags & pageOrientationFlag ? 1 : 0); // 1: landscape
	ui.chkWhiteBackground->setChecked(PageParams::flags & printWhiteBackgroundFlag);
	ui.chkGrayscale->setChecked(PageParams::flags & printGrayScaleFlag);
	ui.chkPrintBackgroundImage->setChecked(PageParams::flags & printBackgroundImageFlag);
	ui.chkGrid->setChecked(PageParams::flags & printGridFlag);
	ui.chkDontPrintImages->setChecked(PageParams::flags & printNoImagesFlag);

	ui.chkPageNumbers->setChecked(PageParams::flags & pageNumberFlagUsePageNumber);
	ui.rbPgBottom->setChecked(PageParams::flags & pageNumberFlagTopBottom);
	ui.rbPgTop->setChecked((PageParams::flags & pageNumberFlagTopBottom) == 0);
	ui.rbPgCenter->setChecked((PageParams::flags & pageNumberFlagLeftCenterRight) == 0);
	ui.rbPgLeft->setChecked(PageParams::flags & pageNumberBitsForLeft);
	ui.rbPgRight->setChecked(PageParams::flags & pageNumberBitsForRight);

	// for PDF

	PageParams::pdfWidth  = myPageSizes[PageParams::pdfIndex].w;					// w.o. margins in inches
	PageParams::pdfHeight = myPageSizes[PageParams::pdfIndex].h;

	pdfMaxLR = myPageSizes[PageParams::pdfIndex].w / 3.0;				// always stored in inches
	pdfMaxTB = myPageSizes[PageParams::pdfIndex].h / 3.0;				// always stored in inches


	float fact = PageParams::pdfUnitIndex == PageParams::uiInch ? 1.0 : 2.54;				// to convert cm-s to inches

	ui.sbMarginLR->setMaximum(pdfMaxLR*fact);		// this may be in inches or cm
	ui.sbMarginLR->setValue(PageParams::hMargin*fact);

	ui.sbMarginTB->setMaximum(pdfMaxTB*fact);		// this may be in inches or cm
	ui.sbMarginTB->setValue(PageParams::vMargin*fact);

	ui.sbGutterMargin->setMaximum(pdfMaxTB * fact);		// this may be in inches or cm
	ui.sbGutterMargin->setValue(PageParams::gutterMargin * fact);

	ui.cbPdfPaperSize->setCurrentIndex(PageParams::pdfIndex);
	ui.cbPdfUnit->setCurrentIndex(PageParams::UnitToIndex(PageParams::pdfUnitIndex) );

	switch (PageParams::pdfDpiIndex)
	{
		case 0: ui.rb300->setChecked(true); break;
		case 1: ui.rb600->setChecked(true); break;
		case 2: 
		default:ui.rb1200->setChecked(true); break;
	}

	switch (whatToDo)
	{
		case PageParams::wtdExportPdf:
				ui.btnOk->setText("&Eport Pdf");
				ui.gbPrinter->setVisible(false);
			break;
		case PageParams::wtdPageSetup:		// page setup needs printer to set printable page size
				ui.gbPDFRes->setVisible(false);
				ui.chkOpenPDFInViewer->setVisible(false);
		case PageParams::wtdPrint:
		default:
			ui.gbPDFRes->setVisible(false);
			QStringList sl = QPrinterInfo::availablePrinterNames();
			ui.cbPrinterSelect->addItems(sl);
			if (PageParams::actPrinterName.isEmpty())
			{
				QPrinterInfo pdf = QPrinterInfo::defaultPrinter();
				if (!pdf.isNull())
					ui.cbPrinterSelect->setCurrentText(pdf.printerName());
			}
			else
				ui.cbPrinterSelect->setCurrentText(PageParams::actPrinterName);
		break;
	}
	_busy = false;
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
	PageParams::resolutionIndex = i;
	_busy = true;
	if (i >= 0)
		ui.sbHorizPixels->setValue(myScreenSizes[i].w);
	_changed = true;
	_busy = false;
}

void PageSetupDialog::on_sbHorizPixels_valueChanged(int val)
{
	if (_busy)
		return;
	_busy = true;
	int ix = _GetIndexForHorizPixels(val);
	PageParams::resolutionIndex = ix;
	ui.cbScreenResolution->setCurrentIndex(ix);
	_changed = true;
	_busy = false;
}

void PageSetupDialog::on_edtScreenDiag_textChanged(const QString& txt) 
{ 
	static int prevind = 0;
	int actInd = ui.cbUnit->currentIndex();
	if (prevind == actInd)		// otherwise the text is changed because the unit changed
		PageParams::screenDiagonal = txt.toInt() * (actInd == 0 ? 1.0 : (actInd == 1 ? 1 / 2.54 : 1 / 25.4));			// and this value is always in inch
	_changed = true;
	prevind = actInd;
}
void PageSetupDialog::on_cbUnit_currentIndexChanged(int i) 
{ 
	PageParams::unitIndex = PageParams::GetUnit(i);
	ui.edtScreenDiag->setText(QString().setNum(PageParams::screenDiagonal * (PageParams::unitIndex == PageParams::uiInch ? 1.0 : (PageParams::unitIndex == PageParams::uiCm ? 2.54 : 25.4))));
	_changed = true;
}
void PageSetupDialog::on_cbOrientation_currentIndexChanged(int landscape) 
{ 
	PageParams::flags &= ~pageOrientationFlag;
	if (landscape) 
		PageParams::flags |= pageOrientationFlag;
	_changed = true;
}

void PageSetupDialog::on_chkPrintBackgroundImage_toggled(bool b)
{
	PageParams::flags &= ~printBackgroundImageFlag;
	if (b)
		PageParams::flags |= printBackgroundImageFlag;
	_changed = true;
}

void PageSetupDialog::on_chkWhiteBackground_toggled(bool b)
{
	PageParams::flags &= ~printWhiteBackgroundFlag;
	if(b)
		PageParams::flags |= printWhiteBackgroundFlag;
	_changed = true;
}

void PageSetupDialog::on_chkGrayscale_toggled(bool b)
{
	PageParams::flags &= ~printGrayScaleFlag;
	if (b)
		PageParams::flags |= printGrayScaleFlag;
	_changed = true;
}

void PageSetupDialog::on_chkGrid_toggled(bool b)
{
	PageParams::flags &= ~printGridFlag;
	if (b)
		PageParams::flags |= printGridFlag;
	_changed = true;
}

void PageSetupDialog::on_chkDontPrintImages_toggled(bool b)
{
	PageParams::flags &= ~printNoImagesFlag;
	if (b)
	{
		PageParams::flags |= printNoImagesFlag;
		ui.chkPrintBackgroundImage->setChecked(false);
	}
	_changed = true;
}

void PageSetupDialog::on_chkOpenPDFInViewer_toggled(bool b)
{
	PageParams::flags &= ~openPdfViewerFlag;
	if (b)
	{
		PageParams::flags |= openPdfViewerFlag;
		ui.chkPrintBackgroundImage->setChecked(false);
	}
	_changed = true;
}

void PageSetupDialog::on_chkPageNumbers_toggled(bool b)
{
	if (_busy)
		return;
	unsigned flg = PageParams::flags & ~pageNumberFlagUsePageNumber;
	if (b)
		flg |= pageNumberFlagUsePageNumber;
	PageParams::flags = flg;
	_changed = true;
}
	// save INI file when OK clicked
void PageSetupDialog::on_btnOk_clicked()
{
	if(_changed )
		PageParams::Save();
	accept();
}
void PageSetupDialog::on_cbPdfPaperSize_currentIndexChanged(int i)
{
	PageParams::pdfIndex = i;
	_ChangePdfPaperSize();
	_changed = true;
}

void PageSetupDialog::on_rbUseResInd_toggled(bool b)
{
	ui.sbHorizPixels->setEnabled(!b);
	ui.cbScreenResolution->setEnabled(b);
	PageParams::useResInd = b;
	_changed = true;
}

void PageSetupDialog::on_rb300_toggled(bool b)
{
	if(b)
		PageParams::pdfDpiIndex = 0;
	_changed = true;
}
void PageSetupDialog::on_rb600_toggled(bool b)
{
	if (b)
		PageParams::pdfDpiIndex = 1;
	_changed = true;
}
void PageSetupDialog::on_rb1200_toggled(bool b)
{
	if (b)
		PageParams::pdfDpiIndex = 2;
	_changed = true;
}

void PageSetupDialog::on_rbPgBottom_toggled(bool b)
{
	if (_busy)	// special case: b -> top, !b -> bottom
		return;
	unsigned flg = PageParams::flags & ~pageNumberFlagTopBottom;
	if (b)
		flg |= pageNumberFlagTopBottom;
	PageParams::flags = flg;
	_changed = true;
}

void PageSetupDialog::on_rbPgLeft_toggled(bool b)
{
	if (_busy || !b)
		return;
	unsigned flg = PageParams::flags & ~pageNumberFlagLeftCenterRight;
	flg |= pageNumberBitsForLeft;
	PageParams::flags = flg;
	_changed = true;
}

void PageSetupDialog::on_rbPgCenter_toggled(bool b)
{
	if (_busy || !b)
		return;
	unsigned flg = PageParams::flags & ~pageNumberFlagLeftCenterRight;
	PageParams::flags = flg;	// clear all flags
	_changed = true;
}

void PageSetupDialog::on_rbPgRight_toggled(bool b)
{
	if (_busy || !b)
		return;
	unsigned flg = PageParams::flags & ~pageNumberFlagLeftCenterRight;
	flg |= pageNumberBitsForRight;
	PageParams::flags = flg;
	_changed = true;
}

void PageSetupDialog::_ChangePdfPaperSize()	
{
	if (_busy)
		return;

	double factM = PageParams::pdfUnitIndex == PageParams::uiInch ? 1.0 : 2.54;	// if 0: numbers are in cm, if 1 in inches
	if (PageParams::pdfIndex >= 0)
	{
		PageParams::pdfWidth  = myPageSizes[PageParams::pdfIndex].w;
		PageParams::pdfHeight = myPageSizes[PageParams::pdfIndex].h;
		pdfMaxLR  = PageParams::pdfWidth  / 3.0;
		pdfMaxTB  = PageParams::pdfHeight / 3.0;
		if (PageParams::hMargin > pdfMaxLR)
		{
			PageParams::hMargin = pdfMaxLR;
			_busy = true;
			ui.sbMarginLR->setValue(pdfMaxLR * factM);
			_busy = false;

		}
		if (PageParams::vMargin > pdfMaxTB)
		{
			PageParams::vMargin = pdfMaxTB;
			_busy = true;
			ui.sbMarginTB->setValue(pdfMaxTB * factM);
			_busy = false;

		}
		ui.sbMarginLR->setMaximum( pdfMaxLR * factM );
		ui.sbMarginTB->setMaximum( pdfMaxTB * factM );
	}
	_changed = true;
}
void PageSetupDialog::_ChangePrintMarginsUnit()
{
	if (_busy)
		return;
	// unit change does not change the stored values
	double factM = PageParams::pdfUnitIndex == PageParams::uiInch ? 1 : 2.54;
	ui.sbMarginLR->setMaximum( pdfMaxLR * factM );
	ui.sbMarginTB->setMaximum( pdfMaxTB * factM );
	ui.sbMarginLR->setValue( PageParams::hMargin * factM );
	ui.sbMarginTB->setValue( PageParams::vMargin * factM );
	_changed = true;
}


void PageSetupDialog::on_cbPdfUnit_currentIndexChanged(int i)
{
	if (_busy)
		return;
	PageParams::pdfUnitIndex = PageParams::GetUnit(i);
	_ChangePrintMarginsUnit();
	_changed = true;
}

void PageSetupDialog::on_cbPrinterSelect_currentIndexChanged(int i)
{
	PageParams::actPrinterName.clear();
	if (i >= 0)
	{
		PageParams::actPrinterName = ui.cbPrinterSelect->itemText(i);
		ui.lblPrinter->setText(PageParams::actPrinterName);
	}
	_changed = true;
}

void PageSetupDialog::on_sbMarginLR_valueChanged(double val)
{
	if (PageParams::pdfUnitIndex == PageParams::uiCm)
		val /= 2.54;
	PageParams::hMargin = val;	// always in inches
	_changed = true;
}

void PageSetupDialog::on_sbMarginTB_valueChanged(double val)
{
	if (PageParams::pdfUnitIndex == PageParams::uiCm)
		val /= 2.54;
	PageParams::vMargin = val;	// always in inches
	_changed = true;
}

void PageSetupDialog::on_sbGutterMargin_valueChanged(double val)
{
	if (PageParams::pdfUnitIndex == PageParams::uiCm)
		val /= 2.54;
	PageParams::gutterMargin = val;		// always in inches
	_changed = true;
}
