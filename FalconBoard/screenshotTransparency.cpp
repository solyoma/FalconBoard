#include <QString>

#include <QColorDialog>

#include "screenshotTransparency.h"

const QString ssFormat = QStringLiteral("QToolButton#btnColor {\nbackground-color:%1;\nborder-color:%2;color:%3\n}");

ScreenShotTransparencyDialog::ScreenShotTransparencyDialog(QWidget* parent, QColor trcolor, bool usetr) : _trcolor(trcolor)
{
	ui.setupUi(this);
	ui.chkUse->setChecked(usetr);
		if (!_trcolor.isValid())
			_trcolor = "white";

	QString s = _trcolor.name() == "#ffffff" ? "black":"white";
	ui.btnColor->setStyleSheet(QString(ssFormat).arg(_trcolor.name()).arg("black").arg(s));
}

void ScreenShotTransparencyDialog::on_btnColor_clicked()
{
	QColor clr = QColorDialog::getColor(_trcolor, this, "FalconG - Select Color");
	if (clr.isValid())
	{
		_trcolor = clr;
		QString s = _trcolor.name() == "#ffffff" ? "black":"white";
		QString styleSheet = QString(ssFormat).arg(clr.name()).arg("black").arg(s);
		ui.btnColor->setStyleSheet(styleSheet);
	}
}

void ScreenShotTransparencyDialog::GetResult(QColor& trcolor, bool& usetr)
{
	trcolor = _trcolor;
	usetr = _trcolor.isValid() ? ui.chkUse->isChecked() : false;
}
