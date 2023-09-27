#include "pencolorset.h"

PenColorSetDialog::PenColorSetDialog(QString actName, QWidget* parent)
{
	ui.setupUi(this);
	QSettings* s = FBSettings::Open();
	s->beginGroup(PENGROUP);
	int n = s->value(PENGROUPCOUNT, 0).toInt();
	if (n)
	{
		QString title;
		for (int j = 0; j < n; ++j)
		{
			title = s->value(FalconPens::ItemName(j, 0, "t"), "").toString();
			if (!title.isEmpty())
				ui.lstwNames->addItem(title);
		}
		ui.btnRemoveAllSchemes->setEnabled(true);
	}
	if (!actName.isEmpty())
		ui.edtName->setText(actName);
	s->endGroup();
	FBSettings::Close();
}

void PenColorSetDialog::on_edtName_textChanged(const QString &text)
{
	
}

void PenColorSetDialog::on_lstwNames_currentRowChanged(int index)
{
	ui.btnRemoveScheme->setEnabled(index >= 0);
}

void PenColorSetDialog::on_btnRemoveScheme_pressed()
{
	QSettings* s = FBSettings::Open();
	s->beginGroup(PENGROUP);
	int n = s->value(PENGROUPCOUNT, 0).toInt();
	if (n)
	{
		int ci = ui.lstwNames->currentIndex().row();
		QString title = ui.lstwNames->item(ci)->text();
		for (ci = 0; ci < n; ++ci)
			if (s->value(FalconPens::ItemName(ci, 0, "t"), "").toString() == title)
			{
				s->remove(FalconPens::ItemName(ci, 0, "t"));
				for (int j = 2; j < PEN_COUNT; ++j)
				{
					s->remove(FalconPens::ItemName(ci, j, "c"));
					s->remove(FalconPens::ItemName(ci, j, "n"));
				}
				break;
			}
	}
	s->endGroup();
	FBSettings::Close();
}

void PenColorSetDialog::on_btnRemoveAllSchemes_pressed()
{
	QSettings* s = FBSettings::Open();
	s->remove(PENGROUP);
	FBSettings::Close();
	ui.btnRemoveScheme->setEnabled(false);
	ui.btnRemoveAllSchemes->setEnabled(false);
}

void PenColorSetDialog::on_btnRemoveDefault_pressed()
{
	QSettings* s = FBSettings::Open();
	s->remove(DEFPENGROUP);
	FBSettings::Close();
	reject();
}
