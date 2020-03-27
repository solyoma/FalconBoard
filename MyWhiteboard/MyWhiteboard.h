#pragma once

#include <QtWidgets/QMainWindow>
#include <QApplication>
#include <QColorDialog>
#include <QFileDialog>
#include <QImageWriter>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QSpinBox>
#include <QCloseEvent>
#include <QEvent>

#include "ui_MyWhiteboard.h"

class DrawArea;

const int HISTORY_DEPTH = 20;

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
	void on_actionSaveAs_triggered();
	void on_action_Print_triggered() { _drawArea->print();  }

	void on_action_Mark_triggered() { ; }

	void on_action_Black_triggered() { _SetBlackPen(); _SetPenWidth(); };
	void on_action_Red_triggered()	 { _SetRedPen();   _SetPenWidth(); };
	void on_action_Green_triggered() { _SetGreenPen(); _SetPenWidth(); };
	void on_action_Blue_triggered()  { _SetBluePen();  _SetPenWidth(); };
	void on_action_Ereaser_triggered() { _ereaserOn = true; _drawArea->setPenColor("white");  _SetPenWidth(); slotForFocus(); }

	void on_actionUndo_triggered();
	void on_actionRedo_triggered();

	void slotPenWidthChanged(int val) 
	{ 
		if (_busy)		// from program
			return;
						// from user
		if (_ereaserOn)
			_ereaserWidth = val;
		else
			_actPenWidth  = val;

		_SetPenWidth(); 
	}

	void slotForUndo(bool b);
	void slotForRedo(bool b);
	void slotForFocus();
private:
	Ui::MyWhiteboardClass ui;

	bool _busy = false;

	bool	_ereaserOn = false;
	int		_actPenWidth = 2,
			_ereaserWidth = 30;
	QColor	_actColor;
	QString _saveName;	// get format from extension
	QByteArray _fileFormat = "png";

	DrawArea * _drawArea;
	QSpinBox * _psbPenWidth = nullptr;
	QList<QAction*> _saveAsActs;

	QVector<DrawnItem> _undoStack;

	void _CreateAndAddActions();
	void _AddSaveAsMenu();
	bool _ShouldSave();
	bool _SaveFile();
		 
	void _SetPenColor(QColor& color) 
	{ 
		_actColor = color; 
		_ereaserOn = false; 
		_drawArea->setPenColor(_actColor);  
		_busy = true;
		_psbPenWidth->setValue(_actPenWidth);
		_busy = false;
		ui.centralWidget->setFocus();
	}
	void _SetBlackPen()  { _SetPenColor(QColor("black")); }
	void _SetRedPen()    { _SetPenColor(QColor("red")); }
	void _SetGreenPen()  { _SetPenColor(QColor("green")); }
	void _SetBluePen()   { _SetPenColor(QColor("blue")); }
		 
	void _SetPenWidth() 
	{ 
		int pw = _ereaserOn ? _ereaserWidth : _actPenWidth; 
		_drawArea->setPenWidth(pw); 
		_busy = true;
		_psbPenWidth->setValue(pw);
		_busy = false;
	}
};
