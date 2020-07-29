#include <QSpinBox>
#include <QLabel>
#include <QScreen>
#include <QPrinter>
#include <QSettings>

#if defined(QT_PRINTSUPPORT_LIB)
	#include <QtPrintSupport/qtprintsupportglobal.h>
	#if QT_CONFIG(printdialog)
		#include <QPrinter>
		#include <QPrintDialog>
		#include <QPrinterInfo>
	#endif
#endif

#include "pagesetup.h"


int PageSetupDialog::_w[] = { 3840,
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
int PageSetupDialog::_h[] = { 2160,
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

int PageSetupDialog::_GetScreenSize(int index, QSize& size)
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

int PageSetupDialog::GetScreenSize(QSize& size)
{
	return _GetScreenSize(resolutionIndex, size);
}

PageSetupDialog::PageSetupDialog(QWidget* parent, QString actP) : actPrinter(actP), QDialog(parent)
{
	ui.setupUi(this);

	ui.edtScreenDiag->setValidator(new QIntValidator(1, 200, this));

	QSettings s("MyWhiteboard.ini", QSettings::IniFormat);
	resolutionIndex = s.value("resi", 6).toInt();		// 1920 x 1080
	horizPixels     = s.value("hpxs", 1920).toInt();
	screenDiagonal	= s.value("sdiag", 24).toInt();		// inch
	unitFactor		= s.value("uf", 0).toInt();			// multipl. factor for number in edScreenDiag number to inch
	orientation		= s.value("porient", 0).toInt();		// portrait
	flags			= s.value("pflags", 0).toInt();		// bit): print background image, bit 1: white background

	ui.cbScreenResolution->setCurrentIndex(resolutionIndex);
	ui.edtScreenDiag->setText(QString().setNum(screenDiagonal)) ;
	ui.cbUnit->setCurrentIndex(unitFactor);
	ui.cbOrientation->setCurrentIndex(orientation);

	QStringList sl = QPrinterInfo::availablePrinterNames();
	ui.cbPrinterSelect->addItems(sl);
	if (!actPrinter.isEmpty())
		ui.cbPrinterSelect->setCurrentText(actPrinter);
}

PageSetupDialog::~PageSetupDialog()
{
	QSettings s("MyWhiteboard.ini", QSettings::IniFormat);
	s.setValue("resi", ui.cbScreenResolution->currentIndex());			
	s.setValue("hpxs", ui.sbHorizPixels->value());
	s.setValue("sdiag", ui.edtScreenDiag->text());
	s.setValue("uf", ui.cbUnit->currentIndex());
	s.setValue("porient", ui.cbOrientation->currentIndex());
	s.setValue("pflags", flags);
}

int PageSetupDialog::_GetIndexForHorizPixels(int hp)
{
	for (int i = 0; i < sizeof(_w) / sizeof(int); ++i)
		if (_w[i] == hp)
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
		ui.sbHorizPixels->setValue(_w[i]);
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
void PageSetupDialog::on_cbUnit_currentIndexChanged(int i) { unitFactor = i; }
void PageSetupDialog::on_cbOrientation_currentIndexChanged(int i) { orientation = i; }
void PageSetupDialog::on_cbPrinterSelect_currentIndexChanged(int i)
{
	actPrinter.clear();
	if (i >= 0)
	{
		actPrinter = ui.cbPrinterSelect->itemText(i);
		ui.lblPrinter->setText(actPrinter);
	}
	ui.btnSetupPrinter->setEnabled(!actPrinter.isEmpty());
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
