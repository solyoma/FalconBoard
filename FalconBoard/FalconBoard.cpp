#include <QSpinBox>
#include <QLabel>
#include <QScreen>
#include <QPainter>
#include <QThread>
#include <QSettings>

#include "config.h"
#include "pagesetup.h"
#include "common.h"
#include "DrawArea.h"
#include "screenshotTransparency.h"
#include "FalconBoard.h"
#include "myprinter.h"   // for MyPrinterData
#include "helpdialog.h"

QSize FalconBoard::screenSize;

QString UNTITLED;                               // stored here, defined in  common.h

    // these must be stored somewhere I store them here
QString FBSettings::homePath;
QSettings* FBSettings::_ps = nullptr;

QString FB_WARNING = QMainWindow::tr("falconBoard - Warning"),
        FB_ERROR   = QMainWindow::tr("falconBoard - Error");

#ifndef _Viewer
const QString appName = "FalconBoard.exe";
const QString keyName = "FalconBoardKey";
    #ifndef _DEBUG
const QString pipeName = "FalconBoardPipe";
    #else
const QString pipeName = "FalconBoardPipe-debug";
    #endif
#else
const QString appName = "FalconBoardViewer.exe";
const QString keyName = "FalconBoardViewerKey";
const QString pipeName = "FalconBoardViewerPipe";
#endif
const QString fileExtension = ".mwb";

int nUntitledOrder = 1;

#ifdef _VIEWER
void FalconBoard::_RemoveMenus()
{
    QList<QAction*> pMenuActions = ui.menuBar->actions(); // [0]:file,[1]:Edit,[2]:Clear,[3]:options,[4]:Help
            // (removeAction changes the order of the actions after the removed one
            // therefore this order is important
    ui.menuBar->removeAction(pMenuActions[1] ); // edit
    ui.menuBar->removeAction(pMenuActions[2]);  // clear

    QMenu* pMenu = pMenuActions[0]->menu();    
    // File Menu 
    pMenu->removeAction(pMenu->actions()[17]);      //17: Save as image
    pMenu->removeAction(pMenu->actions()[16]);      //16: export PDF
    pMenu->removeAction(pMenu->actions()[15]);      //15: Import image
                                                    //14: separator
                                                    //13: print
                                                    //12: page setup
                                                    //11: separator
                                                    //10: save visible
    pMenu->removeAction(pMenu->actions()[9]);       // 9: separator
    pMenu->removeAction(pMenu->actions()[8]);       // 8: Save As
    pMenu->removeAction(pMenu->actions()[7]);       // 7: Save
                                                    // 6: separator
                                                    // 5: close all
                                                    // 4: close
                                                    // 3: recent
    pMenu->removeAction(pMenu->actions()[2]);       // 2: Append
                                                    // 1: Open
    pMenu->removeAction(pMenu->actions()[0]);       // 0: New

    // options menu
    pMenu = pMenuActions[3]->menu();    
                                                    //19: language
    pMenu->removeAction(pMenu->actions()[18]);      //18:    separator
    pMenu->removeAction(pMenu->actions()[17]);      //17: Auto save background image
    pMenu->removeAction(pMenu->actions()[16]);      //16: Auto save before print
	pMenu->removeAction(pMenu->actions()[15]);      //15: Keep Changes
    pMenu->removeAction(pMenu->actions()[14]);      //14: Auto save data
                                                    //13: Allow multiple program instances
                                                    //12:   separator
    pMenu->removeAction(pMenu->actions()[11]);      //11: Load Background Image 
    pMenu->removeAction(pMenu->actions()[10]);      //10:   separator
    pMenu->removeAction(pMenu->actions()[9]);       // 9: Screenshot Transparency
    pMenu->removeAction(pMenu->actions()[8]);       // 8: Define pen colors
                                                    // 7:   separator
    pMenu->removeAction(pMenu->actions()[6]);       // 6: Auto correct tolerance
                                                    // 5: Show page guides
    pMenu->removeAction(pMenu->actions()[4]);       // 4: grid options
                                                    // 3: grid shown
                                                    // 2: show left margin
    pMenu->removeAction(pMenu->actions()[1]);       // 1: limit paper width
                                                    // 0: mode


    //ui.actionLoadBackground->setVisible(false);
    //ui.actionScreenshotTransparency->setVisible(false);

    //ui.actionAutoSaveData->setVisible(false);
    //ui.actionAutoSaveBackgroundImage->setVisible(false);
    //ui.actionApplyTransparencyToLoaded->setVisible(false);
    ui.menuGrid->removeAction(ui.menuGrid->actions()[2]);

    removeToolBar(ui.mainToolBar);
}
#endif

// ************************ helpers **********************

QStringList GetTranslations()
{
    QDir dir(":/FalconBoard/translations");
    QStringList qsl = dir.entryList(QStringList("*.qm"), QDir::Files, QDir::Name);
    qsl.sort();
    return qsl;
}
// ************************ FalconBoard **********************

FalconBoard::FalconBoard(QSize scrSize, QWidget *parent)	: QMainWindow(parent)
{   
    screenSize = scrSize;
    UNTITLED = QString(QObject::tr("Untitled"));    // set here so it gets translated

	ui.setupUi(this);

    globalDrawColors.DefaultsFromSettings();    // set up default colors
    globalDrawColors.Initialize();              // set up all other colors from defaults

    historyList.SetupClipBoard();

    if (!QDir(FBSettings::homePath).exists())
        QDir(FBSettings::homePath).mkdir(FBSettings::homePath);

    _drawArea = ui.drawArea; // static_cast<DrawArea*>(ui.centralWidget);
    _drawArea->SetPenKind(_actPen, _penWidths[_actPen]); 
    _drawArea->SetScreenSize(screenSize);

    _CreateAndAddActions(); // to toolbar
    connect(_pChkGridOn, &QCheckBox::toggled, ui.actionShowGrid, &QAction::setChecked);
    connect(_pChkGridOn, &QCheckBox::toggled, this, &FalconBoard::SlotForChkGridOn);

    connect(&_signalMapper, SIGNAL(mapped(int)), SLOT(_sa_actionRecentFile_triggered(int)));
    connect(&_languageMapper, SIGNAL(mapped(int)), SLOT(_sa_actionLanguage_triggered(int)));
#ifdef _VIEWER
    _RemoveMenus();        // viewer has none of these
#else
    _AddSaveVisibleAsMenu();                                                                                              
#endif

    QCoreApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents); // for tablet

#ifndef _VIEWER
    connect(_drawArea, &DrawArea::PointerTypeChange, this, &FalconBoard::SlotForPointerType);
    connect(_drawArea, &DrawArea::RubberBandSelection, this, &FalconBoard::SlotForRubberBandSelection);
    connect(this, &FalconBoard::SignalPenColorChanged, _drawArea, &DrawArea::SlotForPenColorRedefined);
    connect(_drawArea, &DrawArea::SignalPenColorChanged, this, &FalconBoard::SlotForPenColorChanged);   // set action icon for it from 'globalColors'
	connect(_drawArea, &DrawArea::SignalTakeScreenshot, this, &FalconBoard::SlotTakeScreenshot);
#endif
    connect(_drawArea, &DrawArea::SignalSetGrid, this, &FalconBoard::SlotToSetGrid);
    connect(this, &FalconBoard::SignalGridSpacingChanged, _drawArea, &DrawArea::SlotForGridSpacingChanged);

    connect(qApp, &QApplication::primaryScreenChanged, _drawArea, &DrawArea::SlotForPrimaryScreenChanged);

    RestoreState();     // sets up all history item names, and loads the last used ones

    PageParams::Load();
    _drawArea->SetupPage(PageParams::wtdPageSetup);

    _PrepareActionIcons();
    _SetupIconsForPenColors();
    setAcceptDrops(true);

#ifndef _VIEWER
    #ifdef _DEBUG
        _snapshotTimer.setInterval(10000ms);     // 10 seconds
    #else
        _snapshotTimer.setInterval(30000ms);    // 1/2 minute
    #endif
    connect(&_snapshotTimer, &QTimer::timeout, this, &FalconBoard::SlotForSnapshotTimer);
    _snapshotTimer.start();
#endif

    ui.drawArea->setFocus();
}

void FalconBoard::StartListenerThread(QObject* parent)
{
    _pListener = new Listener(*this);
    _pListener->moveToThread(&_listenerThread);

    connect(_pListener, &Listener::SignalAddNewTab, this, &FalconBoard::SlotForAddNewTab);
    connect(_pListener, &Listener::SignalActivate,  this, &FalconBoard::SlotForActivate);
    connect(this, &FalconBoard::SignalToCloseServer, _pListener, &Listener::SlotForCloseSocket);

    _listenerThread.start();
}

#ifndef _VIEWER
QString FalconBoard::_NextUntitledName()
{
	int n = -1, m;                  // expects: tab's name doesn't contain extension
	QString qsUntitled = UNTITLED,  // language dependednt name
                         tabName;
    for (int i = 0; i < _pTabs->count(); ++i)
    {
        tabName = _pTabs->tabText(i);
        if (tabName.isEmpty())
            n = 0;
        else if (tabName.left(UNTITLED.length()) == UNTITLED)
        {
            m = tabName.mid(UNTITLED.length()).toInt();
            QString qs = tabName.mid(UNTITLED.length());
//            qDebug("%s",tabName.mid(UNTITLED.length()).toLatin1());
            n = m + 1;
        }
    }
	if (n > 0)
		qsUntitled += QString().setNum(n);
	// qsUntitled += ".mwb";
    return qsUntitled;
}
#endif

/*========================================================
 * TASK:
 * PARAMS:
 * GLOBALS:
 * RETURNS:
 * REMARKS: - disables redraw
 *          - restores pen colors if they were modified from
 *            the default. When a new document is created it
 *            will use these colors. Existing documents
 *            still use their own colors.
 *-------------------------------------------------------*/
void FalconBoard::RestoreState()
{
    _drawArea->EnableRedraw(false);

    QString qsSettingsName = FBSettings::Name();
    bool bIniExisted = QFile::exists(qsSettingsName);
    QString qs;

    QSettings *s = FBSettings::Open();

    if (bIniExisted)
    {

        restoreGeometry(s->value(GEOMETRY).toByteArray());
        restoreState(s->value(WINDOWSTATE).toByteArray());
        qs = s->value(VERSION, "0").toString();       // (immediate toInt() looses digits)
        long ver = qs.toInt(0, 0);                                   // format Major.mInor.Sub

        if ((ver & 0xFFFFFF00) != (MAGIC_VERSION & 0xFFFFFF00))        // sub version number not used
        {
            QFile::remove(qsSettingsName);
            _AddNewTab();
            return;
        }
        file_version_loaded = ver;
    }
    bool b = s->value(SINGLE, true).toBool();
    ui.actionAllowMultipleProgramInstances->setChecked(!b);


    b = s->value(AUTOSAVEPRINT, false).toBool();
    ui.actionAutoSaveBeforePrintOrExport->setChecked(b);

    _drawArea->SetAutoCorrectLimit(s->value(AUTOCORRECT_LIMIT, 6).toInt() );

    qs = s->value(MODE, "s").toString();

    switch (qs[0].unicode())
    {
        case 'b': ui.actionBlackMode->setChecked(true); on_actionBlackMode_triggered(); break;
        case 'd': ui.actionDarkMode->setChecked(true);  on_actionDarkMode_triggered();  break;
        case 'l': ui.actionLightMode->setChecked(true); on_actionLightMode_triggered(); break;
        case 'w': ui.actionWhiteMode->setChecked(true); on_actionWhiteMode_triggered(); break;
        case 's': // default on form
        default: break;
    }

    b = s->value(SHOWLEFTMARGIN, true).toBool();        // left margin
    ui.actionLeftMargin->setChecked(b);
    if (!b)
        ui.marginWidget->setVisible(false);

    int n = s->value(GRID, 0).toInt();
    ui.actionFixedGrid->setChecked(n & 2);
#ifndef _VIEWER
    _pChkGridOn->setChecked(n & 1);
#endif
    _drawArea->SetGridOn(n & 1, n & 2);
    n = s->value(PAGEGUIDES, 0).toInt(0);
    ui.actionShowPageGuides->setChecked(n);
    _drawArea->SetPageGuidesOn(n);
    b = s->value(LIMITED, true).toBool();
    ui.actionLimitPaperWidth->setChecked(b);    // default: checked
#ifndef _VIEWER
    _drawArea->SetLimitedPage(b);
#endif

    MyPrinterData data;
    _drawArea->SetMyPrinterData(data);

    // globalDrawColors.FromSettings(s);    -  do not read it here, but save it if changed

#ifndef _VIEWER
    qs = s->value(PENSIZES, "30,3,3,3,3,3,3,3").toString();      // pen size for eraser, black, red, green, blue, yellow
    QStringList qsl = qs.split(',');
    if (qsl.size() != PEN_COUNT)
    {
        qsl.clear();
        qsl << "30" << "3" << "3" << "3" << "3" << "3" << "3" << "3";
    }
    for (int i = 0; i < PEN_COUNT; ++i)
        _penWidths[i] = qsl[i].toInt();

    if (file_version_loaded < 0x560203)      // in old version files eraser was the last, now it is the first
        std::swap(_penWidths[0], _penWidths[5]);


    _psbPenWidth->setValue(_penWidths[(int)penBlackOrWhite]); 
    
    ui.actionAutoSaveData->setChecked(s->value(AUTOSAVEDATA, false).toBool());
	ui.actionKeepChangedNoAsking->setChecked(s->value(KEEPCHANGED, true).toBool());
    ui.actionAutoSaveBackgroundImage->setChecked(s->value(AUTOSBCKIMG, false).toBool());
    qs = s->value(BCKGIMG, QString()).toString();
    if (!qs.isEmpty())
        _drawArea->OpenBackgroundImage(qs);

    if ((_useScreenshotTransparency = s->value(TRANSP, false).toBool()))
        ui.actionScreenshotTransparency->setChecked(_useScreenshotTransparency);

    _screenshotTransparencyColor = s->value(TRANSC, "#00ffffff").toString();
    _transparencyFuzzyness = _screenshotTransparencyColor.alphaF(); // 0.. 1.0
    _screenshotTransparencyColor.setAlphaF(1.0);
    
    _nGridSpacing = s->value(GRIDSPACING, 64).toInt();
    if (_nGridSpacing < 5)
        _nGridSpacing = 64;
    _psbGridSpacing->setValue(_nGridSpacing);
    emit SignalGridSpacingChanged(_nGridSpacing);
#endif

    _lastDir  = s->value(LASTDIR,  "").toString();
    if (!_lastDir.isEmpty() && _lastDir[_lastDir.size() - 1] != '/')
        _lastDir += "/";
    _lastPDFDir  = s->value(LASTPDFDIR,  "").toString();
    if (!_lastPDFDir.isEmpty() && _lastPDFDir[_lastPDFDir.size() - 1] != '/')
        _lastPDFDir += "/";

    // command line arguments    
    QStringList paramsList = QCoreApplication::arguments();
    for (auto& s : paramsList)
        s = QDir::cleanPath(s);     // need not exist

    int argc = 10 - paramsList.size() + 1;     // max 10 tabs can be opened, 0. params is command path
    // tabs

    s->beginGroup(TABS);
    int nFilesToRestore = s->value(TABSIZE, 0).toInt();
    if (nFilesToRestore)
    {
        bool b = false; // window title set?
        QString fname;
        for(int n = 0; n < nFilesToRestore && n < argc; ++n )
        {
            qs = QString().setNum(n);
            fname = qs = s->value(qs, QString()).toString();

            bool isSnapshot = qs.indexOf('/') < 0;
            if (isSnapshot)
                fname = FBSettings::homePath + qs;
            if (paramsList.indexOf(qs) < 0 && QFile::exists(fname) )    // if any of the old files is in the argument list do not use it
            {
                _AddNewTab(qs, false);  // do not load data yet
                if (!qs.isEmpty() || !b)
                    b = true, setWindowTitle(sWindowTitle + QString(" - %1").arg(qs));
            }
        }
        _nLastTab = s->value(LASTTAB, 0).toInt();
        int tabCount = _pTabs->count();
        if (tabCount == 0)   // files were deleted outside of this program
        {
            _nLastTab = -1;
            if (paramsList.size() == 1)  // and no command line parameters (no double click on *.mwb file)
            {
                _AddNewTab();     // + new empty history
                _nLastTab = 0;
            }
        }
        else if (tabCount < _nLastTab + 1)
            _nLastTab = tabCount - 1;
    }
    else if (paramsList.size() == 1)       // nothing to restore: create new empty tab
        _AddNewTab();    // + new empty history

    if (paramsList.size() > 1)      // then started by (double)clicking on a '.mwb' file (command line arguments)
    {
        int nFiles = 0;
        for (int n = 1; n < paramsList.size(); ++n) // 0th parameter is program name
        {
            if (QFile::exists(paramsList[n]))
                _AddNewTab(paramsList[n], false), ++nFiles; // NOT ; !
        }
        if (nFiles)
        {
            setWindowTitle(sWindowTitle + QString(" - %1").arg(paramsList[paramsList.size() - 1]));
            _nLastTab = _pTabs->count() - 1;
        }
        else    // no file in parameter list exists add empty tab
            _AddNewTab();    // + new empty history
    }

    if (_nLastTab > _pTabs->count())
        _nLastTab = 0;

    s->endGroup();       // tabs

    _sImageName = s->value(BCKGIMG, "").toString();
#ifndef _VIEWER
    if (!_sImageName.isEmpty() && !_drawArea->OpenBackgroundImage(_sImageName))
        _sImageName.clear();
#endif

    // recent documents
    s->beginGroup(RECENTLIST);
    int cnt = s->value(CNTRECENT, 0).toInt();
    
    for (int i = 0; i < cnt; ++i)
    {
        qs = s->value(QString().setNum(i + 1), "").toString();
        if (!qs.isEmpty())
        {
            _recentList.push_back(qs);
            if (_recentList.size() == 9)
                break;
        }
    }
    _PopulateRecentMenu();

    s->endGroup();

    FBSettings::Close();
}

/*=============================================================
 * TASK   : save program state
 * PARAMS :
 * EXPECTS:
 * GLOBALS:
 * RETURNS:
 * REMARKS: - does not save file names to restore
 *            that is done in SaveTabState()
 *          - also saves changed pen colors
 *          - pen colors in drawColors are replaced with the
 *            one from the actual document on close
 *------------------------------------------------------------*/
void FalconBoard::SaveState()
{
    QSettings *s = FBSettings::Open();

	s->setValue(GEOMETRY, saveGeometry());
	s->setValue(WINDOWSTATE, saveState());
    QString qsVersion("0x%1");
    qsVersion = qsVersion.arg(MAGIC_VERSION, 8, 16, QLatin1Char('0') );
	s->setValue(VERSION, qsVersion);
    if (_actLanguage < 0)
        s->remove(LANG);
    else
        s->setValue(LANG, _actLanguage);

    QString qs;
    switch (_screenMode)
    {
        case ScreenMode::smSystem:  qs = "s"; break;
        case ScreenMode::smLight :  qs = "l"; break;
        case ScreenMode::smWhite :  qs = "w"; break;
        case ScreenMode::smDark  :  qs = "d"; break;
        case ScreenMode::smBlack :  qs = "b"; break;
        default: break;
    }
    s->setValue(MODE, qs);
    s->setValue(GRID, (ui.actionShowGrid->isChecked() ? 1 : 0) + (ui.actionFixedGrid->isChecked() ? 2 : 0));
    s->setValue(SHOWLEFTMARGIN, ui.actionLeftMargin->isChecked());
    s->setValue(PAGEGUIDES, ui.actionShowPageGuides->isChecked() ? 1 : 0);
    s->setValue(LIMITED, ui.actionLimitPaperWidth->isChecked());
#ifndef _VIEWER
    s->setValue(AUTOCORRECT_LIMIT, _drawArea->AutoCorrectLimit());
    //globalDrawColors.ToSettings(s);   // no restore on start

    s->setValue(PENSIZES , QString("%1,%2,%3,%4,%5,%6,%7,%8").arg(_penWidths[0])
                                                             .arg(_penWidths[1])
                                                             .arg(_penWidths[2])
                                                             .arg(_penWidths[3])
                                                             .arg(_penWidths[4])
                                                             .arg(_penWidths[5])
                                                             .arg(_penWidths[6])
                                                             .arg(_penWidths[7]));
    s->setValue(AUTOSAVEDATA, ui.actionAutoSaveData->isChecked());
    s->setValue(KEEPCHANGED, ui.actionKeepChangedNoAsking->isChecked());
    s->setValue(AUTOSBCKIMG, ui.actionAutoSaveBackgroundImage->isChecked());
    if (ui.actionAutoSaveBackgroundImage->isChecked())
		s->setValue(BCKGIMG, _sImageName);
    s->setValue(TRANSP, _useScreenshotTransparency);
    QColor c = _screenshotTransparencyColor;
    c.setAlphaF(_transparencyFuzzyness);
    s->setValue(TRANSC, c.name(QColor::HexArgb));
    s->setValue(AUTOSAVEPRINT, ui.actionAutoSaveBeforePrintOrExport->isChecked());
#endif

	s->setValue(LASTDIR    , _lastDir);
	s->setValue(LASTPDFDIR , _lastPDFDir);
    s->setValue(GRIDSPACING, _nGridSpacing);

    if (_recentList.size())
    {
        s->beginGroup(RECENTLIST);
        s->setValue(CNTRECENT, _recentList.size());
        for (int i = 0; i < _recentList.size(); ++i)
            s->setValue(QString().setNum(i + 1), _recentList[i]);
        s->endGroup();
    }
    else
        s->remove(RECENTLIST);
    FBSettings::Close();
}

void FalconBoard::_SaveTabStates(QSettings*& s, IntVector& tabs)
{
    History* ph = nullptr;
    // include in the list: TABs with named files (even when they are empty)
    //  or non empty snapshots
    int siz = _pTabs->count();
    if(siz) 
    {
        s->beginGroup(TABS);

    	s->setValue("LASTTAB", _nLastTab);
        QString qs, qsn;
        for (int n = 0; n < siz; ++n)
        {
            ph = historyList[n];
            qs = QString().setNum(n);
            qsn = ph->IsSnapshot() ? ph->SnapshotName(false) : ph->Name();      // no path name for snapshots
            s->setValue(qs, qsn);
        }
        s->setValue("TABSIZE", siz);
        s->endGroup();
    }
}

/*=============================================================
 * TASK   : saves list of all open TABs
 * PARAMS :
 * EXPECTS:
 * GLOBALS: FBSettings
 * RETURNS:
 * REMARKS: WARNING: called from the main thread in the closeEvent
 *          and also from the snapshotTimerThread when timed
 *------------------------------------------------------------*/
void FalconBoard::SaveTabState()
{
    QSettings *s = FBSettings::Open();
    s->remove(TABS);
    IntVector tabs;
    for (int i = 0; i < _pTabs->count(); ++i)
        tabs.push_back(i);
    _SaveTabStates(s, tabs);
    FBSettings::Close();
}

void FalconBoard::SaveTabState(IntVector& savedTabs)
{
    QSettings* s = FBSettings::Open();
    s->remove(TABS);
    _SaveTabStates(s, savedTabs);
}

void FalconBoard::_LoadIcons()
{
    _iconPen = QIcon(":/FalconBoard/Resources/white.png");
    _iconExit = QIcon(":/FalconBoard/Resources/close.png");
    _iconEraser = QIcon(":/FalconBoard/Resources/eraser.png");
    _iconNew = QIcon(":/FalconBoard/Resources/new.png");
    _iconOpen = QIcon(":/FalconBoard/Resources/open.png");
    _iconSave = QIcon(":/FalconBoard/Resources/save.png");
    _iconPrint = QIcon(":/FalconBoard/Resources/printer.png");
    _iconSaveAs = QIcon(":/FalconBoard/Resources/saveas.png");
    _iconUndo = QIcon(":/FalconBoard/Resources/undo.png");
    _iconRedo = QIcon(":/FalconBoard/Resources/redo.png");
    _iconScreenShot = QIcon(":/FalconBoard/Resources/screenshot.png");
}

void FalconBoard::_SetupIconsForPenColors(DrawColors *pdrclr)
{
    if (!pdrclr)
        pdrclr = &globalDrawColors;

//    globalDrawColors.SetDarkMode(sm != smSystem);

#ifndef _VIEWER
    ui.actionPenBlackOrWhite->setIcon(_ColoredIcon(_iconPen, globalDrawColors.Color(penBlackOrWhite)));
    ui.actionPenBlackOrWhite->setText(globalDrawColors.ActionText(penBlackOrWhite));

    ui.actionPenT2->setIcon(_ColoredIcon(_iconPen,   pdrclr->Color(penT2)));
    ui.actionPenT2->setText(pdrclr->ActionText(penT2));

    ui.actionPenT3->setIcon(_ColoredIcon(_iconPen, pdrclr->Color(penT3)));
    ui.actionPenT3->setText(pdrclr->ActionText(penT3));

    ui.actionPenT4->setIcon(_ColoredIcon(_iconPen,  pdrclr->Color(penT4)));
    ui.actionPenT4->setText(pdrclr->ActionText(penT4));

    ui.actionPenT5->setIcon(_ColoredIcon(_iconPen,pdrclr->Color(penT5)));
    ui.actionPenT5->setText(pdrclr->ActionText(penT5));

    ui.actionPenT6->setIcon(_ColoredIcon(_iconPen,pdrclr->Color(penT6)));
    ui.actionPenT6->setText(pdrclr->ActionText(penT6));

    ui.actionPenT7->setIcon(_ColoredIcon(_iconPen,pdrclr->Color(penT7)));
    ui.actionPenT7->setText(pdrclr->ActionText(penT7));
#endif
}

void FalconBoard::_CreateAndAddActions()
{   
    _LoadIcons();
#ifndef _VIEWER
    // add filetype selection submenu for save image
    const QList<QByteArray> imageFormats = QImageWriter::supportedImageFormats();
    for (const QByteArray& format : imageFormats) {
        QString text = tr("%1...").arg(QString::fromLatin1(format).toUpper());

        QAction* action = new QAction(text, this);
        action->setData(format);
        connect(action, &QAction::triggered, this, &FalconBoard::SaveVisibleAsTriggered);
        _saveAsActs.append(action);
    }

    ui.mainToolBar->addAction(ui.actionExit);
    ui.mainToolBar->addSeparator();
    ui.mainToolBar->addAction(ui.actionUndo);
    ui.mainToolBar->addAction(ui.actionRedo);
    ui.mainToolBar->addSeparator();
    ui.mainToolBar->addAction(ui.actionNew);
    ui.mainToolBar->addAction(ui.actionLoad);
    ui.mainToolBar->addAction(ui.actionSave);
    ui.mainToolBar->addAction(ui.actionSaveAs);
    ui.mainToolBar->addSeparator();
    ui.mainToolBar->addAction(ui.actionPrint);
    ui.mainToolBar->addSeparator();

    ui.mainToolBar->addAction(ui.actionPenBlackOrWhite);
    ui.mainToolBar->addAction(ui.actionPenT2);
    ui.mainToolBar->addAction(ui.actionPenT3);
    ui.mainToolBar->addAction(ui.actionPenT4);
    ui.mainToolBar->addAction(ui.actionPenT5);
    ui.mainToolBar->addAction(ui.actionPenT6);
    ui.mainToolBar->addAction(ui.actionPenT7);

    ui.mainToolBar->addAction(ui.actionEraser);

        // these are needed for radiobutton like behaviour for actions in the group
    _penGroup = new QActionGroup(this);
    _penGroup->addAction(ui.actionPenBlackOrWhite);
    _penGroup->addAction(ui.actionPenT2);
    _penGroup->addAction(ui.actionPenT3);
    _penGroup->addAction(ui.actionPenT4);
    _penGroup->addAction(ui.actionPenT5);
    _penGroup->addAction(ui.actionPenT6);
    _penGroup->addAction(ui.actionPenT7);

    _penGroup->addAction(ui.actionEraser);
#endif

    _modeGroup = new QActionGroup(this);
    _modeGroup->addAction(ui.actionLightMode);
    _modeGroup->addAction(ui.actionDarkMode);
    _modeGroup->addAction(ui.actionBlackMode);

#ifndef _VIEWER
    ui.mainToolBar->addSeparator();

    ui.mainToolBar->addWidget(new QLabel(tr("Pen Width:")));
    _psbPenWidth = new QSpinBox();
    _psbPenWidth->setMinimum(1);
    _psbPenWidth->setMaximum(200);
    _psbPenWidth->setSingleStep(1);
    _psbPenWidth->setValue(_penWidths[0]);
    QRect rect = _psbPenWidth->geometry();
    rect.setWidth(40);
    _psbPenWidth->setGeometry(rect);
    ui.mainToolBar->addWidget(_psbPenWidth);

    ui.mainToolBar->addSeparator();

//    ui.mainToolBar->addWidget(new QLabel(tr("Grid ")));
    _pChkGridOn = new QCheckBox(tr("Grid size:"));
    ui.mainToolBar->addWidget(_pChkGridOn);
//    ui.mainToolBar->addWidget(new QLabel(tr(" size:")));


    _psbGridSpacing = new QSpinBox();
    _psbGridSpacing->setMinimum(10);
    _psbGridSpacing->setMaximum(400);
    _psbGridSpacing->setSingleStep(10);
    _psbGridSpacing->setValue(_nGridSpacing);
    rect = _psbGridSpacing->geometry();
    rect.setWidth(60);
    _psbGridSpacing->setGeometry(rect);
    ui.mainToolBar->addWidget(_psbGridSpacing);

    ui.mainToolBar->addSeparator();
    ui.mainToolBar->addAction(ui.action_Screenshot);
    ui.mainToolBar->addSeparator();
#endif
    _pTabs = new QTabBar();
    ui.mainToolBar->addWidget(_pTabs);
    _pTabs->setMovable(true);
    _pTabs->setAutoHide(true);
    _pTabs->setTabsClosable(true);
#ifndef _VIEWER
   // status bar
    _plblMsg = new QLabel();
    statusBar()->addWidget(_plblMsg);

    // more than one valueChanged() function exists
    connect(_psbPenWidth, QOverload<int>::of(&QSpinBox::valueChanged), this, &FalconBoard::slotPenWidthChanged);
    connect(_psbPenWidth, &QSpinBox::editingFinished, this, &FalconBoard::slotPenWidthEditingFinished);

    connect(_psbGridSpacing, QOverload<int>::of(&QSpinBox::valueChanged), this, &FalconBoard::slotGridSpacingChanged);
    connect(_psbGridSpacing, &QSpinBox::editingFinished, this, &FalconBoard::slotGridSpacingEditingFinished);

    connect(_drawArea, &DrawArea::CanUndo, this, &FalconBoard::SlotForUndo);
    connect(_drawArea, &DrawArea::CanRedo, this, &FalconBoard::SlotForRedo);
    connect(_drawArea, &DrawArea::WantFocus, this, &FalconBoard::SlotForFocus);
    connect(_drawArea, &DrawArea::TextToToolbar, this, &FalconBoard::SlotForLabel);
    connect(_drawArea, &DrawArea::IncreaseBrushSize, this, &FalconBoard::SlotIncreaseBrushSize);
    connect(_drawArea, &DrawArea::DecreaseBrushSize, this, &FalconBoard::SlotDecreaseBrushSize);

    connect(ui.actionClearHistory, &QAction::triggered, _drawArea, &DrawArea::ClearHistory);
#endif
    connect(_drawArea, &DrawArea::CloseTab, this, &FalconBoard::SlotForTabCloseRequested);
    connect(_drawArea, &DrawArea::TabSwitched, this, &FalconBoard::SlotForTabSwitched);

    connect(_pTabs, &QTabBar::currentChanged, this, &FalconBoard::SlotForTabChanged);
    connect(_pTabs, &QTabBar::tabCloseRequested, this, &FalconBoard::SlotForTabCloseRequested);
    connect(_pTabs, &QTabBar::tabMoved, this, &FalconBoard::SlotForTabMoved);
}

void FalconBoard::_UncheckOtherModes(ScreenMode mode)
{
    bool checks[5] = { false }; // same # as the # of modes

    checks[(int)mode] = true;
    ui.actionSystemMode->setChecked(checks[0]);
    ui.actionLightMode->setChecked(checks[1]);
    ui.actionWhiteMode->setChecked(checks[2]);
    ui.actionDarkMode->setChecked(checks[3]);
    ui.actionBlackMode->setChecked(checks[4]);
}                

int FalconBoard::_AddNewTab(QString fname, bool loadIt, bool force) // and new history record
{
    if (_pTabs->count() == MAX_NUMBER_OF_TABS)
    {
        QMessageBox::warning(this, tr("Falconboard - Warning"), tr("Too many open files"));
        return -1;
    }
#ifndef _VIEWER
    _drawArea->HideRubberBand(true);
#endif
    // check if it is already loaded
    int n;
    if (!force && (n = _drawArea->SameFileAlreadyUsed(fname))>=0) // force: used only for new document
    {
        _pTabs->setCurrentIndex(n);
        return n;
    }

    QString name = fname;
    n = _pTabs->addTab(_FileNameToTabText(QString(fname)));
    if (n > 0)  // else tab switch is called 
        _pTabs->setCurrentIndex(n);

    _drawArea->AddHistory(name, loadIt);
    // DEBUG
//*    int m = _pTabs->count();
//    _pTabs->setAutoHide(_pTabs->count() > 1);
    return n;
}

void FalconBoard::_CloseTab(int index)
{
    _drawArea->SwitchToHistory(index, false);    // do not redraw

    _AddToRecentList(pHistory->Name());
    int cnt = _drawArea->RemoveHistory(index);
#ifndef _VIEWER
    _drawArea->HideRubberBand(true);
#endif
    _pTabs->removeTab(index);
    if (!cnt)
        _AddNewTab(QString(), false);

    _drawArea->SwitchToHistory(_nLastTab, !cnt);
    ui.actionAppend->setEnabled(!pHistory->Name().isEmpty());
}

#ifndef _VIEWER
void FalconBoard::_AddSaveVisibleAsMenu()
{
    QMenu *pSaveAsVisibleMenu = new QMenu(QApplication::tr("Save Visi&ble As..."), this);
    for (QAction* action : qAsConst(_saveAsActs))
        pSaveAsVisibleMenu->addAction(action);
    ui.menu_File->insertMenu(ui.actionPageSetup, pSaveAsVisibleMenu);
    ui.menu_File->insertSeparator(ui.actionPageSetup);
}


bool FalconBoard::_StopSnapshotTimerAndWaitForIt()
{
    bool res = _snapshotTimer.isActive();
    _snapshotTimer.stop();
    int of = 1000000000;                   // wait until save in the other thread is finished
    while (_pSnapshotter && --of)
        ;
    return res;
}

/*========================================================
 * TASK:    check index-th history for change 
 *          conditionally asks for save confirmation 
 *          then saves it
 * PARAMS:  index: save this history if changed
 *          mustAsk: do not autosave it, always ask
 *          onTabClose: true: do not switch to index-th history
 *                  false: do not switch to 
 * GLOBALS: ui.actionAutoSaveData, _historyList
 * RETURNS: srNoSave : no save was required or file was saved
 *          srCanceed: save is cancelled
 *          srFailed : save error
 *          srSaveSuccess: saved
 * REMARKS: - set 'mustAsk' to true when the user closes
 *              a changed TAB
 *          - not just the active history can be saved
 *          - when autosave is in effect and 
 *              the file has no name set yet, saves into 
 *              the snapshot, otherwise saves the file first
 *              and then deletes the snapshot files
 *-------------------------------------------------------*/
SaveResult FalconBoard::_SaveIfYouWant(int index, bool mustAsk, bool onTabClose)
{
    bool isTimerIsRunning = _StopSnapshotTimerAndWaitForIt();  // save timer state

    _saveResult = srSaveSuccess;

    _drawArea->SwitchToHistory(index, false);    // but do not redraw

    const QString & saveName = pHistory->Name();

    bool hasSnapshot = pHistory->IsSnapshot();
    if (!pHistory->IsModified() && !hasSnapshot) //?? other cases?
    {
       if(isTimerIsRunning)
           _snapshotTimer.start();
        return _saveResult;
    }

    QString snapshotName = pHistory->SnapshotName(true);

    QMessageBox::StandardButton ret = QMessageBox::Save;

    bool isAutoSaveSet = ui.actionAutoSaveData->isChecked() && !mustAsk;
    bool isUntitled = saveName.isEmpty() || saveName.left(UNTITLED.length()) == UNTITLED;

            // when a tab was closed
    auto handleTabClose = [&]()
        {
            if (!onTabClose)
            {
                _drawArea->SwitchToHistory(_nLastTab, true);
                ui.actionAppend->setEnabled(!pHistory->Name().isEmpty());
            }       
        };

    
    if (!isAutoSaveSet )         // otherwise don't ask user, not even for untitled
    {
        ret = QMessageBox::warning(this, tr(WindowTitle),
                                        tr("<i>%1</i> have been modified.\n"
                                            "Do you want to save your changes?").arg(saveName.isEmpty() ? _NextUntitledName() : saveName),
                                                    QMessageBox::Save | QMessageBox::No | QMessageBox::Cancel);

        if (ret == QMessageBox::Save)
        {
            on_actionSave_triggered();
            if (_saveResult == srSaveSuccess)
            {
                pHistory->SetName(_lastSaveName, true);
                if (_drawArea->HistoryIsSnapshot(-1))
                {
                    QFile::remove(snapshotName);
                    QFile::remove(snapshotName + ".dat");
                }
                handleTabClose();
            }
            if (isTimerIsRunning)
                _snapshotTimer.start();
            return _saveResult = srFailed;
        }
        else if (ret == QMessageBox::Cancel)
        {
            if (isTimerIsRunning)
                _snapshotTimer.start();
            return _saveResult = srCancelled;
        }
        else if (ret == QMessageBox::No)
        {
            if (isTimerIsRunning)
                _snapshotTimer.start();
            return _saveResult = srNoSave;
        }
    }

    // auto save file
    _saveResult = pHistory->Save(saveName.isEmpty());    // save file or snapshot

    if (isTimerIsRunning)
        _snapshotTimer.start();

    return _saveResult;
}

SaveResult FalconBoard::_SaveFile(const QString name)   // returns: 0: cancelled, -1 error, 1 saved
{
    _saveResult = _drawArea->Save(-1, name);
    if (_saveResult == srSaveSuccess)
    {
        _AddToRecentList(name);
        _lastSaveName = name;
    }
    return _saveResult;
}

/*=============================================================
 * TASK   : Saves snapshot in the background
 * PARAMS :
 * EXPECTS:
 * GLOBALS:
 * RETURNS:
 * REMARKS: - starts new thread to save snapshots
 *          - state of save is checked in 'SlotSnapshotSaverFinished()'
 *------------------------------------------------------------*/
void FalconBoard::_StartSnapshotSaveThread()
{
    IntVector  modList = historyList.ModifiedItems();
    if (modList.size())
    {
        _drawArea->SetSnapshotterState(true);           // before creating snapshotter is it important?
        _pSnapshotter = new Snapshotter(this, modList);
        _pSnapshotter->moveToThread(&_snapshotterThread);
        connect(_pSnapshotter, &Snapshotter::SignalFinished, this, &FalconBoard::SlotSnapshotSaverFinished);
        connect(&_snapshotterThread, &QThread::started, _pSnapshotter, &Snapshotter::SlotToSaveSnapshots);
        _snapshotterThread.start();
    }
}

bool FalconBoard::_SaveBackgroundImage()
{
    return _drawArea->SaveVisibleImage(_backgroundImageName, _fileFormat.constData());
}

void FalconBoard::_SelectPenForAction(QAction* paction)
{
    paction->setChecked(true);
}

void FalconBoard::_SelectPen()     // call after '_actPen' is set
{
    switch (_actPen)
    {
        default:
        case penBlackOrWhite: ui.actionPenBlackOrWhite->setChecked(true); break;
        case penT2: ui.actionPenT2->setChecked(true); break;
        case penT3: ui.actionPenT3->setChecked(true); break;
        case penT4: ui.actionPenT4->setChecked(true); break;
        case penT5: ui.actionPenT5->setChecked(true); break;
        case penT6: ui.actionPenT6->setChecked(true); break;
        case penT7: ui.actionPenT7->setChecked(true); break;
        case penEraser:on_actionEraser_triggered(); return;
    }
    _SetPenKind();
}

void FalconBoard::_SetPenKind()
{
    _eraserOn = _actPen == penEraser;
    if(_actPen != penNone)
	{
		_drawArea->SetPenKind(_actPen, _penWidths[_actPen]);
		++_busy;
		_psbPenWidth->setValue(_penWidths[_actPen]);
		--_busy;
	}
}

void FalconBoard::_SetPenKind(FalconPenKind newPen)
{
    _actPen = newPen;
    _SelectPen();
}


void FalconBoard::_SetCursor(DrawCursorShape cs)
{
    _drawArea->SetCursor(cs);
}

void FalconBoard::_SetPenWidth(FalconPenKind pk)
{
    _drawArea->SetPenKind(pk, _penWidths[pk]);
    ++_busy;
    _psbPenWidth->setValue(_penWidths[pk]);
    --_busy;
}

void FalconBoard::_SelectTransparentPixelColor()
{
//    _drawArea->_SelectTransparentPixelColor();
    
    ScreenShotTransparencyDialog *dlg = new ScreenShotTransparencyDialog(this, _screenshotTransparencyColor, _useScreenshotTransparency, _transparencyFuzzyness);
    if (dlg->exec())
        dlg->GetResult(_screenshotTransparencyColor, _useScreenshotTransparency, _transparencyFuzzyness);
    ui.actionScreenshotTransparency->setChecked(_useScreenshotTransparency);
    delete dlg;
}
#endif

/*========================================================
 * TASK: from a single QIcon create icons for all used colors
 * PARAMS:  icon - base icon (png with white and black color)
 *          colorW - replacement color for white must be valid
 *          colorB - replacement colors for black,
                     may be invalid
 * GLOBALS:
 * RETURNS: new icon with new colors set
 * REMARKS: -
 *-------------------------------------------------------*/
QIcon FalconBoard::_ColoredIcon(QIcon& sourceIcon, QColor colorW, QColor colorB)
{
    QPixmap pm, pmW, pmB;
    pmW = pmB = pm = sourceIcon.pixmap(64, 64);
    QBitmap mask = pm.createMaskFromColor(Qt::white, Qt::MaskOutColor); // whites become transparent

    pmW.fill(colorW);
    pmW.setMask(mask);
    // DEBUG
    // pmW.save("whiteMask.png");
    if (colorB.isValid())
    {
        mask = pm.createMaskFromColor(Qt::black, Qt::MaskOutColor); // blacks become transparent
        pmB.fill(colorB);
        pmB.setMask(mask);
    // DEBUG
    //    pmB.save("blackMask.png");

        QPainter painter(&pmW);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawPixmap(0,0, pm.width(), pm.height(), pmB);
    }
    return QIcon(pmW);
}


#ifndef _VIEWER
void FalconBoard::_ConnectScreenshotLabel()
{
    connect(_pSnipper, &Snipper::SnipperCancelled, this, &FalconBoard::SlotForScreenShotCancelled);
    connect(_pSnipper, &Snipper::SnipperReady, this, &FalconBoard::SlotForScreenshotReady);
}
    
void FalconBoard::_DisconnectScreenshotLabel()
{
    disconnect(_pSnipper, &Snipper::SnipperCancelled, this, &FalconBoard::SlotForScreenShotCancelled);
    disconnect(_pSnipper, &Snipper::SnipperReady, this, &FalconBoard::SlotForScreenshotReady);
}
#endif

QString FalconBoard::_FileNameToTabText(QString fname)
{
    bool isSnapshot = fname.indexOf('/') < 0;
    if (isSnapshot)
    {
        QFile f(FBSettings::homePath + fname);
        f.open(QIODevice::ReadOnly);
        if (!f.isOpen())         // no name file
            fname.clear();       // and no name
        else
        {
            QDataStream ids(&f);
            ids >> fname;
        }
    }

#ifndef _VIEWER
    if (fname.isEmpty())
        fname = _NextUntitledName();
    else
#endif
    {
        int i = fname.lastIndexOf('/'),  // get file name, -1 is Ok
            j = fname.lastIndexOf(".mwb");
        if (j < 0)
            j = fname.length();
        return fname.mid(i + 1, j - i - 1);
    }
    return fname;
}

void FalconBoard::_SetResetChangedMark(int index)
{   
    if(index < 0)
        index = _pTabs->currentIndex();
    QString text = _pTabs->tabText(index);

    if (_drawArea->IsModified(index, true)) // when snapshot saved
        _pTabs->setTabIcon(index, _ColoredIcon(_iconSaveAs, CHANGED_MARKER_FOREGROUND, CHANGED_MARKER_BACKGROUND));
    else
        _pTabs->setTabIcon(index, QIcon());
    _pTabs->setTabText(index, text);
}

void FalconBoard::_SetTabText(int index, QString fname)
{
    if (index >= _pTabs->count())
        return;
    if (index < 0)
        index = _pTabs->currentIndex();
#ifndef _VIEWER
    if (fname.isEmpty())
        fname = _NextUntitledName();
#endif

    _pTabs->setTabToolTip(index, fname);
    QString text = _FileNameToTabText(fname);
    _pTabs->setTabText(index, text);
}

void FalconBoard::_PrepareActionIcons()
{
	if (globalDrawColors.IsDarkMode())
	{
		ui.actionExit->setIcon(_ColoredIcon(_iconExit, Qt::black, Qt::white));
		ui.actionEraser->setIcon(_ColoredIcon(_iconEraser, Qt::black, Qt::white));
		ui.actionNew->setIcon(_ColoredIcon(_iconNew, Qt::black, Qt::white));
		ui.actionLoad->setIcon(_ColoredIcon(_iconOpen, Qt::black, Qt::white));
		ui.actionRedo->setIcon(_ColoredIcon(_iconRedo, Qt::black, Qt::white));
		ui.actionUndo->setIcon(_ColoredIcon(_iconUndo, Qt::black, Qt::white));
		ui.actionPrint->setIcon(_ColoredIcon(_iconPrint, Qt::black, Qt::white));
		ui.actionSave->setIcon(_ColoredIcon(_iconSave, Qt::black, Qt::white));
		ui.action_Screenshot->setIcon(_ColoredIcon(_iconScreenShot, Qt::black, Qt::white));
	}
	else
	{
		ui.actionExit->setIcon(_iconExit);
		ui.actionEraser->setIcon(_iconEraser);
		ui.actionNew->setIcon(_iconNew);
		ui.actionLoad->setIcon(_iconOpen);
		ui.actionPrint->setIcon(_iconPrint);
		ui.actionRedo->setIcon(_iconRedo);
		ui.actionUndo->setIcon(_iconUndo);
		ui.actionSave->setIcon(_iconSave);
		ui.action_Screenshot->setIcon(_iconScreenShot);
	}
}

void FalconBoard::_SetupMode(ScreenMode mode)
{
    QString ss,
        sPng = ".png";

    _screenMode = mode;

    _UncheckOtherModes(mode);

    QString actions[] = {
            "close",        // 0
            "eraser",       // 1
            "new",          // 2
            "open",         // 3
            "redo",         // 4
            "save",         // 5
            "screenshot",   // 6
            "undo"          // 7
    };

    _SetupIconsForPenColors();      // pen selection

    switch (mode)
    {
        default:
        case ScreenMode::smSystem:
            _sGridColor = "#d0d0d0";            // drawn by the app and not the system
            _sPageGuideColor = "#fcd475";       // ditto
            _sMarginColor = "#E0E0E0";          // must be set as style
            break;
        case ScreenMode::smLight:
            _sBackgroundColor = "#F0F0F0";
            _sBackgroundHighlightColor = "#D8EAF9";
            _sDisabledColor = "#A0A0A0";
            _sEditBackgroundColor = "#F0F0F0",
            _sEditTextColor = "#000000",
            _sGridColor = "#d0d0d0";
            _sMarginColor = "#EeEeEe",
            _sPageGuideColor = "#fcd475";
            _sPressedBackground = "#0000bb",
            _sSelectedBackgroundColor = "#007acc",
            _sToolBarColor = "#F0F0F0";
            _sTabBarActiveTextColor = "#B30000",
            _sTabBarInactiveTextColor = "#808080";
            _sTextColor = "#000000";
            _sToolTipTextColor = "#000000";
            _sToolTipBackground = "#cccccc";
            _sUnselectedBackgroundColor = "#d0d0d0";
                break;
        case ScreenMode::smWhite:
            _sBackgroundColor = "#FFFFFF";
            _sBackgroundHighlightColor = "#D8EAF9";
            _sSelectedBackgroundColor = "#007acc",
            _sEditBackgroundColor = "#FFFFFF",
            _sEditTextColor = "#000000",
            _sDisabledColor = "#AAAAAA";
            _sMarginColor = "#fafafa",
            _sGridColor = "#E0E0E0";
            _sPageGuideColor = "#fcd475";
            _sPressedBackground = "#0000bb",
            _sToolBarColor = "#FFFFFF";
            _sTabBarActiveTextColor = "#B30000",
            _sTabBarInactiveTextColor = "#808080";
            _sTextColor = "#000000";
            _sToolTipTextColor = "#000000";
            _sToolTipBackground = "#cccccc";
            _sUnselectedBackgroundColor = "#d0d0d0";
            break;
        case ScreenMode::smDark:
            // already set : _drawArea->globalDrawColors.SetDarkMode(true);
            // already set : ui.actionPenBlackOrWhite->setIcon(_ColoredIcon(_iconPen, Qt::black)); // white
            _sBackgroundColor = "#282828";
            _sBackgroundHighlightColor = "#D8EAF9";
            _sEditBackgroundColor = "#555",
            _sEditTextColor = "#ffffff",
            _sDisabledColor = "#888888";
            _sGridColor = "#202020";
            _sMarginColor = "#252525",
            _sPageGuideColor = "#413006";
            _sPressedBackground = "#888888";
            _sSelectedBackgroundColor = "#007acc";
            _sTextColor = "#E1E1E1";
            _sToolBarColor = "#202020";
            _sTabBarActiveTextColor = "#FFA0A0",
            _sTabBarInactiveTextColor = "#808080";
            _sToolTipTextColor = "#000000";
            _sToolTipBackground = "#cccccc";
            _sUnselectedBackgroundColor = "#303030";
            break;
        case ScreenMode::smBlack:
            // already set : _drawArea->globalDrawColors.SetDarkMode(true);
            // already set : ui.actionPenBlackOrWhite->setIcon(_ColoredIcon(_iconPen, Qt::black));     // white
            _sBackgroundColor = "#000000";
            _sBackgroundHighlightColor = "#D8EAF9";
            _sEditBackgroundColor = "#FFFFFF",
            _sDisabledColor = "#888888";
            _sEditTextColor = "#000000",
            _sGridColor = "#303030";
            _sMarginColor = "#101010",
            _sPageGuideColor = "#2e2204";
            _sPressedBackground = "#888888";
            _sSelectedBackgroundColor = "#007acc";
            _sTextColor = "#CCCCCC";
            _sToolBarColor = "#101010";
            _sTabBarActiveTextColor = "#FFA0A0",
            _sTabBarInactiveTextColor = "#808080";
            _sToolTipTextColor = "#888888";
            _sToolTipBackground = "#101010";
            _sUnselectedBackgroundColor = "#101010";
            break;
    }

    _drawArea->SetMode(mode, _sBackgroundColor, _sGridColor, _sPageGuideColor);
    _PrepareActionIcons();

    if(_eraserOn)
        _drawArea->SetCursor(csEraser);

    if (mode == ScreenMode::smSystem)
        ss = "QWidget#marginWidget {\n"
        "  border-right: 1px solid " + _sTextColor + ";\n"
        "  background-color:" + _sMarginColor + ";\n"
        "}\n"
        ;

    else
        ss = "* {\n" 
                  "  background-color:" + _sBackgroundColor + ";\n"
                  "  color:" + _sTextColor + ";\n"
            "}\n"

            "QComboBox {"
                "  background-color:" + _sEditBackgroundColor + ";\n"
                "  color:" + _sEditTextColor + ";\n"
            "}\n"

            "QMenu::separator {  height:1px;\n  margin:1px 3px;\n  background:" + _sTextColor + "\n}\n"
            "QMenuBar::item, "
            "QMenu::item {\n"
            "  color:" + _sTextColor + ";\n"
            "  background-color:" + _sBackgroundColor + ";\n"
            "}\n"
            "QMenuBar::item:selected,\n"
            "QMenu::item:selected {\n"
            "  color:" + _sBackgroundColor + ";\n"
            "  background-color:" + _sTextColor + ";\n"
            "}\n"
            "QMenu::item:disabled {\n"
            "  color:" + _sDisabledColor + ";\n"
            "}\n"

            "QSpinBox {\n"
            "  background-color:" + _sBackgroundColor + ";\n"
            "  color:" + _sTextColor + ";\n"
            "}\n"
            //"QSpinBox::up-button,QSpinBox::down-button, QSpinBox::up-arrow,QSpinBox::down-arrow {\n"
            //"  color:"+_sBackgroundColor + ";\n"
            //"  background-color:" + _sTextColor + ";\n"
            //"}\n"

            "QLineEdit {\n"
            "  background-color:" + _sEditBackgroundColor + ";\n"
            "  color:" + _sEditTextColor + ";\n"
            "  border: 1px solid" + _sGridColor + ";\n"
            "  border-radius:4px;"
            "  padding:1px 2px;\n"
            "}\n"

            "QGroupBox{\n"
            "  margin-top:2em;"
            "  border: 1px solid" + _sBackgroundHighlightColor + ";\n"
            "  border-radius:4px;\n"
            "}\n"
            "QGroupBox::title {\n"
            "  subcontrol-origin: margin;\n"
            "  subcontrol-position: 50%;\n"
            "  padding:4px;\n"
            //            "  margin-top:2em;"
            "}\n"

            "QPushButton {\n"
            "  padding: 2px;\n"
            "  margin: 2px;\n"
            "  border: 2px solid" + _sGridColor + ";"
            "\n}\n"
            "QPushButton:hover {\n  background-color:" + _sSelectedBackgroundColor + ";\n}\n"

            "QToolButton:pressed {\n"
            "  background-color:" + _sPressedBackground + ";\n"
            "  border: none;\n"
            "}\n"

            "QToolButton:hover {\n  "
            "  border:1px solid " + _sTabBarActiveTextColor + ";\n"
            "}\n"

            "QToolBar:disabled, QToolButton:disabled {\n"
            "  color:" + _sDisabledColor + ";\n"
            "}\n"

            "QStatusBar, QToolBar {\n"
            " background-color:" + _sToolBarColor + ";\n"
            " border:1px solid " + _sGridColor + ";\n"
            "}\n"

            "QTabBar::tab {\n"
            "  color:" + _sTabBarActiveTextColor + ";\n"
            "  background-color:" + _sBackgroundColor + ";\n"
            "  selection-background-color:" + _sSelectedBackgroundColor + ";\n"
            "}\n"
            "QTabBar::tab{\n"
            "  border-top-left-radius: 4px;\n"
            "  border-top-right-radius: 4px;\n"
            "  min-width: 60px;"
            "  padding: 2px 4px;\n"
            "  border:1px solid " + _sTabBarActiveTextColor + ";\n"
            "}\n"
            "QTabBar::tab:!selected{"
            "  color:" + _sTabBarInactiveTextColor + ";\n"
            "  background-color:" + _sUnselectedBackgroundColor + ";\n"
            "  margin-top: 2px;"
            "  border:1px solid " + _sTabBarInactiveTextColor + ";\n"
            "}\n"
            "QTabBar:selected {\n"
            "  background-color:" + _sSelectedBackgroundColor + ";\n"
            "  color:" + _sBackgroundColor + ";\n"
            "}\n"

            "QToolTip {\n"
            "  background-color:" + _sToolTipBackground + ";\n"
            "  color:" + _sToolTipTextColor + ";\n"
            "  border 1px solid " + _sToolTipTextColor + ";\n"
            "}\n"

            "QWidget#marginWidget {\n"
            "  border-right: 1px solid " + _sTextColor + ";\n"
            "  background-color:" + _sMarginColor + ";\n"
                        "}\n"
            ;

    ((QApplication*)(QApplication::instance()))->setStyleSheet(ss); // so it cascades down to all sub windows/dialogs, etc
// DEBUG
    //{
    //    QFile f("styleSheet.txt");
    //    f.open(QIODevice::WriteOnly);
    //    QTextStream ofs(&f);
    //    ofs << ss;
    //}
// /DEBUG
}

void FalconBoard::_ClearRecentMenu()
{
    // remove actions
    ui.actionRecentDocuments->setEnabled( !_recentList.isEmpty() );
    QList<QAction*> actList = ui.actionRecentDocuments->actions();
    for(int i = 0; i < actList.size(); ++i)
    {
        _signalMapper.removeMappings(actList[i]);
        disconnect(actList[i], SIGNAL(triggered()), &_signalMapper, SLOT(map()));
    }
    ui.actionRecentDocuments->clear();
}

void FalconBoard::_PopulateRecentMenu()
{
    if (_busy)
        return;

    _ClearRecentMenu();
    if (_recentList.isEmpty())
        return;

//*    QMenu* pMenu;
    QAction* pAction;

    // add items

    QString s;
    for (int i = 0; i < _recentList.size(); ++i)
    {
        s = QString("&%1. %2").arg(i + 1).arg(_recentList[i]);
        pAction = ui.actionRecentDocuments->addAction(s);
        connect(pAction, SIGNAL(triggered()), &_signalMapper, SLOT(map()));
        _signalMapper.setMapping(pAction, i);
    }
    if (_recentList.size())
    {        // then add 'clear list' menu with separator
        ui.actionRecentDocuments->addSeparator();
        pAction = ui.actionRecentDocuments->addAction(tr("C&lear list"), this, &FalconBoard::on_actionCleaRecentList_triggered);
    }
}

void FalconBoard::_PopulateLanguageMenu()
{
    if (_busy || _translations.isEmpty())
        return;

//*    QMenu* pMenu;
    QAction* pAction;
    QString s;
                // after new translation added add Language names here
                // use as many elements as you have languages
                // in the ABC order of their 5 character names
                //                  en_US       hu_HU
    static QStringList s_names = { "English", "Magyar" };   

    ui.actionLanguages->setEnabled(_translations.size() > 1);

    for (int i = 0; i < s_names.size(); ++i)
    {
        s = s_names[i];
        pAction = ui.actionLanguages->addAction(s);
        pAction->setCheckable(true);
        if (i == _actLanguage)
            pAction->setChecked(true);

        connect(pAction, SIGNAL(triggered()), &_languageMapper, SLOT(map()));
        _languageMapper.setMapping(pAction, i);
    }
}

void FalconBoard::_SetWindowTitle(QString qs)
{
    if (qs.isEmpty())
        qs = sWindowTitle;
    else
        qs = sWindowTitle + QString(" - %1").arg(qs);
#ifdef _DEBUG
    qs += QString("- Debug version");
#endif
    setWindowTitle(qs);
}

/*=============================================================
 * TASK   : when the program closed either asks for save or
 *          autosaves or keeps/saves snapshots of changed tabs
 * PARAMS :
 * EXPECTS:
 * GLOBALS: actions, _pSnapshotter, _saveResult, _saveCount
 * RETURNS:
 * REMARKS:
 *------------------------------------------------------------*/
void FalconBoard::closeEvent(QCloseEvent* event)
{
    emit  SignalToCloseServer();
    _listenerThread.quit();
#ifndef _VIEWER
    if (ui.actionAutoSaveBackgroundImage->isChecked())
        _SaveBackgroundImage();

    _snapshotTimer.stop();        // no more autosave

    size_t tout = std::numeric_limits<size_t>::max();  // timout
    while (tout-- && _pSnapshotter) // wait for snapshot save
        ;

    IntVector savedTabs;
    // save all changed
    bool bAuto = ui.actionAutoSaveData->isChecked();
    int n = -1;
    SaveResult sr = srSaveSuccess;

    int waitCounter =0;

	auto waitforsnapshot = [this]() -> int  // lambda for waiting for snapshot save
		{
            int i = 0;
		    while (_pSnapshotter && i++ < SLEEP_COUNTER) // wait for snapshot save for max 3 seconds
			    std::this_thread::sleep_for(SLEEP_DURATION);
			return i;
		};
#ifndef _VIEWER
    if (ui.actionKeepChangedNoAsking->isChecked())
    {
		savedTabs = historyList.ModifiedItems();
        waitCounter = waitforsnapshot();
        if (waitCounter != SLEEP_COUNTER)
        {
            _StartSnapshotSaveThread();
            waitCounter = waitforsnapshot();
        }

		if (waitCounter == 1000)
        {
            QMessageBox::warning(this, FB_WARNING, tr("Automatic snapshot save not finished in 30 seconds!\nPlease save each changed files manually!\n\nAborting close."));
			event->ignore();
            return;
        }
    }
    else
#endif
    {
        while (sr != srCancelled && _drawArea->SearchForModified(n))    // returned : n = (index of first modified after 'n') || 0
        {
            if ((sr = _SaveIfYouWant(--n, !bAuto, true)) == srSaveSuccess || sr == srNoNeedToSave)
                savedTabs.push_back(n);
        }
    }
    _saveCount = savedTabs.size();

    _saveResult = (sr != srCancelled ? srSaveSuccess :srCancelled);   // even when some was not saved

    if (_saveResult == srSaveSuccess ) 
    {
        SaveState();
        SaveTabState(savedTabs);
        event->accept();
    }
    else
    {
        _snapshotTimer.start();
        event->ignore();
    }
#else
    SaveState();        // viewer: always
#endif
}

void FalconBoard::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);

    if (!_firstShown)
    {
        _firstShown = true;
        if (_pTabs->count())
        {
            int n = _nLastTab;      // must switch to this tab
            if (_pTabs->count() == 1)
            {
                _drawArea->SwitchToHistory(0, true);
                ui.actionAppend->setEnabled(!pHistory->Name().isEmpty());
            }
            else if(_nLastTab == _pTabs->currentIndex())
            {
                _dontCareForTabChange = true;
                _pTabs->setCurrentIndex(n ? 0 : 1); // move selection so next selection will work

            }
            _pTabs->setCurrentIndex(n);     // select last used tab and load data
            _drawArea->EnableRedraw(true);
//            _LoadData(_nLastTab);   // only load first tab
    // DEBUG
            n = _pTabs->count();
//            _pTabs->setAutohide(true);
        }
        else
            _drawArea->EnableRedraw(true);
    }
    _drawArea->setFocus();
}

void FalconBoard::focusInEvent(QFocusEvent* event)
{
    _drawArea->setFocus();
}

/*============================================================================
 * TASK: The dragEnterEvent() is called whenever the user drags an object onto
 *		  a widget.
 * EXPECTS: pointer to the event
 * RETURNS: nothing
 * REMARKS: - If we call acceptProposedAction() on the event, we indicate
 *        that the user can drop the drag object on this widget. By default,
 *        the widget wouldn't accept the drag. Qt automatically changes the
 *        cursor to indicate to the user whether the widget is a legitimate drop site.
 *          - Here we want the user to be allowed to drag files, but nothing
 *		  else. To do so, we check the MIME type of the drag. The MIME type
 *        text/uri-list is used to store a list of uniform resource identifiers
 *        (URIs), which can be file names, URLs (such as HTTP or FTP paths), or
 *        other global resource identifiers. Standard MIME types are defined by the
 *        Internet Assigned Numbers Authority (IANA). They consist of a type and a
 *        subtype separated by a slash. The clipboard and the drag and drop system
 *        use MIME types to identify different types of data. The official list of
 *        MIME types is available at http://www.iana.org/assignments/media-types/.
 *---------------------------------------------------------------------------*/
void FalconBoard::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat("text/uri-list"))
    {
        event->acceptProposedAction();
    }
}

/*============================================================================
* TASK: The dropEvent() is called when the user drops an object onto the widget.
* EXPECTS: pointer to the event
* RETURNS: nothing
* REMARKS: - We call QMimeData::urls() to obtain a list of QUrls. Typically, users
*        drag only one file at a time, but it is possible for them to drag multiple
*        files by dragging a selection.
*          - If the URL is not a local file name, we return immediately.
*		   - If Alt is kept pressed curves in memory are discarded
*---------------------------------------------------------------------------*/
void FalconBoard::dropEvent(QDropEvent* event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;
    QString fname;
    QStringList flist;
    for (auto it = 0; it != urls.size(); ++it)
    {
        fname = urls[it].toLocalFile();
        if (!fname.isEmpty())
        {
            QString ext = fname.right(4);
            if (ext == ".mwb")
                flist << fname;
        }
    }
    _LoadFiles(flist);
    event->acceptProposedAction();
}

#ifndef _VIEWER
void FalconBoard::on_actionNew_triggered()
{
    if (_pTabs->count() == MAX_NUMBER_OF_TABS)
    {
        QMessageBox::warning(this, "falconBoard", tr("Maximum mumber of TABs reached. Please close some TABs to proceed."));
        return;
    }
// no need to save: creating new TAB
//    if (!_SaveIfYouWant(true))    // must ask if data changed, false: cancelled
//        return;

    bool b = QMessageBox::question(this, "falconBoard", tr("Do you want to limit the editable area horizontally to the pixel width set in Page Setup?\n"
                                                           " You may change this any time in 'Options/Limit Paper Width'"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
    ui.actionLimitPaperWidth->setChecked(b);
    _drawArea->SetLimitedPage(b);

    _AddNewTab(QString(), false, true);
    ui.actionAppend->setEnabled(false);

    //_drawArea->NewData();
    setWindowTitle(sWindowTitle);
    _backgroundImageName.clear();
}
void FalconBoard::on_actionImportImage_triggered()
{
    QString s = QFileDialog::getOpenFileName(this, tr("FalconBoard - Import"), _lastDir, "Images (*.jpg *.bmp *.png *.gif);;All files (*)");
    if (!s.isEmpty())
        _drawArea->AddImage(s);
}
#endif

bool FalconBoard::_LoadData(int index)
{
    _drawArea->EnableRedraw(false);
    bool res = _drawArea->SwitchToHistory(index, true); // and loads into current history

    if (res)     // loaded
    {
#ifndef _VIEWER
        _SetPenKind();
#endif
        _AddToRecentList(historyList[index]->Name());
        ui.actionAppend->setEnabled(true);
    }
    else
        ui.actionAppend->setEnabled(false);

    _drawArea->EnableRedraw(true);

    return res;
}

/*=============================================================
 * TASK:    either add new name to recent files list or
 *          move it on top of history
 * PARAMS:
 * GLOBALS: _recentList
 * RETURNS: nothing
 * REMARKS:
 *------------------------------------------------------------*/
void FalconBoard::_AddToRecentList(QString path)
{
    if (path.isEmpty())
        return;

    int i = _recentList.indexOf(path);
    if (i == 0)                    // at top
        return;

    if (i > 0)
        _recentList.removeAt(i);
    else    // i < 0: new document
    {
        if (_recentList.size() == 9) // max number of recent files
            _recentList.pop_back();
    }

    _recentList.push_front(path);

    _PopulateRecentMenu();
}

/*=============================================================
 * TASK:    loads files into the program
 * PARAMS:  names - list of '.mwb' file names
 * GLOBALS: _pTabs, _eraserOn
 * RETURNS:
 * REMARKS:
 *------------------------------------------------------------*/
void FalconBoard::_LoadFiles(QStringList names)
{
    if (!names.isEmpty())
    {
        QString lastName;
        int nLimit = MAX_NUMBER_OF_TABS - (_pTabs->count() - (IsOverwritable() ? 1 : 0));
        if (_pTabs->count() == MAX_NUMBER_OF_TABS)
        {
            QMessageBox::warning(this, FB_WARNING,
                                       tr("Maximum number of files reached, no new files can be loaded."));
            return;
        }
        else if (_pTabs->count() + names.size() > MAX_NUMBER_OF_TABS)
        {
            QMessageBox::warning(this, FB_WARNING,
                QString(tr("Possibly too many files! \nOnly the first %1 valid, and not already loaded files will be loaded.").arg(nLimit)));
            return;
        }

        bool bLast = false; // last name loaded?
        for (int i =0; i < names.size() && nLimit; ++i)
        {
            QString fileName = names[i];
            int n;
            bLast = i == names.size() - 1 || nLimit==1;
            if ((n = _drawArea->SameFileAlreadyUsed(fileName)) >= 0)
            {
                if (bLast)
                {
                    _pTabs->setCurrentIndex(n);
                    lastName.clear();
                }
            }
            else
            {
                n = _pTabs->currentIndex();
                if (!IsOverwritable())
                    n = _AddNewTab(fileName, bLast);   // sets current tab and adds history item and set it to current too
                else
                    pHistory->SetName(fileName,true);

                if (n >= 0 && _LoadData(-1)) // no load error
                    --nLimit, lastName = fileName, _SetTabText(n, fileName);    // comma !
            }
        }
        if (!lastName.isEmpty())
        {
            _SaveLastDirectory(lastName);
            _SetWindowTitle(lastName);
        }
    }
#ifndef _VIEWER
    if (_eraserOn)
        on_actionEraser_triggered();
#endif
}

/*========================================================
 * TASK:    Loads file into actual TAB, if it is empty or 
 *             into new TAB if not
 * PARAMS:
 * GLOBALS:
 * RETURNS:
 * REMARKS: -
 *-------------------------------------------------------*/
void FalconBoard::on_actionLoad_triggered()
{
//#ifndef _VIEWER
//    _SaveIfYouWant(true);   // must ask when data changed
//#endif
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Load Data"), 
                                                    _lastDir, // QDir::currentPath(),
                                                    tr("FalconBoard files (*.mwb);;All files (*)"));
    if (fileName.isEmpty())     // cancelled
        return;

    _SaveLastDirectory(fileName);
    //int n = _pTabs->currentIndex();
    if (IsOverwritable())
    {
        pHistory->SetName(fileName,true);
        _SetTabText(-1, fileName);
        _drawArea->Load();
    }
    else
        /*n =*/(void) _AddNewTab(fileName, true);   // sets current tab and adds history item and set it to current too
                            
    _SetWindowTitle(fileName);

#ifndef _VIEWER
    if (_eraserOn)
        on_actionEraser_triggered();
#endif
}

void FalconBoard::on_action_Close_triggered()
{
    SlotForTabCloseRequested(_drawArea->ActHistoryIndex());
}

void FalconBoard::on_action_CloseAll_triggered()
{
    int n = -1,
        nAct = _drawArea->ActHistoryIndex();
    while (_drawArea->SearchForModified(n))
    {
        _drawArea->SwitchToHistory(n, false, false);
        on_action_Close_triggered();
    }
    _drawArea->SwitchToHistory(nAct, false, false);
}

void FalconBoard::on_actionGoToPage_triggered()
{
    bool ok = false;
    int n = QInputDialog::getInt(this, tr("FalconBoard - Go to Page"), tr("Page:"), 1, 1, 1000000, 1, &ok);
    if(ok)
        _drawArea->GotoPage(n);
}

void FalconBoard::on_actionCleaRecentList_triggered()
{
    ui.actionRecentDocuments->clear();
    ui.actionRecentDocuments->setEnabled(false);
    _recentList.clear();
    _ClearRecentMenu();
}

void FalconBoard::_sa_actionRecentFile_triggered(int which)
{
//    _busy = true;   // do not delete signal mapper during a mapping
        // viewer: open in new tab unless this is the first one
        // and no file has been opened yet

    bool bOverwritable = IsOverwritable();
    QString fileName = _recentList[which];     // can't be reference: recent list changes in loaddata
    _SaveLastDirectory(fileName);
    bool res = true;
    if (bOverwritable)  // then load into current tab 
    {
        pHistory->SetName(fileName,true);
        if (!_LoadData())        // to current TAB 
            fileName.clear();
        else
            _SetTabText(-1, fileName);
    }
    else              // try to add a new TAB and load data there
    {
        int n;
        if ((n = _drawArea->SameFileAlreadyUsed(fileName)) >= 0)
        {
            _pTabs->setCurrentIndex(n);
            _nLastTab = n;
        }
        else if (_drawArea->HistoryListSize() < MAX_NUMBER_OF_TABS)
            res = _AddNewTab(fileName) >= 0;   // also appends history and loads from file
    }
    if(res)
        _SetWindowTitle(fileName);
//    _busy = false;
}

void FalconBoard::_sa_actionLanguage_triggered(int which)
{
    // TODO: language change w.o. restart
    if (which != _actLanguage)
    {
        QMessageBox::warning(this, FB_WARNING, tr("Please restart the program to change the language!").arg(which));
        _actLanguage = which;
    }
}

void FalconBoard::_SaveLastDirectory(QString fileName)
{
    if (!fileName.isEmpty())
    {
        int n = fileName.lastIndexOf('/');
        _lastDir = fileName.left(n + 1);
// using _nLastTab instead        _lastFile = fileName.mid(n + 1);
    }
}

#ifndef _VIEWER
void FalconBoard::on_actionSave_triggered()
{
    bool wasRunning = _StopSnapshotTimerAndWaitForIt();
    QString fname = pHistory->Name();
    if (fname.isEmpty())
        on_actionSaveAs_triggered();
    else
    {
        _SaveFile(fname);
        _SetTabText(-1, fname);
        _SetResetChangedMark(-1);
    }
    if (wasRunning)
        _snapshotTimer.start();
}
void FalconBoard::on_actionSaveAs_triggered() // current tab
{
    QString fname = _pTabs->tabText(_pTabs->currentIndex());//    _drawArea->HistoryName(UNTITLED);
    //int n = fname.length() - 1;

    QString initialPath = _lastDir + fname; 
    QString fileName;
    bool askForName = true;
    while(askForName)
    {
        fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
            initialPath, tr("FalconBoard Files (*.mwb);; All Files (*))"));
        if (fileName.isEmpty())
        {
            _saveResult = srCancelled;
            return;
        }
        askForName = false; // if name is not a name of another open document then break the loop
        for (size_t i = 0; i < historyList.size(); ++i)
        {
            if (i != (size_t)historyList.ActualHistory() && historyList[i]->Name() == fileName)
            {
                QMessageBox::StandardButton res = QMessageBox::warning(this, tr("FalconBoard - Warning"), 
                                        tr("This file path name is already used for another open document.\nPlease enter a different one"));
                if (res == QMessageBox::Escape || res == QMessageBox::Cancel)
                    return;
                askForName = true;
                break;
            }
        }
    }
    _SaveLastDirectory(fileName);
    _SaveFile(fileName);    // and sets _SaveResult
    setWindowTitle(sWindowTitle + QString(" - %1").arg(fileName));
    _SetTabText(-1, fileName);
    _SetResetChangedMark(-1);
}

void FalconBoard::on_actionAppend_triggered()
{
    QStringList fileNames = QFileDialog::getOpenFileNames(this,
        tr("Load Data"),
        _lastDir, // QDir::currentPath(),
        tr("FalconBoard files (*.mwb);;All files (*)"));
    if (fileNames.isEmpty())     // cancelled
        return;

    _SaveLastDirectory(fileNames[0]);
    _drawArea->Append(fileNames);
#ifndef _VIEWER
    if (_eraserOn)
        on_actionEraser_triggered();
#endif
}

void FalconBoard::on_actionLoadBackground_triggered()
{
    _sImageName = QFileDialog::getOpenFileName(this,
                    tr("Open Background Image"), QDir::currentPath());
    if (!_sImageName.isEmpty())
        _drawArea->OpenBackgroundImage(_sImageName);
}

void FalconBoard::on_actionSaveVisible_triggered()
{
    if (_backgroundImageName.isEmpty())
        SaveVisibleAsTriggered();
    else
        _SaveBackgroundImage();
}

void FalconBoard::SaveVisibleAsTriggered()
{
    QAction* action = qobject_cast<QAction*>(sender());
    _fileFormat = action->data().toByteArray();
    if (_fileFormat.isEmpty())
        _fileFormat = "png";

    QString initialPath = QDir::currentPath() + "/" + UNTITLED+"." + _fileFormat;

    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
        initialPath,
        tr("%1 Files (*.%2);;All Files (*)")
        .arg(QString::fromLatin1(_fileFormat.toUpper()))
        .arg(QString::fromLatin1(_fileFormat)));
    if (fileName.isEmpty())
        return ;
    _backgroundImageName = fileName;
    _SaveBackgroundImage();
}
#endif

void FalconBoard::on_actionAbout_triggered()
{
    QMessageBox::about(this, tr("About FalconBoard"),
        tr("Open source Whiteboard/blackboard application")+
            tr("<p>Version ") + sVersion + QString("</p>")+
            tr("<p>© A. Sólyom (2020-22)</p><br>"
            "<p>https://github.com/solyoma/FalconBoard</p>"
            "<p>Based on Qt's <b>Scribble</b> example.</p>"
            "<p>QuadTree code from <b>https://github.com/pvigier/Quadtree</b>"));
}

void FalconBoard::on_actionHelp_triggered()
{
    HelpDialog hd;
    hd.exec();
}

void FalconBoard::on_actionSystemMode_triggered()
{
    _SetupMode(ScreenMode::smSystem);
}

void FalconBoard::on_actionLightMode_triggered()
{
    _SetupMode(ScreenMode::smLight);
}

void FalconBoard::on_actionWhiteMode_triggered()
{
    _SetupMode(ScreenMode::smWhite);
}

void FalconBoard::on_actionDarkMode_triggered()
{
    _SetupMode(ScreenMode::smDark);
}

void FalconBoard::on_actionBlackMode_triggered()
{
    _SetupMode(ScreenMode::smBlack);
}

void FalconBoard::on_actionAllowMultipleProgramInstances_triggered()
{
    QSettings* s = FBSettings::Open(); 
    s->setValue(SINGLE, !ui.actionAllowMultipleProgramInstances->isChecked());
    FBSettings::Close();
}

void FalconBoard::SlotForAddNewTab(QString name)
{
    name = QDir::fromNativeSeparators(name);
    bool bOverwritable = IsOverwritable();
    if (bOverwritable)  // then load into current tab 
    {
        pHistory->SetName(name, true);
        if (!_LoadData())        // to current TAB 
            name.clear();
        else
            _SetTabText(-1, name);
    }
    else
    {
        int n;
        if ((n = _drawArea->SameFileAlreadyUsed(name)) >= 0)
        {
            _pTabs->setCurrentIndex(n);
            _nLastTab = n;
        }
        else if (_drawArea->HistoryListSize() < MAX_NUMBER_OF_TABS)
            n = _AddNewTab(name);   // also appends history and loads from file
    }
}

void FalconBoard::SlotForTabChanged(int index) // index <0 =>invalidate tab
{
    // this function is called only when the _pTabs->currentTab() changes
    if (!_pTabs->count())
        return;

    QString qsTabName;
    if (_dontCareForTabChange) // so it won't reflect in history change or loading data
    {
        _drawArea->SwitchToHistory(index, true, true); // invalidate 
        _dontCareForTabChange = false;      // care next time
    }
    else
    {
        if (_drawArea->SwitchToHistory(index, true))    // then pHistory is st
            qsTabName = pHistory->Name(UNTITLED);
        else
            qsTabName = UNTITLED;
         _SetWindowTitle(qsTabName);

        _SetupIconsForPenColors();   // don't touch black pen
    }
    ui.actionAppend->setEnabled(pHistory ? !pHistory->Name().isEmpty() : false);
    _nLastTab = index;
}

void FalconBoard::SlotForTabCloseRequested(int index)
{
#ifndef _VIEWER
    _drawArea->HideRubberBand(true);
    if (!_drawArea->IsModified(index, true) || _SaveIfYouWant(index, true, true)!= srCancelled)
    {
#endif
        _CloseTab(index);
        // if the deleted tab was the actual one or
        //  one above then SlotForTabChanged will load 
        // the new data, else data has not been changed
        // and nothing to do
#ifndef _VIEWER
    }
#endif
}

void FalconBoard::SlotForTabMoved(int from, int to)
{
    _drawArea->SwapHistories(from, to); 
}

void FalconBoard::SlotForTabSwitched(int direction)
{
    int n = _pTabs->currentIndex(), N = _pTabs->count();
    n += direction;
    if (direction > 0)
    {
        if (n == N)
            n = 0;
    }
    else if (n < 0)
        n = N - 1;

    _pTabs->setCurrentIndex(n);
}

void FalconBoard::SlotForActivate()
{
    raise();
    activateWindow();
}

void FalconBoard::on_actionLeftMargin_triggered()
{
    if (_busy)
        return;
    ++_busy;
    ui.marginWidget->setVisible(ui.actionLeftMargin->isChecked());
    --_busy;
}

void FalconBoard::on_actionShowGrid_triggered()
{
    if (_busy)
        return;
    ++_busy;
    _drawArea->SetGridOn(ui.actionShowGrid->isChecked(), ui.actionFixedGrid->isChecked());
    _pChkGridOn->setChecked(ui.actionShowGrid->isChecked());
    --_busy;
}

void FalconBoard::on_actionFixedGrid_triggered()
{
    if (_busy)
        return;
    _drawArea->SetGridOn(ui.actionShowGrid->isChecked(), ui.actionFixedGrid->isChecked());
}

void FalconBoard::on_actionGridSize_triggered()
{
    bool ok;
    int n = QInputDialog::getInt(this, tr("falconBoard - Grid spacing"),
                                         tr("Spacing in pixels:"), _nGridSpacing,
                                         5, 400, 10, &ok);
    if (ok && n != _nGridSpacing)
    {
        _nGridSpacing = n;
        emit SignalGridSpacingChanged(n);
    }
            
}

void FalconBoard::SlotToSetGrid(bool on, bool fixed, uint16_t value)
{
#ifndef _VIEWER
    ++_busy;
    _nGridSpacing = value;
    _psbGridSpacing->setValue(_nGridSpacing);
    ui.actionShowGrid->setChecked(on);
    ui.actionFixedGrid->setChecked(fixed);
    --_busy;
#endif
}
void FalconBoard::slotGridSpacingChanged(int val)
{
    if (_busy)		// from program
        return;
    // from user
    _nGridSpacing = val;
    emit SignalGridSpacingChanged(val);
}

void FalconBoard::slotGridSpacingEditingFinished()
{
    ui.drawArea->setFocus();
}


void FalconBoard::on_actionShowPageGuides_triggered()
{
    _drawArea->SetPageGuidesOn(ui.actionShowPageGuides->isChecked());
}

void FalconBoard::on_actionAutocorrectTolerance_triggered()
{
    bool ok=false;
    int tol = QInputDialog::getInt(this, tr("FalconBoard - settings"), tr("Autocorrect tolerance in pixels"), 6, 0, 99, 1, &ok);
    if (ok)
        _drawArea->SetAutoCorrectLimit(tol);
}

#ifndef _VIEWER

void FalconBoard::_SetPenCommon(FalconPenKind pk)
{
    _drawArea->RecolorSelected(pk);
    _SetPenKind(pk);
    _SetCursor(csPen);
    _SetPenWidth(pk);
    ui.drawArea->setFocus();
}

void FalconBoard::on_actionPenBlackOrWhite_triggered()
{
    _SetPenCommon(penBlackOrWhite);
}

void FalconBoard::on_actionPenT2_triggered() 
{   
    _SetPenCommon(penT2);
}

void FalconBoard::on_actionPenT3_triggered()
{
    _SetPenCommon(penT3);
}

void FalconBoard::on_actionPenT4_triggered()
{
    _SetPenCommon(penT4);
}

void FalconBoard::on_actionPenT5_triggered()
{
    _SetPenCommon(penT5);
}

void FalconBoard::on_actionPenT6_triggered()
{
    _SetPenCommon(penT6);
}

void FalconBoard::on_actionPenT7_triggered()
{
    _SetPenCommon(penT7);
};

void FalconBoard::on_actionEraser_triggered()
{
    _eraserOn = true;
    _SetPenWidth(_actPen = penEraser);
    _drawArea->SetCursor(csEraser);
    _SelectPenForAction(ui.actionEraser);
	ui.drawArea->setFocus();
}

void FalconBoard::on_actionRotate_triggered()
{
    _drawArea->SynthesizeKeyEvent(Qt::Key_F7);
}

void FalconBoard::on_actionRepeatRotation_triggered()
{
    _drawArea->SynthesizeKeyEvent(Qt::Key_F7, Qt::ShiftModifier);
}

void FalconBoard::on_actionRotateLeft_triggered()
{
    _drawArea->SynthesizeKeyEvent(Qt::Key_0);
}

void FalconBoard::on_actionRotateRight_triggered()
{
    _drawArea->SynthesizeKeyEvent(Qt::Key_9);
}

void FalconBoard::on_actionRotate180_triggered()
{
    _drawArea->SynthesizeKeyEvent(Qt::Key_8);
}

void FalconBoard::on_actionHFlip_triggered()
{
    _drawArea->SynthesizeKeyEvent(Qt::Key_H);
}

void FalconBoard::on_actionVFlip_triggered()
{
    _drawArea->SynthesizeKeyEvent(Qt::Key_V);
}

void FalconBoard::on_actionRectangle_triggered()
{
    _drawArea->SynthesizeKeyEvent(Qt::Key_R);
}

void FalconBoard::on_actionDrawFilledRectangle_triggered()
{
    _drawArea->SynthesizeKeyEvent(Qt::Key_R, Qt::ShiftModifier);
}

void FalconBoard::on_actionDrawEllipse_triggered()
{
    _drawArea->SynthesizeKeyEvent(Qt::Key_C);
}
void FalconBoard::on_actionDrawFilledEllipse_triggered()
{
    _drawArea->SynthesizeKeyEvent(Qt::Key_C, Qt::ShiftModifier);
}

void FalconBoard::on_actionMarkCenterPoint_triggered()
{
    _drawArea->SynthesizeKeyEvent(Qt::Key_Period);
}

void FalconBoard::on_actionXToCenterPoint_triggered()
{
    _drawArea->SynthesizeKeyEvent(Qt::Key_X);
}

void FalconBoard::_DoScreenshot(bool doHide)
{
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    _pSnipper = new Snipper(nullptr);
    _pSnipper->setGeometry(screen->geometry());
    _ConnectScreenshotLabel();

	if (doHide)
        hide();

    QThread::msleep(300);

    _pSnipper->setPixmap(screen->grabWindow(0));

	if (doHide)
        show();
    _pSnipper->show();
}


void FalconBoard::on_action_Screenshot_triggered()
{
    _DoScreenshot(true);
}

void FalconBoard::on_actionScreenshotTransparency_triggered()
{
    _SelectTransparentPixelColor();   // or set it to unused
}

void FalconBoard::on_actionClearRoll_triggered()
{
    _drawArea->ClearRoll();
}

void FalconBoard::on_actionClearThisScreen_triggered()
{
    _drawArea->ClearVisibleScreen();    // from _topLeft
}

void FalconBoard::on_actionClearDownward_triggered()
{
    _drawArea->ClearDown(); // from _topLeft
}

void FalconBoard::on_actionClearBackgroundImage_triggered()
{
    _drawArea->ClearBackground();
    _sImageName.clear();
}

void FalconBoard::on_actionLimitPaperWidth_triggered()
{
    if (_busy)
        return;
    ++_busy;
    bool b = ui.actionLimitPaperWidth->isChecked();
    _drawArea->SetLimitedPage(b);

    --_busy;
}

void FalconBoard::on_actionDefinePenColors_triggered()
{
    if (_busy)
        return;
    ++_busy;
    PenColorsDialog *pcdg = new PenColorsDialog(this);
    if (pcdg->exec() == QDialog::Accepted)
    {
        DrawColors newc(globalDrawColors);  // current colors
        if (pcdg->GetChanges(newc))
        {
            emit SignalPenColorChanged(newc);
        }
    }
    delete pcdg;
    --_busy;
}

void FalconBoard::on_actionUndo_triggered()
{
    _drawArea->Undo();                
    _SelectPen();
    if (_eraserOn)
        on_actionEraser_triggered();
    else
        _SetCursor(csPen);
    _SetResetChangedMark(-1);
}

void FalconBoard::on_actionRedo_triggered()
{
    _drawArea->Redo();
    _SelectPen();
    if (_eraserOn)
        on_actionEraser_triggered();
    else
        _SetCursor(csPen);
}

void FalconBoard::on_action_InsertVertSpace_triggered()
{
    _drawArea->InsertVertSpace();
}

void FalconBoard::on_actionApplyTransparencyToLoaded_triggered()
{
    _drawArea->ApplyTransparencyToLoadedScreenshots(_screenshotTransparencyColor, _transparencyFuzzyness);
}

void FalconBoard::SlotForRubberBandSelection(int on)
{
    ui.action_InsertVertSpace->setEnabled(on);
    ui.menuRotate->setEnabled(on);
    ui.actionRotate->setEnabled(on);
    ui.actionRepeatRotation->setEnabled(on);
    ui.actionRotateLeft->setEnabled(on);
    ui.actionRotateRight->setEnabled(on);
    ui.actionRotate180->setEnabled(on);
    ui.actionHFlip->setEnabled(on);
    ui.actionVFlip->setEnabled(on);
    ui.actionDrawEllipse->setEnabled(on);
    ui.actionDrawFilledEllipse->setEnabled(on);
    ui.actionRectangle->setEnabled(on);
    ui.actionDrawFilledRectangle->setEnabled(on);
    ui.actionMarkCenterPoint->setEnabled(on);
    ui.actionXToCenterPoint->setEnabled(on);
}

void FalconBoard::slotPenWidthChanged(int val)
{
    if (_busy)		// from program
        return;
    // from user
    _penWidths[_actPen] = val;
    _SetPenKind();
}

void FalconBoard::slotPenWidthEditingFinished()
{
    ui.drawArea->setFocus();
}
void FalconBoard::SlotForUndo(bool b)
{
    ui.actionUndo->setEnabled(b);
    _SetResetChangedMark(-1);  // signal modified state
}

void FalconBoard::SlotForRedo(bool b)
{
    ui.actionRedo->setEnabled(b);
    _SetResetChangedMark(-1);  // signal unmodified state
}

void FalconBoard::SlotForFocus()
{
    ui.drawArea->setFocus();
}

void FalconBoard::SlotForPointerType(QTabletEvent::PointerType pt)   // only sent by tablet
{
    static bool isPenEraser = false;     // set to true if not erasemode just
    static FalconPenKind pk = penBlackOrWhite;
    switch (pt)                         // eraser end of stylus used
    {
        case QTabletEvent::Eraser:      // only when eraser side selected
            if (!_eraserOn)
            {
                pk = _actPen;           // save for restoring
                isPenEraser = true;
                _eraserOn = true;
                _SetPenWidth(_actPen = penEraser);
                _SetCursor(csEraser);
            }
            break;
        default:
            if (isPenEraser)
            {
                _SetPenKind(pk);
                _SetCursor(csPen);
                _SetPenWidth(pk);
                _eraserOn = false;
                isPenEraser = false;
            }
            break;
    }
}

void FalconBoard::SlotForScreenshotReady(QRect gmetry)
{
    _pSnipper->hide();

    QPixmap pixmap;  // need a pixmap for transparency (used Qpixmap previously)
    pixmap = QPixmap(gmetry.size()); //  , Qpixmap::Format_ARGB32);

    QPainter *painter = new QPainter(&pixmap);   // need to delete it before the label is deleted
    painter->drawPixmap(QPoint(0,0), _pSnipper->pixmap(Qt::ReturnByValue), gmetry);
    delete painter;

    if (_useScreenshotTransparency)
    {
        QBitmap bm = MyCreateMaskFromColor(pixmap, _screenshotTransparencyColor, _transparencyFuzzyness);
        pixmap.setMask(bm);
    }

    _drawArea->AddScreenShotImage(pixmap);

    _DisconnectScreenshotLabel();
    delete _pSnipper;
}

void FalconBoard::SlotForScreenShotCancelled()
{
    _pSnipper->hide();
    _DisconnectScreenshotLabel();
    delete _pSnipper;
    _pSnipper = nullptr;
}

void FalconBoard::SlotIncreaseBrushSize(int ds)
{
    _psbPenWidth->setValue(_psbPenWidth->value() + ds);
}

void FalconBoard::SlotDecreaseBrushSize(int ds)
{
    _psbPenWidth->setValue(_psbPenWidth->value() - ds);
}

void FalconBoard::SlotForLabel(QString text)
{
    _plblMsg->setText(text);
}
void FalconBoard::SlotForPenKindChange(FalconPenKind pk)
{
    _SetPenKind(pk);
}
#endif  // not _VIEWER

void FalconBoard::SlotForPenColorChanged()      // called from _drawArea after the pen color is redefined
{
    _SetupIconsForPenColors();
//    _SetResetChangedMark(-1);

//    _drawArea->setFocus();
}

void FalconBoard::on_actionPageSetup_triggered()
{
    _drawArea->PageSetup(PageParams::wtdPageSetup);
}

void FalconBoard::on_actionExportToPdf_triggered()
{
#ifndef _VIEWER
    _QDoSaveModified(tr("export"));
#endif
    QString saveName = pHistory->Name();
    int pos = saveName.lastIndexOf('/');
    QString name = _lastPDFDir + saveName.mid(pos+1);
    _drawArea->ExportPdf(name, _lastPDFDir);
}

void FalconBoard::SlotForChkGridOn(bool checked)
{
    if (_busy)
        return;
    on_actionShowGrid_triggered();
}

#ifndef _VIEWER
void FalconBoard::SlotForSnapshotTimer()
{
    if (_pSnapshotter)
        return;
    
    _StartSnapshotSaveThread();
}
void FalconBoard::SlotSnapshotSaverFinished()
{
    _snapshotterThread.quit();
    _snapshotterThread.wait();
    delete _pSnapshotter;
    _pSnapshotter = nullptr;
    _drawArea->SetSnapshotterState(false);
}
void FalconBoard::SlotTakeScreenshot(bool hideThisWindow)
{
	_DoScreenshot(hideThisWindow);
}
#endif

// ========================================================= Snapshotter ==========================================
#ifndef _VIEWER
void Snapshotter::SlotToSaveSnapshots()       // of all changed files runs in separate thread
{
    History* ph = nullptr;
    for (auto i : _whose)
    {
        ph = historyList[i];
        ph->Save(true);
    }
    _falconBoard->SaveTabState();
    emit SignalFinished();
}
#endif