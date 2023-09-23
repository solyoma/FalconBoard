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
QString PenColorsDialog::_makeStyle (int i, int dark)
{
	QString style;

	style = QString("background-color:%1").arg(_colors[i][dark].name());
	return style;
};


PenColorsDialog::~PenColorsDialog()
{
}

void PenColorsDialog::_SelectColor(int row, int dark)
{
	QColor newc = QColorDialog::getColor(_colors[row][dark], this, QString(tr("FalconBoard - Select new pen color for Color #%1")).arg(row + 2));
	if (newc.isValid())
	{
		_colors[row][dark] = newc;
		pb[row][dark]->setStyleSheet(_makeStyle(row, dark) );
	}
}
// color names
//void PenColorsDialog::on_edtL2_textChanged(const QChar &txt)
//{
//}
//void PenColorsDialog::on_edtL3_textChanged(const QChar &txt)
//{
//}
//void PenColorsDialog::on_edtL4_textChanged(const QChar &txt)
//{
//}
//void PenColorsDialog::on_edtL5_textChanged(const QChar &txt)
//{
//}
//void PenColorsDialog::on_edtD2_textChanged(const QChar &txt)
//{
//}
//void PenColorsDialog::on_edtD3_textChanged(const QChar &txt)
//{
//}
//void PenColorsDialog::on_edtD4_textChanged(const QChar &txt)
//{
//}
//void PenColorsDialog::on_edtD5_textChanged(const QChar &txt)
//{
//}

// button clicks
void PenColorsDialog::on_btnL2_clicked()
{
	_SelectColor(0, 0);
}
void PenColorsDialog::on_btnL3_clicked()
{
	_SelectColor(1, 0);
}
void PenColorsDialog::on_btnL4_clicked()
{
	_SelectColor(2, 0);
}
void PenColorsDialog::on_btnL5_clicked()
{
	_SelectColor(3, 0);
}
void PenColorsDialog::on_btnD2_clicked()
{
	_SelectColor(0, 1);
}
void PenColorsDialog::on_btnD3_clicked()
{
	_SelectColor(1, 1);
}
void PenColorsDialog::on_btnD4_clicked()
{
	_SelectColor(2, 1);
}
void PenColorsDialog::on_btnD5_clicked()
{
	_SelectColor(3, 1);
}
