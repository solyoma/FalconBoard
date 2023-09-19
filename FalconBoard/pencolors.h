#pragma once
#ifndef _PENCOLORS_H
#define _PENCOLORS_H

#include <QtWidgets/QDialog>
#include <QColorDialog>

#include  "common.h"
#include "ui_pencolors.h"

class PenColorsDialog : public QDialog
{
	Q_OBJECT

public:
	PenColorsDialog(QWidget *parent=nullptr) : QDialog(parent)	{}
	~PenColorsDialog() {};
private:
	QColor _colors[5];

};

#endif