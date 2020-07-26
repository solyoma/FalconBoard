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


PageSetupDialog::PageSetupDialog(QWidget* parent, QString actP) : actPrinter(actP), QDialog(parent)
{
	ui.setupUi(this);

	ui.edtScreenDiag->setValidator(new QIntValidator(1, 200, this));

	QSettings s("MyWhiteboard.ini", QSettings::IniFormat);
	resolutionIndex = s.value("resi", 6).toInt();		// 1920 x 1080
	screenDiagonal	= s.value("sdiag", 24).toInt();		// inch
	unitFactor		= s.value("uf", 0).toInt();			// multipl. factor for number in edScreenDiag number to inch
	orientation		= s.value("orient", 0).toInt();		// portrait

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
	s.setValue("sdiag", ui.edtScreenDiag->text());
	s.setValue("uf", ui.cbUnit->currentIndex());
	s.setValue("orient", ui.cbOrientation->currentIndex());
}

void PageSetupDialog::on_cbScreenResolution_currentIndexChanged(int i) { resolutionIndex = i; }
void PageSetupDialog::on_edtScrenDiag_textChanged(QString& txt) { screenDiagonal = txt.toInt(); }
void PageSetupDialog::on_cbUnit_currentIndexChanged(int i) { unitFactor = i; }
void PageSetupDialog::on_cbOrientation_currentIndexChanged(int i) { orientation = i; }
void PageSetupDialog::on_cbPrinterSelect_currentIndexChanged(int i)
{
	if (i >= 0)
	{
		actPrinter = ui.cbPrinterSelect->itemText(i);
		ui.lblPrinter->setText(actPrinter);
	}
}
