#pragma once
#ifndef _PRINTPROGRESS_H
	#define _PRINTPROGRESS_H

#include <QtWidgets/QDialog>

#include "ui_printprogress.h"

class PrintProgressDialog : public QDialog
{
	Q_OBJECT

private:
	Ui::PrintProgressDialog ui;

	int _cntPages, _cntItems;
	int _itemsPrinted = 0;

public:
	PrintProgressDialog(int pageCount, int itemCount, QString title, QWidget* parent = nullptr) : _cntPages(pageCount), _cntItems(itemCount), QDialog(parent) 
	{
		ui.setupUi(this);
		if(!title.isEmpty())
			setWindowTitle(title);
	}
	~PrintProgressDialog() {}

	void Progress(int page)
	{
		ui.lblPage->setText(QString("#%1/%2").arg(page+1).arg(_cntPages));
		_itemsPrinted += 1;
		ui.pbPrint->setValue(_itemsPrinted * 100 / _cntItems);
	}
};

#endif // _PRINTPROGRESS_H