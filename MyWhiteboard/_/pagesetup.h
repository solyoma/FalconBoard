#pragma once
#ifndef _PAGESETUP
	#define _PAGESETUP

#include <QtWidgets/QDialog>

#include "ui_pagesetup.h"

class PageSetupDialog : public QDialog
{
	Q_OBJECT
public:
	PageSetupDialog(QWidget* parent = nullptr);
private:
	Ui::PageSetupDialogClass ui;
};

#endif