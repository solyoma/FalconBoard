#include <QSpinBox>
#include <QLabel>
#include <QScreen>
#include <QPainter>
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


PageSetupDialog::PageSetupDialog(QWidget* parent) : QDialog(parent)
{
	ui.setupUi(this);

	QSettings s("MyWhiteboard.ini", QSettings::IniFormat);

}

