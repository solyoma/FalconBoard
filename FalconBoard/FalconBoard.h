#pragma once
#ifndef _MYWHITEBOARD_H
#define _MYWHITEBOARD_H

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
#include <QActionGroup>

#include "snipper.h"

#include "ui_FalconBoard.h"

//class DrawArea;

const int HISTORY_DEPTH = 20;
constexpr int NUM_COLORS = 6; // black/white, red, green, blue, eraser and yellow/purple

enum ScreenMode { smSystem, smDark, smBlack };

class FalconBoard : public QMainWindow
{
	Q_OBJECT

public:
	FalconBoard(QWidget *parent = Q_NULLPTR);

protected:
	void closeEvent(QCloseEvent* event) override;
	void showEvent(QShowEvent* event) override;

private slots:
	void on_actionShowGrid_triggered();
	void on_actionFixedGrid_triggered();
	void on_actionShowPageGuides_triggered();

	void on_actionLoad_triggered();
	void on_actionCleaRecentList_triggered();
	void _sa_actionRecentFile_triggered(int which);

#ifndef _VIEWER
	void on_actionNew_triggered();

	void on_actionSave_triggered();
	void on_actionSaveAs_triggered();

	void on_actionLoadBackground_triggered();
	void on_actionSaveVisible_triggered();
	void SaveVisibleAsTriggered();

	void on_actionUndo_triggered();
	void on_actionRedo_triggered();

	void on_action_Black_triggered() ;
	void on_action_Red_triggered()	 ;
	void on_action_Green_triggered() ;
	void on_action_Blue_triggered()  ;
	void on_action_Yellow_triggered();
	void on_action_Eraser_triggered();
	void on_action_Screenshot_triggered();

	void on_actionClearRoll_triggered();
	void on_actionClearThisScreen_triggered();
	void on_actionClearDownward_triggered();

	void on_actionClearBackgroundImage_triggered();

	void on_actionInfinitePage_triggered();
	void on_actionLimitedPage_triggered();

	void on_action_InsertVertSpace_triggered();

	void SlotForRubberBandSelection(int on);

	void slotPenWidthChanged(int val);
	void slotPenWidthEditingFinished();

	void SlotForUndo(bool b);
	void SlotForRedo(bool b);
	void SlotForFocus();
	void SlotForPointerType(QTabletEvent::PointerType pt);

	void SlotForScreenshotReady(QRect geometry);
	void SlotForScreenShotCancelled();

	void SlotIncreaseBrushSize(int ds);
	void SlotDecreaseBrushSize(int ds);

	void SlotForLabel(QString text);
	void SlotForPenKindChange(MyPenKind pk);
#endif
	void on_actionPageSetup_triggered();
	void on_actionPrint_triggered() { _drawArea->Print(_saveName);  }
	void on_actionExportToPdf_triggered() { _drawArea->ExportPdf(_saveName); }


	void on_actionAbout_triggered();
	void on_actionHelp_triggered();

	void on_actionLightMode_triggered();
	void on_actionDarkMode_triggered();
	void on_actionBlackMode_triggered();

#ifndef _VIEWER
   signals:
	   void DrawPenColorBy(int key);

#endif

private:
	Ui::FalconBoardClass ui;

	struct FLAG {
		int b = 0;
		int operator++() { ++b; return b; }
		int operator--() { if(b) --b; return b; }
		operator bool() { return b; }
	} _busy;

	bool _firstShown = false;	// main window was shown first

	bool	_eraserOn = false;
	int		_penWidth[NUM_COLORS] = { 3,3,3,3,30,3 };	// historical: penEraser = 5
	MyPenKind _actPen = penBlack;

		// default icons
	QIcon	_iconPen;	// set from white and change colors as required
	QIcon   _iconExit		   ;
	QIcon   _iconEraser		   ;
	QIcon   _iconNew		   ;
	QIcon   _iconOpen		   ;
	QIcon   _iconSave		   ;
	QIcon   _iconSaveAs		   ;
	QIcon   _iconUndo		   ;
	QIcon   _iconRedo		   ;
	QIcon   _iconScreenShot	   ;

	QString _backgroundImageName;	// get format from extension
	QString _saveName;				// last saved data file
	QString _sImageName;			// background image
	QByteArray _fileFormat = "png";

	DrawArea * _drawArea;
	QSpinBox * _psbPenWidth = nullptr;	// put on toolbar
	QLabel* _plblMsg = nullptr;			// -" -

	QList<QAction*> _saveAsActs;

	// recent files 
	QSignalMapper _signalMapper;
	QStringList _recentList;

	QActionGroup* _penGroup, *_modeGroup;

	Snipper* plblScreen = nullptr;		// screen grab label

	QString _sBackgroundColor = "#FFFFFF",
			_sTextColor = "#000000",
			_sDisabledColor = "#ccccc";

	QString _sGridColor = "#d0d0d0",
		    _sPageGuideColor = "#fcd475";

	ScreenMode _screenMode = smSystem;

	QString _lastDir, _lastFile;

	void RestoreState();
	void SaveState();

#ifdef _VIEWER
	void _RemoveMenus();
#endif
	QIcon _ColoredIcon(QIcon& sourceIcon, QColor colorW, QColor colorB = QColor());
	void _LoadIcons();
	void _SetupIconsForPenColors(ScreenMode sm);		// depend on mode

	void _SaveLastDirectory(QString fileName);
	void _LoadData(QString fileName);
	void _AddToRecentList(QString path);

	void _CreateAndAddActions();
#ifndef _VIEWER
	void _AddSaveVisibleAsMenu();
	bool _SaveIfYouWant(bool mustAsk = false);
	bool _SaveFile();
	bool _SaveBackgroundImage();
		 
	void _SelectPenForAction(QAction* paction);

	void _SelectPen();	// for _actPen

	void _SetPenKind();

	void _SetPenKind(MyPenKind color);

	void _SetCursor(DrawArea::CursorShape cs);

	void _SetBlackPen();
	void _SetRedPen()  ;
	void _SetGreenPen();
	void _SetBluePen() ;
	void _SetYellowPen() ;
		 
	void _SetPenWidth(MyPenKind pk);


	void _ConnectDisconnectScreenshotLabel(bool join); // toggle
#endif

	void _SetupMode(ScreenMode mode);
	void _ClearRecentMenu();
	void _PopulateRecentMenu();	// from recentList
};
#endif
