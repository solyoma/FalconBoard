#include <QColorDialog>
#include <QMessageBox>
#include "config.h"
#include "pencolorsdialog.h"
#include "pencolorset.h"

static const QString pgbase = "pen";

void PenColorsDialog::Data::SetupFrom(const DrawColors& dc, QString schemeName)
{
	title = schemeName;
	for (int i = 0; i < PEN_COUNT - 2; ++i)
	{
		names[i][0]  = dc.ActionText((FalconPenKind)(i + 2)	, ScreenMode::smLight);
		names[i][1]  = dc.ActionText((FalconPenKind)(i + 2)	, ScreenMode::smDark);
		colors[i][0] = dc.Color((FalconPenKind)(i + 2)		, ScreenMode::smLight);
		colors[i][1] = dc.Color((FalconPenKind)(i + 2)		, ScreenMode::smDark);
	}
}

PenColorsDialog::PenColorsDialog(QWidget* parent) : QDialog(parent)
{
	ui.setupUi(this);

	pe[0][0] = ui.edtL2; pe[0][1] = ui.edtD2;
	pe[1][0] = ui.edtL3; pe[1][1] = ui.edtD3;
	pe[2][0] = ui.edtL4; pe[2][1] = ui.edtD4;
	pe[3][0] = ui.edtL5; pe[3][1] = ui.edtD5;
	pe[4][0] = ui.edtL6; pe[4][1] = ui.edtD6;
	pe[5][0] = ui.edtL7; pe[5][1] = ui.edtD7;

	pb[0][0] = ui.btnL2; pb[0][1] = ui.btnD2;
	pb[1][0] = ui.btnL3; pb[1][1] = ui.btnD3;
	pb[2][0] = ui.btnL4; pb[2][1] = ui.btnD4;
	pb[3][0] = ui.btnL5; pb[3][1] = ui.btnD5;
	pb[4][0] = ui.btnL6; pb[4][1] = ui.btnD6;
	pb[5][0] = ui.btnL7; pb[5][1] = ui.btnD7;

	_ReadData(true);
	_SetupFrom(1);	// actual
}

PenColorsDialog::~PenColorsDialog()
{
}

bool PenColorsDialog::GetChanges(DrawColors& drcls) const
{
	int changeCount = 0;
	drcls.SetupPenAndCursor(penBlackOrWhite, drcls.Color(penBlackOrWhite, ScreenMode::smLight), drcls.Color(penBlackOrWhite, ScreenMode::smDark));
	for (int i = 0; i < PEN_COUNT - 2; ++i)
	{
		if ((_data.colors[i][0] != drcls.Color((FalconPenKind)(i + 2), ScreenMode::smLight)) ||
			(_data.colors[i][1] != drcls.Color((FalconPenKind)(i + 2), ScreenMode::smDark)))
		{
			drcls.SetupPenAndCursor((FalconPenKind)(i + 2), _data.colors[i][0], _data.colors[i][1], pe[i][0]->text(), pe[i][1]->text());
			++changeCount;
		}
	}
	return changeCount;
}

int PenColorsDialog::_ReadData(bool overWriteActual)
{
	_schemes.clear();	// fresh start
	if (ui.cbSelectScheme->count() > 2)
	{
		_busy = true;
		int i = ui.cbSelectScheme->count();
		while(i > 2)
			ui.cbSelectScheme->removeItem(--i);
		ui.cbSelectScheme->setCurrentIndex(1);	// to actual colors
		_busy = false;
	}

	// setup _schemes
	Data tmp;
	DrawColors dc;
	dc.Initialize();			// from defaults
	tmp.SetupFrom(dc, tr("Default pen colors"));
	_schemes.push_back(tmp);	// index #0

	tmp.SetupFrom(globalDrawColors, tr("Actual pen colors"));
	if (overWriteActual)
		_data = tmp;
	else
		tmp = _data;

	_schemes.push_back(tmp);	// index #1 - actual

	QSettings* s = FBSettings::Open();
	s->beginGroup(PENGROUP);
	int n = s->value(PENGROUPCOUNT, 0).toInt();
	int cntRead = 0;
	if (n)
	{
		for (int grp = 1; grp <= n; ++grp)
		{
			tmp.SetupFrom(dc, "dummy");	// default pen colors were not saved, nor restored
			// the others remain default
			tmp.title = s->value(FalconPens::ItemName(grp, 0, "t"), "").toString();
			if (!tmp.title.isEmpty())
			{
				++cntRead;
				for (int pen = 0; pen < PEN_COUNT-2; ++pen)
				{
					QString qs = s->value(FalconPens::ItemName(grp, pen+2, "c"), "").toString(), qs1, qs2;
					if (!qs.isEmpty())
					{
						FalconPens::Separate(qs, qs1, qs2);
						if (!qs1.isEmpty())
							tmp.colors[pen][0] = qs1;
						if (!qs2.isEmpty())
							tmp.colors[pen][1] = qs2;

						qs = s->value(FalconPens::ItemName(grp, pen+2, "n")).toString();
						FalconPens::Separate(qs, qs1, qs2);
						if (!qs1.isEmpty())
							tmp.names[pen][0] = qs1;
						if (!qs2.isEmpty())
							tmp.names[pen][1] = qs2;
					}
				}
				_schemes.push_back(tmp);
				ui.cbSelectScheme->addItem(tmp.title);
			}
		}
	}
	FBSettings::Close();
	return cntRead;
}

void PenColorsDialog::_SetupFrom(int i)
{
	if(i >=0)
		_data = _schemes[i];

	for (int j = 0; j < PEN_COUNT - 2; ++j)
	{
		pe[j][0]->setText(_data.names[j][0]);
		pb[j][0]->setStyleSheet(_makeStyle(j,0));

		pe[j][1]->setText(_data.names[j][1]);
		pb[j][1]->setStyleSheet(_makeStyle(j,1));
	}
}
QString PenColorsDialog::_makeStyle (int whichPen, int dark)
{
	QString style;

	style = QString("background-color:%1").arg(_data.colors[whichPen][dark].name());
	return style;
};


void PenColorsDialog::_SelectColor(int row, int dark)
{
	QColor newc = QColorDialog::getColor(_data.colors[row][dark], this, tr("FalconBoard - Select new pen color for Color #%1").arg(row + 2));
	if (newc.isValid())
	{
		_busy = true;
		ui.cbSelectScheme->setCurrentIndex(1);	// actual color is now the changed one
		_busy = false;
		_data.colors[row][dark] = newc;
		pb[row][dark]->setStyleSheet(_makeStyle(row,dark) );
		_changed = true;
	}
}

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
void PenColorsDialog::on_btnL6_clicked()
{
	_SelectColor(4, 0);
}
void PenColorsDialog::on_btnL7_clicked()
{
	_SelectColor(5, 0);
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
void PenColorsDialog::on_btnD6_clicked()
{
	_SelectColor(4, 1);
}
void PenColorsDialog::on_btnD7_clicked()
{
	_SelectColor(5, 1);
}

void PenColorsDialog::on_cbSelectScheme_currentIndexChanged(int index)
{
	if (_busy)
		return;
	if (_changed)
	{
		QMessageBox::StandardButton yes = 
			QMessageBox::question(this, tr("FalconBoard - Question"), 
										tr("Colors were changed. Without saving changes will be lost.\nDo you want to save the changes?"));
		if (yes == QMessageBox::Yes)
		{
			on_btnSaveScheme_pressed();
			return;
		}
	}
	_changed = false;
	_SetupFrom(index);
}

void PenColorsDialog::on_btnSaveScheme_pressed()
{
	int ix = ui.cbSelectScheme->currentIndex();
	QString qs = ix > 0 && ix != 1 ? ui.cbSelectScheme->currentText() : QString();
	PenColorSetDialog *pdlg = new PenColorSetDialog(qs, this);
	if (pdlg->exec())
	{
		_data.title = qs = pdlg->text();
		int selected[PEN_COUNT - 2] = { 0 };
		int cntChanged = 0;							// only save when there's a difference
		for (int i = 0; i < PEN_COUNT - 2; ++i)		// between the actual name or color and the default
		{
			if (_data.colors[i][0] != _schemes[0].colors[i][0] ||
				_data.colors[i][1] != _schemes[0].colors[i][1] ||
				_data.names[i][0]  != _schemes[0].names[i][0]  ||
				_data.names[i][1]  != _schemes[0].names[i][1] 	 )
			{
				selected[i] = 1;
				++cntChanged;
			}
		}

		int cnt = _schemes.size() - 1;		// new data will be appended
		QSettings* s = FBSettings::Open();
		if (qs.isEmpty())					// defaults
		{
			if (cntChanged)					// always save all pens as default
			{
				for (int i = 0; i < PEN_COUNT - 2; ++i)
					globalDrawColors.SetDefaultPen((FalconPenKind)(i+2),_data.colors[i][0], _data.colors[i][1], _data.names[i][0], _data.names[i][1]);
				globalDrawColors.DefaultsToSettings();
			}
		}
		else
		{
			s->beginGroup(PENGROUP);
			s->setValue(PENGROUPCOUNT, cnt);
			s->setValue(FalconPens::ItemName(cnt, 0, "t"), _data.title);
			if (cntChanged)
			{
				for (int i = 0; i < PEN_COUNT - 2; ++i)
				{
					if (selected[i])
					{
						s->setValue(FalconPens::ItemName(cnt, i + 2, "c"), FalconPens::Merge(_data.colors[i][0].name(), _data.colors[i][1].name()));
						s->setValue(FalconPens::ItemName(cnt, i + 2, "n"), FalconPens::Merge(_data.names[i][0], _data.names[i][1]));
					}
				}
			}
			s->endGroup();
		}
		FBSettings::Close();
	}

	_ReadData(false);
}
