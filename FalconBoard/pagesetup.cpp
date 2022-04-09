#include <math.h>

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

/// <summary>
///  screen sizes
/// </summary>
QSize PageSetupDialog::_pgsize[] = {
										{2840,2160},
										{3440,1440},
										{2560,1440},
										{2560,1080},
										{2048,1152},
										{1920,1200},
										{1920,1080},
										{1680,1050},
										{1600, 900},
										{1536, 864},
										{1440, 900},
										{1366, 768},
										{1360, 768},
										{1280,1024},
										{1280, 800},
										{1280, 720},
										{1024, 768},
										{800 , 600},
										{640 , 360}
};
int PageSetupDialog::_GetScreenSize(int index, QSize& size)
{
	if (index >= 0)
		size = _pgsize[index];
	else
		size = QSize(horizPixels, -1);

	return horizPixels;
}

int PageSetupDialog::GetScreenSize(QSize& size)
{
	return _GetScreenSize(resolutionIndex, size);
}

PageSetupDialog::PageSetupDialog(QWidget* parent, QString actP) : _actPrinterName(actP), QDialog(parent)
{
	ui.setupUi(this);
	_busy = true;

	ui.edtScreenDiag->setValidator(new QIntValidator(1, 200, this));

	QSettings s = FBSettings::Open();
	resolutionIndex = s.value("resi", 6).toInt();		// 1920 x 1080
	horizPixels     = s.value("hpxs", 1920).toInt();
	screenDiagonal	= s.value("sdiag", 24).toInt();		// inch
	unitIndex		= s.value("uf", 0).toInt();			// index to determines the multipl. factor for number in edScreenDiag number to inch
	flags			= s.value("pflags", 0).toInt();		// bit): print background image, bit 1: white background

	ui.cbScreenResolution->setCurrentIndex(resolutionIndex);
	ui.edtScreenDiag->setText(QString().setNum(screenDiagonal)) ;
	ui.cbUnit->setCurrentIndex(unitIndex);
	ui.cbOrientation->setCurrentIndex(flags & pfLandscape ? 1 : 0);
	ui.chkWhiteBackground->setChecked(flags & pfWhiteBackground);
	ui.chkGrayscale->setChecked(flags & pfGrayscale);
	ui.chkPrintBackgroundImage->setChecked(flags & pfPrintBackgroundImage);
	ui.chkGrid->setChecked(flags & pfGrid);
	ui.chkDontPrintImages->setChecked(flags & pfDontPrintImages);

	QStringList sl = QPrinterInfo::availablePrinterNames();
	ui.cbPrinterSelect->addItems(sl);
	if (_actPrinterName.isEmpty())
	{
		QPrinterInfo pdf = QPrinterInfo::defaultPrinter();
		if (!pdf.isNull())
			ui.cbPrinterSelect->setCurrentText(pdf.printerName());
	}
	else
		ui.cbPrinterSelect->setCurrentText(_actPrinterName);

	_busy = false;
}

void PageSetupDialog::on_okButton_clicked()
{
	_SaveParams();

	accept();
}

void PageSetupDialog::_SaveParams()
{
	QSettings s = FBSettings::Open();
	s.setValue("resi", resolutionIndex);
	s.setValue("hpxs", ui.sbHorizPixels->value());
	s.setValue("sdiag", screenDiagonal);
	s.setValue("pflags", flags);
	s.setValue("ui", unitIndex);
}

PageSetupDialog::~PageSetupDialog()
{
}

int PageSetupDialog::_GetIndexForHorizPixels(int hp)
{
	for (size_t i = 0; i < sizeof(_pgsize) / sizeof(QSize); ++i)
		if (_pgsize[i].width() == hp)
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
		ui.sbHorizPixels->setValue(_pgsize[i].width());
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
void PageSetupDialog::on_cbUnit_currentIndexChanged(int i) 
{ 
	if (_busy)
		return;

	static double fact[] = { 1.0, 2.54, 25.4 };
	_busy = true;
	double diag = ui.edtScreenDiag->text().toDouble() / fact[unitIndex];	// back to inches
	unitIndex = i;
	diag *= fact[unitIndex];
	ui.edtScreenDiag->setText(QString().setNum(int(round(diag))));
	_busy = false;
}
void PageSetupDialog::on_cbOrientation_currentIndexChanged(int landscape) { flags &= ~pfLandscape; if (landscape) flags |= pfLandscape; }
void PageSetupDialog::on_cbPrinterSelect_currentIndexChanged(int i)
{
	_actPrinterName.clear();
	if (i >= 0)
	{
		_actPrinterName = ui.cbPrinterSelect->itemText(i);
		ui.lblPrinter->setText(_actPrinterName);
	}
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

