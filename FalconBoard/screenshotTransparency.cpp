#include <QString>
#include <QColorDialog>

#include "screenshotTransparency.h"

ScreenShotTransparencyDialog::ScreenShotTransparencyDialog(QWidget* parent, QColor trcolor, bool usetr) : _trcolor(trcolor)
{
	ui.setupUi(this);
	ui.chkUse->setChecked(usetr);
		if (!_trcolor.isValid())
		_trcolor = "white";
// DEBUG
//	QString s = _trcolor.name();
	ui.btnColor->setStyleSheet(QString("background-color:%1;border-color:%2").arg(_trcolor.name()).arg("black"));
}

void ScreenShotTransparencyDialog::on_btnColor_clicked()
{
	QColor clr = QColorDialog::getColor(_trcolor, this, "FalconG - Select Color");
	if (clr.isValid())
	{
		_trcolor = clr;
		QString styleSheet = QString("background-color:%1;border-color:%2").arg(clr.name()).arg("black");
		ui.btnColor->setStyleSheet(styleSheet);
	}
}

void ScreenShotTransparencyDialog::GetResult(QColor& trcolor, bool& usetr)
{
	trcolor = _trcolor;
	usetr = _trcolor.isValid() ? ui.chkUse->isChecked() : false;
}
