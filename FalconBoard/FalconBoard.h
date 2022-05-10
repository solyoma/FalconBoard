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

#ifndef _VIEWER
	#include "snipper.h"
#endif
#include "ui_FalconBoard.h"

//class DrawArea;

const int MAX_NUMBER_OF_TABS = 30;

enum ScreenMode { smSystem, smDark, smBlack };

// ************************ helper **********************
QStringList GetTranslations();	// list of translation files
// ************************ /helper **********************


// ************************ FalconBoard **********************
class FalconBoard : public QMainWindow
{
	Q_OBJECT

public:
	FalconBoard(QWidget *parent = Q_NULLPTR);

	void SetLanguages(QStringList &names, int act)
	{
		_actLanguage = act;
		_translations = names;
		_PopulateLanguageMenu();
	}

	static QSize screenSize;
protected:
	void closeEvent(QCloseEvent* event) override;
	void showEvent(QShowEvent* event) override;

	void dragEnterEvent(QDragEnterEvent* event);
	void dropEvent(QDropEvent* event);

public slots:
	void slotGridSpacingChanged(int val);
	void slotGridSpacingEditingFinished();

private slots:
	void on_actionShowGrid_triggered();
	void on_actionFixedGrid_triggered();
	void on_actionGridSize_triggered();

	void on_actionShowPageGuides_triggered();

	void on_actionLoad_triggered();
	void on_action_Close_triggered();
	void on_action_CloseAll_triggered();

	void on_actionGoToPage_triggered();
	void on_actionCleaRecentList_triggered();
	void _sa_actionRecentFile_triggered(int which);
	void _sa_actionLanguage_triggered(int which);

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

	void on_actionRotateLeft_triggered();
	void on_actionRotateRight_triggered();
	void on_actionRotate180_triggered();
	void on_actionHFlip_triggered();
	void on_actionVFlip_triggered();
	void on_actionRectangle_triggered();
	void on_actionDrawEllipse_triggered();

	void on_action_Screenshot_triggered();
	void on_actionScreenshotTransparency_triggered();

	void on_actionClearRoll_triggered();
	void on_actionClearThisScreen_triggered();
	void on_actionClearDownward_triggered();

	void on_actionClearBackgroundImage_triggered();

	void on_actionInfinitePage_triggered();
	void on_actionLimitedPage_triggered();

	void on_action_InsertVertSpace_triggered();

	void on_actionApplyTransparencyToLoaded_triggered();

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
	void SlotForPenKindChange(FalconPenKind pk);

#endif
	void on_actionPageSetup_triggered();
	void on_actionPrint_triggered() { _drawArea->Print();  }
	void on_actionExportToPdf_triggered();

	void on_actionAbout_triggered();
	void on_actionHelp_triggered();

	void on_actionLightMode_triggered();
	void on_actionDarkMode_triggered();
	void on_actionBlackMode_triggered();

	void SlotForTabChanged(int index);
	void SlotForTabCloseRequested(int index);
	void SlotForTabMoved(int from, int to);
	void SlotForTabSwitched(int direction); //1: right, 2: left - no tab history

#ifndef _VIEWER
   signals:
	   void DrawPenColorBy(int key);

#endif
   signals:
	   void GridSpacingChanged(int spacing);

private:
	Ui::FalconBoardClass ui;

	struct FLAG {
		int b = 0;
		int operator++() { ++b; return b; }
		int operator--() { if(b) --b; return b; }
		operator bool() { return b; }
	} _busy;

	bool _firstShown = false;	// main window was shown first

	QStringList _translations;	// list of *.qm files in resources
	int _actLanguage = -1;			// index in list of languages ordered by abbreviations 
	QSignalMapper _languageMapper;

	bool	_eraserOn = false;
	int		_penWidth[PEN_COUNT] = { 3,3,3,3,3,30 };	// last is always the eraser
	FalconPenKind _actPen = penBlack;

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
	QString _sImageName;			// background image
	QByteArray _fileFormat = "png";

	DrawArea * _drawArea;
	QSpinBox * _psbPenWidth = nullptr;	// put on toolbar
	QSpinBox * _psbGridSpacing = nullptr;	// - " -
	QTabBar  * _pTabs = nullptr;
	QLabel   * _plblMsg = nullptr;		// put on status bar

	int		_nLastTab=-1;	// needed for tab change in slot as QTabBar send switch after already switched
	int _dontCareForTabChange = false; //true: no history switch and invalidate current history

	QList<QAction*> _saveAsActs;

	// recent files 
	QSignalMapper _signalMapper;
	QStringList _recentList;


	QActionGroup* _penGroup, *_modeGroup;

#ifndef _VIEWER
	Snipper* _plblScreen = nullptr;		// screen grab label
#endif

	QString _sBackgroundColor = "#FFFFFF",
			_sBackgroundHighLigtColor="#D8EAF9",
			_sSelectedBackgroundColor="#FFF",
			_sUnselectedBackgroundColor="#d0d0d0",
			_sTextColor = "#000000",
			_sDisabledColor = "#ccccc",
			_sToolBarColor = "#cccccc";

	QString _sGridColor = "#d0d0d0",
		    _sPageGuideColor = "#fcd475";
	bool _useScreenshotTransparency = false;
	qreal _transparencyFuzzyness = 0.0;
	QColor _screenshotTransparencyColor;

	ScreenMode _screenMode = smSystem;

	QString _lastDir, _lastPDFDir, _lastSaveName;
	int _nGridSpacing = 64;

	void RestoreState();
	void SaveState();

#ifdef _VIEWER
	void _RemoveMenus();
#endif
	QIcon _ColoredIcon(QIcon& sourceIcon, QColor colorW, QColor colorB = QColor());
	void _LoadIcons();
	void _SetupIconsForPenColors(ScreenMode sm);		// depend on mode

	void _LoadFiles(QStringList fileNames);
	void _SaveLastDirectory(QString fileName);
	bool IsOverwritable() const
	{
		bool bOverwritable = _drawArea->HistoryName().isEmpty() &&
			!_drawArea->IsModified();
		return bOverwritable;
	}
	bool _LoadData(int index = -1);	// into the index-th history (-1: current)
	void _AddToRecentList(QString path);

	void _CreateAndAddActions();

#ifndef _VIEWER
	QString _NextUntitledName();

	void _AddSaveVisibleAsMenu();

	SaveResult _saveResult = srSaveSuccess;	// because save as slot can't return a value
			// for _SaveXX() 0: cancelled, -1 error, 1 success
	SaveResult _SaveIfYouWant(int index, bool mustAsk = false);
	SaveResult _SaveFile(const QString name);
	bool _SaveBackgroundImage();
		 
	void _SelectPenForAction(QAction* paction);

	void _SelectPen();	// for _actPen

	void _SetPenKind();

	void _SetPenKind(FalconPenKind color);

	void _SetCursor(DrawCursorShape cs);

	void _SetBlackPen();
	void _SetRedPen()  ;
	void _SetGreenPen();
	void _SetBluePen() ;
	void _SetYellowPen() ;
		 
	void _SetPenWidth(FalconPenKind pk);

	void _SelectTransparentPixelColor();

	void _ConnectDisconnectScreenshotLabel(bool join); // toggle
#endif

	QString _FileNameToTabText(QString fname);

	int _AddNewTab(QString fname = QString(), bool loadIt = true, bool force=false);
	void _CloseTab(int index);
	void _SetTabText(int index, QString fname);
	void _SetupMode(ScreenMode mode);
	void _ClearRecentMenu();
	void _PopulateRecentMenu();		// from recentList
	void _PopulateLanguageMenu();	// from _translations (no clear)
	void _SetWindowTitle(QString qs);
};
#endif
