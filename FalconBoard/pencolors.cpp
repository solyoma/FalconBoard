#include "pencolors.h"
#include <QColorDialog>


PenColorsDialog::PenColorsDialog(QWidget* parent) : QDialog(parent)
{
	ui.setupUi(this);

	pe[0][0] = ui.edtL2; pe[0][1] = ui.edtD2;
	pe[1][0] = ui.edtL3; pe[1][1] = ui.edtD3;
	pe[2][0] = ui.edtL4; pe[2][1] = ui.edtD4;
	pe[3][0] = ui.edtL5; pe[3][1] = ui.edtD5;

	pb[0][0] = ui.btnL2; pb[0][1] = ui.btnD2;
	pb[1][0] = ui.btnL3; pb[1][1] = ui.btnD3;
	pb[2][0] = ui.btnL4; pb[2][1] = ui.btnD4;
	pb[3][0] = ui.btnL5; pb[3][1] = ui.btnD5;

	QString style;

	auto _makeStyle = [&](int i, int dark)->QString
		{
			style = QString("background-color:%1").arg(_colors[i][dark].name());
			return style;
		};

	for (int i = 0; i < PEN_COUNT - 2; ++i)
	{
		pe[i][0]->setText(globalDrawColors.ActionText((FalconPenKind)(i+2), 0)) ;
		pe[i][1]->setText(globalDrawColors.ActionText((FalconPenKind)(i+2), 1)) ;
		_colors[i][0] = globalDrawColors.Color((FalconPenKind)(i + 2), 0);
		pb[i][0]->setStyleSheet(_makeStyle(i, 0));
		_colors[i][1] = globalDrawColors.Color((FalconPenKind)(i + 2), 1);
		pb[i][1]->setStyleSheet(_makeStyle(i, 1));
	}
}

PenColorsDialog::~PenColorsDialog()
{

}

void PenColorsDialog::mousePressEvent(QMouseEvent* event)
{
}

void PenColorsDialog::keyPressEvent(QKeyEvent* event)
{
}

void PenColorsDialog::_SelectColor(int row, int dark)
{
	bool isDarkMode = globalDrawColors.IsDarkMode();
	globalDrawColors.SetDarkMode(dark);

	QColor newc = QColorDialog::getColor(_colors[row][dark], this, QString(tr("FalconBoard - Select new pen color for Color #%1")).arg(row + 2));
	if (newc.isValid())
	{
		_colors[row][dark] = newc;
		globalDrawColors.SetupPenAndCursor((FalconPenKind)row, _colors[row][0], _colors[row][1]);	// leave name the same
	}
	if((bool)dark != isDarkMode)
		globalDrawColors.SetDarkMode(isDarkMode);

}
