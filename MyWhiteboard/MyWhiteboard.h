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
	void on_actionNew_triggered();

	void on_actionLoad_triggered();
	void on_actionSave_triggered();
	void on_actionSaveAs_triggered();

	void on_actionLoadBackground_triggered();
	void on_actionSaveVisible_triggered();
	void SaveVisibleAsTriggered();

	void on_actionClearArea_triggered();
	void on_action_Print_triggered() { _drawArea->Print();  }
	void on_actionAbout_triggered();

	void on_action_Mark_triggered() {/*TODO*/ ; }

	void on_action_Black_triggered() { _SetBlackPen(); _SetCursor(DrawArea::csPen); _SetPenWidth(); };
	void on_action_Red_triggered()	 { _SetRedPen();   _SetCursor(DrawArea::csPen); _SetPenWidth(); };
	void on_action_Green_triggered() { _SetGreenPen(); _SetCursor(DrawArea::csPen); _SetPenWidth(); };
	void on_action_Blue_triggered()  { _SetBluePen();  _SetCursor(DrawArea::csPen); _SetPenWidth(); };
	void on_action_Eraser_triggered() 
	{ 
		_eraserOn = true; 
		_drawArea->SetPenColor("white");
		_drawArea->SetCursor(DrawArea::csEraser);
		_SetPenWidth(); 
		SlotForFocus(); 
	}

	void on_actionUndo_triggered();
	void on_actionRedo_triggered();

	void slotPenWidthChanged(int val) 
	{ 
		if (_busy)		// from program
			return;
						// from user
		if (_eraserOn)
			_eraserWidth = val;
		else
			_actPenWidth  = val;

		_SetPenWidth(); 
	}

	void SlotForUndo(bool b);
	void SlotForRedo(bool b);
	void SlotForFocus();
	void SlotForPointerType(QTabletEvent::PointerType pt);
private:
	Ui::MyWhiteboardClass ui;

	bool _busy = false;

	bool	_eraserOn = false;
	int		_actPenWidth = 2,
			_eraserWidth = 30;
	QColor	_actColor;
	QString _backgroundImageName;	// get format from extension
	QString _saveName;				// last saved data file
	QByteArray _fileFormat = "png";

	DrawArea * _drawArea;
	QSpinBox * _psbPenWidth = nullptr;
	QList<QAction*> _saveAsActs;

	void _CreateAndAddActions();
	void _AddSaveAsVisibleMenu();
	bool _SaveIfYouWant();
	bool _SaveFile();
	bool _SaveBackgroundImage();
		 
	void _SetPenColor(QColor& color) 
	{ 
		_actColor = color; 
		_eraserOn = false; 
		_drawArea->SetPenColor(_actColor);  
		_busy = true;
		_psbPenWidth->setValue(_actPenWidth);
		_busy = false;
		ui.centralWidget->setFocus();
	}
	void _SetCursor(DrawArea::CursorShape cs);
	void _SetBlackPen()  { _SetPenColor(QColor("black")); }
	void _SetRedPen()    { _SetPenColor(QColor("red")); }
	void _SetGreenPen()  { _SetPenColor(QColor("green")); }
	void _SetBluePen()   { _SetPenColor(QColor("blue")); }
		 
	void _SetPenWidth() 
	{ 
		int pw = _eraserOn ? _eraserWidth : _actPenWidth; 
		_drawArea->SetPenWidth(pw); 
		_busy = true;
		_psbPenWidth->setValue(pw);
		_busy = false;
	}
};
