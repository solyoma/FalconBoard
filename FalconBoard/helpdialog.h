#pragma once
#ifndef _HELPDIALOG_H
#define _HELPDIALOG_H

#include <QtWidgets/QDialog>

#include "ui_helpdialog.h"

class HelpDialog : public QDialog
{
	Q_OBJECT

public:
	HelpDialog(QWidget* parent = nullptr) : QDialog(parent) 
	{ 
		ui.setupUi(this); 
	}
private:
	Ui::HelpDialogClass ui;
};
#endif