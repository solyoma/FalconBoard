#include <QString>

#include <QColorDialog>

#include "screenshotTransparency.h"

const QString ssFormat = QStringLiteral("QToolButton#btnColor {\nbackground-color:%1;\nborder-color:%2;color:%3\n}");

ScreenShotTransparencyDialog::ScreenShotTransparencyDialog(QWidget* parent, QColor trcolor, bool usetr, qreal fuzzyness) : _trcolor(trcolor), _fuzzyness(fuzzyness)
{
	ui.setupUi(this);
	ui.chkUse->setChecked(usetr);
	if (!_trcolor.isValid())
		_trcolor = "white";

	ui.hsFuzzyness->setValue(_fuzzyness*100.0);

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

void ScreenShotTransparencyDialog::GetResult(QColor& trcolor, bool& usetr,qreal &fuzzyness)
{
	trcolor = _trcolor;
	usetr = _trcolor.isValid() ? ui.chkUse->isChecked() : false;
	fuzzyness = (qreal)ui.hsFuzzyness->value() / 100.0;	// between 0 and 1.0
}

void ScreenShotTransparencyDialog::on_hsFuzzyness_valueChanged(int value)
{
	ui.lblFuzzyness->setText(QString(tr("Fuzzyness: %1")).arg((qreal)value / 100.0, 7, 'g', 2));
}
