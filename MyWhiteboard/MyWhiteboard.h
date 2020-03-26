#pragma once

#include <QtWidgets/QMainWindow>
#include <QApplication>
#include <QColorDialog>
#include <QFileDialog>
#include <QImageWriter>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QCloseEvent>
#include <QEvent>

#include "ui_MyWhiteboard.h"

class DrawArea;

class MyWhiteboard : public QMainWindow
{
	Q_OBJECT

public:
	MyWhiteboard(QWidget *parent = Q_NULLPTR);

protected:
	void closeEvent(QCloseEvent* event) override;

private slots:
	void on_actionOpen_triggered();
	void on_actionSave_triggered();
	void on_actionAbout_triggered();
	void on_actionClearArea_triggered();
	void ona_actionSaveAs_triggered();

private:
	Ui::MyWhiteboardClass ui;

	QToolButton * btnBlack,
				* btnRed,
				* btnGreen,
				* btnBlue;

	void penColor(QColor& color) { _actColor = color; };
	void setBlackPen() { penColor(QColor("black")); }
	void setRedPen() { penColor(QColor("red")); }
	void setGreenPen() { penColor(QColor("gree")); }
	void setBluePen() { penColor(QColor("blue")); }

	void setPenWidth(int width) { if(width > 0) _actPenWidth = width; }

	QColor	_actColor;
	int		_actPenWidth = 2;
	QString _saveName;	// get format from extension
	QByteArray _fileFormat = "png";

	void CreateAndAddActions();
	bool shouldSave();
	bool saveFile();
	DrawArea* _drawArea;

	QList<QAction*> _saveAsActs;
};
