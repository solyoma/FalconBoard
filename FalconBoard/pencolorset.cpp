#include "pencolorset.h"

static const inline QString __ItemName(int grp, int pen, const char* pn)
{
	if (pen)
		return QString("grp%1pen%2_%3").arg(grp + 1).arg(pen).arg(pn);
	else	// remove title
		return QString("grp%1_%2").arg(grp + 1).arg(pn);
}

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
			title = s->value(__ItemName(j, 0, "t"), "").toString();
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
	// ??? ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!text.isEmpty());
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
			if (s->value(__ItemName(ci, 0, "t"), "").toString() == title)
			{
				s->remove(__ItemName(ci, 0, "t"));
				for (int j = 2; j < PEN_COUNT; ++j)
				{
					s->remove(__ItemName(ci, j, "c"));
					s->remove(__ItemName(ci, j, "n"));
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
