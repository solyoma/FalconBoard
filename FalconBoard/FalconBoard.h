#pragma once
#ifndef _FALCONBOARD_H
#define _FALCONBOARD_H

#include <thread>	// for sleep when waiting for data through the pipe 'FalconBoardPipe'

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

// to check if the application alread run we need these:
#include <QThread>
#include <QFileInfo>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>
// link under windows with AdvAPI32.Lib and add network to Qt libraries


#ifndef _VIEWER
	#include "snipper.h"
	#include "pencolorsdialog.h"
#endif
#include "ui_FalconBoard.h"

//class DrawArea;

const int MAX_NUMBER_OF_TABS = 30;
constexpr const char* TO_FRONT = "*toFront";
constexpr const int   TO_FRONT_SIZE = 8;

extern const QString appName ;
extern const QString keyName ;
extern const QString pipeName;
extern const QString fileExtension;


// ************************ helper **********************
QStringList GetTranslations();	// list of translation files
// ************************ /helper **********************

	class Listener;		// pre-declaration
#ifndef _VIEWER
	class Snapshotter;
#endif
// ************************ FalconBoard **********************
class FalconBoard : public QMainWindow
{
	Q_OBJECT

		//friend class ListenerThread;
#ifndef _VIEWER
		friend class Snapshotter;
#endif

public:
	FalconBoard(QSize screenSize, QWidget *parent = Q_NULLPTR);

	void StartListenerThread(QObject*parent=nullptr);

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
	void focusInEvent(QFocusEvent* event) override;

	void dragEnterEvent(QDragEnterEvent* event);
	void dropEvent(QDropEvent* event);

public slots:
	void SlotToSetGrid(bool on, bool fixed, uint16_t value);
	void slotGridSpacingChanged(int val);
	void slotGridSpacingEditingFinished();

private slots:
	void on_actionLeftMargin_triggered();
	void on_actionShowGrid_triggered();
	void on_actionFixedGrid_triggered();
	void on_actionGridSize_triggered();

	void on_actionShowPageGuides_triggered();

	void on_actionAutocorrectTolerance_triggered();

	void on_actionLoad_triggered();
	void on_action_Close_triggered();
	void on_action_CloseAll_triggered();

	void on_actionGoToPage_triggered();
	void on_actionCleaRecentList_triggered();
	void _sa_actionRecentFile_triggered(int which);
	void _sa_actionLanguage_triggered(int which);

#ifndef _VIEWER
	void on_actionNew_triggered();
	void on_actionAppend_triggered();

	void on_actionSave_triggered();
	void on_actionSaveAs_triggered();

	void on_actionImportImage_triggered();

	void on_actionLoadBackground_triggered();
	void on_actionSaveVisible_triggered();
	void SaveVisibleAsTriggered();

	void on_actionUndo_triggered();
	void on_actionRedo_triggered();

	void on_actionPenBlackOrWhite_triggered() ;
	void on_actionPenT2_triggered();
	void on_actionPenT3_triggered();
	void on_actionPenT4_triggered();
	void on_actionPenT5_triggered();
	void on_actionPenT6_triggered();
	void on_actionPenT7_triggered();
	void on_actionEraser_triggered();

	void on_actionRotate_triggered();
	void on_actionRepeatRotation_triggered();
	void on_actionRotateLeft_triggered();
	void on_actionRotateRight_triggered();
	void on_actionRotate180_triggered();
	void on_actionHFlip_triggered();
	void on_actionVFlip_triggered();
	void on_actionRectangle_triggered();
	void on_actionDrawFilledRectangle_triggered();
	void on_actionDrawEllipse_triggered();
	void on_actionDrawFilledEllipse_triggered();

	void on_actionMarkCenterPoint_triggered();
	void on_actionXToCenterPoint_triggered();

	void on_action_Screenshot_triggered();
	void on_actionScreenshotTransparency_triggered();

	void on_actionClearRoll_triggered();
	void on_actionClearThisScreen_triggered();
	void on_actionClearDownward_triggered();

	void on_actionClearBackgroundImage_triggered();

	void on_actionLimitPaperWidth_triggered();

	void on_actionDefinePenColors_triggered();

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
	void SlotForPenColorChanged();	// sent when 'history' is changed and pens are changed too changed actions when needed
	void SlotForChkGridOn(bool checked);
#ifndef _VIEWER
	void SlotForSnapshotTimer();
	void SlotSnapshotSaverFinished();
	void SlotTakeScreenshot(bool hideThisWindow);
#endif
	void on_actionPageSetup_triggered();
	void on_actionPrint_triggered() 
	{ 
#ifndef _VIEWER
		_QDoSaveModified(tr("print"));
#endif

		_drawArea->Print();  
	}
	void on_actionExportToPdf_triggered();

	void on_actionAllowMultipleProgramInstances_triggered();

	void on_actionAbout_triggered();
	void on_actionHelp_triggered();

	void on_actionSystemMode_triggered();
	void on_actionLightMode_triggered();
	void on_actionWhiteMode_triggered();
	void on_actionDarkMode_triggered();
	void on_actionBlackMode_triggered();

	void SlotForAddNewTab(QString name);
	void SlotForTabChanged(int index);
	void SlotForTabCloseRequested(int index);
	void SlotForTabMoved(int from, int to);
	void SlotForTabSwitched(int direction); //1: right, 2: left - no tab history

	void SlotForActivate();

#ifndef _VIEWER
   signals:
	   void SignalDrawPenColorBy(int key);
	   void SignalPenColorChanged(const DrawColors &drwclr);
	   void SignalToSaveSnapshots(IntVector& which);
#endif
   signals:
	   void SignalGridSpacingChanged(int spacing);
	   void SignalToCloseServer();

private:
	Ui::FalconBoardClass ui;

	FLAG _busy;
	bool _firstShown = false;	// main window was shown first

	QThread _listenerThread
#ifndef _VIEWER
		, _snapshotterThread
#endif
		;

#ifndef _VIEWER
	QTimer _snapshotTimer;
#endif

	Listener* _pListener = nullptr;
#ifndef _VIEWER
	Snapshotter* _pSnapshotter = nullptr;
#endif

	QStringList _translations;	// list of *.qm files in resources
	int _actLanguage = -1;			// index in list of languages ordered by abbreviations 
	QSignalMapper _languageMapper;

	bool	_eraserOn = false;
	int _penWidths[PEN_COUNT] = {30,3,3,3,3,3,3,3}; // first is the eraser C.f. FalconPenKind
	FalconPenKind _actPen = penBlackOrWhite;

		// default icons
	QIcon	_iconPen;	// set from white and change colors as required
	QIcon   _iconExit		   ;
	QIcon   _iconEraser		   ;
	QIcon   _iconNew		   ;
	QIcon   _iconOpen		   ;
	QIcon   _iconPrint		   ;
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
	QCheckBox* _pChkGridOn = nullptr;
	QTabBar  * _pTabs = nullptr;
	QLabel   * _plblMsg = nullptr;		// put on status bar

	int		_nLastTab=-1;	// needed for tab change in slot as QTabBar send switch after already switched
	int _dontCareForTabChange = false; //true: no history switch and invalidate current history

	QList<QAction*> _saveAsActs;

	// recent files 
	QSignalMapper _signalMapper;
	QStringList _recentList;


	QActionGroup* _penGroup, *_modeGroup;		// these are needed for radiobutton like behaviour for actions in the group

#ifndef _VIEWER
	QPointer<Snipper> _pSnipper = nullptr;		// screen grab label
#endif

	QString _sBackgroundColor,			// depends of the mood
		_sBackgroundHighlightColor,
		_sDisabledColor,
		_sEditBackgroundColor,
		_sEditTextColor,
		_sGridColor,
		_sMarginColor,
		_sPageGuideColor,
		_sPressedBackground,
		_sSelectedBackgroundColor,
		_sTextColor,
		_sToolBarColor,
		_sTabBarActiveTextColor,
		_sTabBarInactiveTextColor,
		_sToolTipTextColor,
		_sToolTipBackground,
		_sUnselectedBackgroundColor
		;

	bool _useScreenshotTransparency = false;
	qreal _transparencyFuzzyness = 0.0;
	QColor _screenshotTransparencyColor;

	ScreenMode _screenMode = ScreenMode::smSystem;

	QString _lastDir, _lastPDFDir, _lastSaveName;
	int _nGridSpacing = 64;

	void RestoreState();
	void SaveState();
	void SaveTabState();	// for all tabs after each timed snapshot save
	void SaveTabState(IntVector &tabindex);	// for tabs in list

#ifdef _VIEWER
	void _RemoveMenus();
#endif
	QIcon _ColoredIcon(QIcon& sourceIcon, QColor colorW, QColor colorB = QColor());
	void _LoadIcons();
	void _SetupIconsForPenColors(DrawColors *fromColors = nullptr);		// depend on mode

	void _LoadFiles(QStringList fileNames);
	void _SaveLastDirectory(QString fileName);
	bool IsOverwritable() const
	{
		bool bOverwritable = pHistory->Name().isEmpty() &&
			!pHistory->IsModified();
		return bOverwritable;
	}
	bool _LoadData(int index = -1);	// into the index-th history (-1: current)
	void _AddToRecentList(QString path);

	void _CreateAndAddActions();
	void _UncheckOtherModes(ScreenMode mode);

#ifndef _VIEWER

	void _QDoSaveModified(const QString s)
	{
		if (pHistory->IsModified())
		{
			QString cs = tr("Do you want to save the document before %1?\n\n(You may set automatic save in the Options menu.)").arg(s);
			if (ui.actionAutoSaveBeforePrintOrExport->isChecked() ||
				(QMessageBox::question(this, tr("FalconBoard - Question"), cs) == QMessageBox::Yes))
			{
				_drawArea->Save(-1);
				_SetResetChangedMark(-1);
			}
		}

	}
	QString _NextUntitledName();

	void _AddSaveVisibleAsMenu();

	void _DoScreenshot(bool hide);	// if hide is true this window will be hidden before screenshot taken

	SaveResult _saveResult = srSaveSuccess;	// because save as slot can't return a value
	int _saveCount = 0;						// this many documents saved
#endif
	void _SaveTabStates(QSettings*& s, IntVector& tabs);
#ifndef _VIEWER
			// for _SaveXX() 0: cancelled, -1 error, 1 success
	bool _StopSnapshotTimerAndWaitForIt();	// returns true if timer was running, false otherwise
	SaveResult _SaveIfYouWant(int index, bool mustAsk = false, bool onClose = false);
	SaveResult _SaveFile(const QString name);
	void _StartSnapshotSaveThread();
	bool _SaveBackgroundImage();

	void _SetPenCommon(FalconPenKind pk);
		 
	void _SelectPenForAction(QAction* paction);

	void _SelectPen();	// for _actPen

	void _SetPenKind();

	void _SetPenKind(FalconPenKind color);

	void _SetCursor(DrawCursorShape cs);

	void _SetPenWidth(FalconPenKind pk);


	void _SelectTransparentPixelColor();

	void _ConnectScreenshotLabel();
	void _DisconnectScreenshotLabel();
#endif

	QString _FileNameToTabText(QString fname);

	int _AddNewTab(QString fname = QString(), bool loadIt = true, bool force=false);
	void _CloseTab(int index);
	void _SetResetChangedMark(int index);
	void _SetTabText(int index, QString fname);
	void _PrepareActionIcons();
	void _SetupMode(ScreenMode mode);
	void _ClearRecentMenu();
	void _PopulateRecentMenu();		// from recentList
	void _PopulateLanguageMenu();	// from _translations (no clear)
	void _SetWindowTitle(QString qs);
};
// ========================================================= Listener ==========================================

class Listener	: public QObject
{
	Q_OBJECT
public:
	explicit Listener(FalconBoard& falconBoard)
				: _falconBoard(falconBoard)
	{
		_pLocalServer  = new QLocalServer(nullptr); // original parent in different thread
		_pLocalServer->listen(pipeName);
		connect(_pLocalServer, &QLocalServer::newConnection, this, &Listener::SlotForConnection);
	}

public slots:
	void SlotForCloseSocket()
	{
		if (_pLocalServer)
		{
			_pLocalServer->close();
			delete _pLocalServer;
			_pLocalServer = nullptr;
		}
	}
	void SlotForConnection()
	{
		GetDataFromServer();
	}

signals:
	void SignalAddNewTab(QString);
	void SignalActivate();

private:
	QLocalServer* _pLocalServer;
	FalconBoard& _falconBoard;

	void GetDataFromServer()	// called when a connection is detected
	{
		if (_pLocalServer->waitForNewConnection(100)) 
		{
			QLocalSocket* clientSocket = _pLocalServer->nextPendingConnection();
			// DEBUG
			qDebug("Listener Thread: connection detected for pipe '%s'", _pLocalServer->serverName().toStdString().c_str());
			if (clientSocket)
			{
				// DEBUG
				qDebug("               : clientSocket created");
				QByteArray data = clientSocket->readAll();
				int siz = data.size();
				// DEBUG
				qDebug("Listener Thread: data size: %d - processing", siz);
				if ((siz))
				{
					if (siz != TO_FRONT_SIZE || strcmp(data.constData(), TO_FRONT)) // then command line
						emit SignalAddNewTab(data.constData());
					clientSocket->flush();

				}

				//clientSocket->disconnectFromServer();
				//clientSocket->deleteLater();

				emit SignalActivate();
			}
		}
	}

};
// ========================================================= Snapshotter ==========================================
#ifndef _VIEWER
class Snapshotter : public QObject
{
	Q_OBJECT
public:
	explicit Snapshotter(FalconBoard* falconBoard, IntVector &whose) : _falconBoard(falconBoard), _whose(whose), QObject(nullptr)
	{

	}
public slots:
	void SlotToSaveSnapshots();
signals:
	void SignalFinished();
private:
	FalconBoard* _falconBoard; 
	IntVector _whose;
};
#endif		// _not VIEWER

#endif

