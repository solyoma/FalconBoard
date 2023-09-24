#pragma once
#ifndef _PENCOLORSET_H
#define _PENCOLORSET_H

#include <QDialog>
#include "config.h"
#include "common.h"

#include "ui_pencolorset.h"

class PenColorSetDialog : public QDialog
{
	Q_OBJECT

		Ui::PenColorSetClass ui;
public:
	PenColorSetDialog(QString actName, QWidget* parent = nullptr);
	~PenColorSetDialog() {}

	QString text() { return ui.edtName->text(); }

private slots:
	void on_edtName_textChanged(const QString &text);
	void on_lstwNames_currentRowChanged(int index);
	void on_btnRemoveScheme_pressed();
	void on_btnRemoveAllSchemes_pressed();
};
#endif