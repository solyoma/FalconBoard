#include <QSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QScreen>
#include <QPainter>
#include <QThread>
#include <QSettings>
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



int nUntitledOrder = 1;

#ifdef _VIEWER
void FalconBoard::_RemoveMenus()
{
    QList<QAction*> pMenuActions = ui.menuBar->actions();
    ui.menuBar->removeAction(pMenuActions[1] ); // edit
    ui.menuBar->removeAction(pMenuActions[2]);  // clear
    ui.actionNew->setVisible(false);
    ui.actionSave->setVisible(false);
    ui.actionSaveAs->setVisible(false);
    ui.actionSaveVisible->setVisible(false);
    ui.actionLoadBackground->setVisible(false);
    ui.actionScreenshotTransparency->setVisible(false);

    ui.actionAutoSaveData->setVisible(false);
    ui.actionAutoSaveBackgroundImage->setVisible(false);
    ui.actionApplyTransparencyToLoaded->setVisible(false);
    ui.menuGrid->removeAction(ui.menuGrid->actions()[2]);
    pMenuActions[3]->menu()->removeAction(pMenuActions[3]->menu()->actions()[1]);

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

FalconBoard::FalconBoard(QWidget *parent)	: QMainWindow(parent)
{
    UNTITLED = QString(QObject::tr("Untitled"));    // set here so itgets translated

	ui.setupUi(this);

    historyList.SetupClipBoard();

    if (!QDir(FBSettings::homePath).exists())
        QDir(FBSettings::homePath).mkdir(FBSettings::homePath);

    _drawArea = static_cast<DrawArea*>(ui.centralWidget);
    _drawArea->SetPenKind(_actPen, _penWidth[_actPen-1]); 
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
#endif
    connect(this, &FalconBoard::GridSpacingChanged, _drawArea, &DrawArea::SlotForGridSpacingChanged);

    connect(qApp, &QApplication::primaryScreenChanged, _drawArea, &DrawArea::SlotForPrimaryScreenChanged);

    RestoreState();     // sets up all history item names, and loads the last used ones

    _SetupIconsForPenColors(_screenMode);
    setAcceptDrops(true);

    ui.centralWidget->setFocus();
}

#ifndef _VIEWER
QString FalconBoard::_NextUntitledName()
{
	int n = -1, m;
	QString qsUntitled = UNTITLED;
	for (int i = 0; i < _pTabs->count(); ++i)
        if (IsUntitled(_pTabs->tabText(i)) == 2)
        {
            m = _pTabs->tabText(i).mid(UNTITLED.length()).toInt();
            if (m < 0)
                ++n;
            else
                n = m + 1;
        }
	if (n > 0)
		qsUntitled += QString().setNum(n);
	qsUntitled += ".mwb";
    return qsUntitled;
}
#endif

/*========================================================
 * TASK:
 * PARAMS:
 * GLOBALS:
 * RETURNS:
 * REMARKS: - disables redraw
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

        restoreGeometry(s->value("geometry").toByteArray());
        restoreState(s->value("windowState").toByteArray());
        qs = s->value("version", "0").toString();       // (immediate toInt() looses digits)
        long ver = qs.toInt(0, 0);                                   // format Major.mInor.Sub

        if ((ver & 0xFFFFFF00) != (MAGIC_VERSION & 0xFFFFFF00))        // sub version number not used
        {
            QFile::remove(qsSettingsName);
            _AddNewTab();
            return;
        }
    }
    qs = s->value("mode", "s").toString();

    switch (qs[0].unicode())
    {
        case 'b': ui.actionBlackMode->setChecked(true);  on_actionBlackMode_triggered(); break;
        case 'd': ui.actionDarkMode->setChecked(true); on_actionDarkMode_triggered(); break;
        case 's': // default on form
        default: break;
    }
    int n = s->value("grid", 0).toInt();
    ui.actionFixedGrid->setChecked(n & 2);
    _pChkGridOn->setChecked(n & 1);
    _drawArea->SetGridOn(n & 1, n & 2);
    n = s->value("pageG", 0).toInt(0);
    ui.actionShowPageGuides->setChecked(n);
    _drawArea->SetPageGuidesOn(n);
    bool b = s->value("limited", true).toBool();
    ui.actionLimitedPage->setChecked(b);    // default: checked
#ifndef _VIEWER
    _drawArea->SetLimitedPage(b);
#endif

    MyPrinterData data;
    
    data.flags = s->value("pflags", 0).toInt();		        // bit 0: print background image, bit 1: white background
    data.SetMargins(s->value("pdfmlr", 1.0).toFloat(), s->value("pdfmtb", 1.0).toFloat(), s->value("pdfgut", 0.0).toFloat(), false);
    int resi = s->value("resi", 6).toInt(); // 1920 x 1080
    if (s->value("useri",true).toBool() )
        data.screenPageWidth = myScreenSizes[resi].w;
    else
        data.screenPageWidth = s->value("hpxs", 1920).toInt();

    data.SetDpi(resos[s->value("pdfdpi", 0).toInt()], false);      // recalculates screen area for page and also sets 'data.screenPageHeight'
    data.SetPrintArea(s->value("pdfpgs", 3).toInt(), true);       // using default DPI value

    _drawArea->SetMyPrinterData(data);

#ifndef _VIEWER
    qs = s->value("size", "3,3,3,3,3,30").toString();      // pen size for black, red, green, blue, yellow, eraser
    QStringList qsl = qs.split(',');
    if (qsl.size() != PEN_COUNT)
    {
        qsl.clear();
        qsl << "3" << "3" << "3" << "3" << "3" << "30";
    }
    for (int i = 0; i < PEN_COUNT; ++i)
        _penWidth[i] = qsl[i].toInt();
    _psbPenWidth->setValue(_penWidth[0]);
    
    ui.actionAutoSaveData->setChecked(s->value("saved", false).toBool());
    ui.actionAutoSaveBackgroundImage->setChecked(s->value("saveb", false).toBool());

    qs = s->value("img", QString()).toString();
    if (!qs.isEmpty())
        _drawArea->OpenBackgroundImage(qs);
    if ((_useScreenshotTransparency = s->value("transp", false).toBool()))
    {
        _screenshotTransparencyColor = s->value("transc", "#ffffff").toString();
        ui.actionScreenshotTransparency->setChecked(_useScreenshotTransparency);
    }
    
    _nGridSpacing = s->value("gridspacing", 64).toInt();
    if (_nGridSpacing < 5)
        _nGridSpacing = 64;
    _psbGridSpacing->setValue(_nGridSpacing);
    emit GridSpacingChanged(_nGridSpacing);
#endif

    _lastDir  = s->value("lastDir",  "").toString();
    if (!_lastDir.isEmpty() && _lastDir[_lastDir.size() - 1] != '/')
        _lastDir += "/";
    _lastPDFDir  = s->value("lastPDFDir",  "").toString();
    if (!_lastPDFDir.isEmpty() && _lastPDFDir[_lastPDFDir.size() - 1] != '/')
        _lastPDFDir += "/";

    // command line arguments    
    QStringList paramsList = QCoreApplication::arguments();
    for (auto& s : paramsList)
        s = QDir::cleanPath(s);     // need not exist

    int argc = 10 - paramsList.size() + 1;     // max 10 tabs can be opened, 0. params is command path
    // tabs

    s->beginGroup("tabs");
    int nFilesToRestore = s->value("tabSize", 0).toInt();
    if (nFilesToRestore)
    {
        bool b = false; // window title set?
        for(int n = 1; n <= nFilesToRestore && n < argc; ++n )
        {
            qs = QString("fn%1").arg(n);
            qs = s->value(qs, QString()).toString();
            if (paramsList.indexOf(qs) < 0 && QFile::exists(qs) )    // if any of the old files is in the argument list do not use it
            {
                _AddNewTab(qs, false);  // do not load data yet
                if (!qs.isEmpty() || !b)
                    b = true, setWindowTitle(sWindowTitle + QString(" - %1").arg(qs));
            }
        }
        _nLastTab = s->value("lastTab", 0).toInt();
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

    _sImageName = s->value("bckgrnd", "").toString();
#ifndef _VIEWER
    if (!_sImageName.isEmpty() && !_drawArea->OpenBackgroundImage(_sImageName))
        _sImageName.clear();
#endif

    // recent documents
    s->beginGroup("recent");
    int cnt = s->value("cnt", 0).toInt();
    
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

void FalconBoard::SaveState()
{
    QSettings *s = FBSettings::Open();

	s->setValue("geometry", saveGeometry());
	s->setValue("windowState", saveState());
    QString qsVersion("0x%1");
    qsVersion = qsVersion.arg(MAGIC_VERSION, 8, 16, QLatin1Char('0') );
	s->setValue("version", qsVersion);
    if (_actLanguage < 0)
        s->remove("lang");
    else
        s->setValue("lang", _actLanguage);

    s->setValue("mode", _screenMode == smSystem ? "s" : _screenMode == smDark ? "d" : "b");
    s->setValue("grid", (ui.actionShowGrid->isChecked() ? 1 : 0) + (ui.actionFixedGrid->isChecked() ? 2 : 0));
    s->setValue("pageG", ui.actionShowPageGuides->isChecked() ? 1 : 0);
    s->setValue("limited", ui.actionLimitedPage->isChecked());
#ifndef _VIEWER
    s->setValue("size", QString("%1,%2,%3,%4,%5").arg(_penWidth[0]).arg(_penWidth[1]).arg(_penWidth[2]).arg(_penWidth[3]).arg(_penWidth[4]));
    s->setValue("saved", ui.actionAutoSaveData->isChecked());
    s->setValue("saveb", ui.actionAutoSaveBackgroundImage->isChecked());
    if (ui.actionAutoSaveBackgroundImage->isChecked())
		s->setValue("img", _sImageName);
    if (_useScreenshotTransparency)
    {
        s->setValue("transp", _useScreenshotTransparency);
        s->setValue("transc", _screenshotTransparencyColor.name());
    }
    else
    {
        s->remove("transp");
        s->remove("transc");
    }
#endif
    s->remove("tabs");
    int nFilesToRestore = _pTabs->count();      // 1,2,...
    if(nFilesToRestore != 1 || !_drawArea->HistoryName().isEmpty()) 
    {
        s->beginGroup("tabs");

    	s->setValue("lastTab", _nLastTab);
        s->setValue("tabSize", nFilesToRestore);
        if (nFilesToRestore)
        {
            QString qs, qsn;
            for (int n = 1; n <= nFilesToRestore && n < 10; ++n)
            {
                qs = QString("fn%1").arg(n);
                qsn = _drawArea->HistoryName(n-1);
                s->setValue(qs, qsn);
            }
        }
        s->endGroup();
    }

	s->setValue("lastDir", _lastDir);
	s->setValue("lastPDFDir", _lastPDFDir);
	s->setValue("bckgrnd", _sImageName);
    s->setValue("gridspacing", _nGridSpacing);

    if (_recentList.size())
    {
        s->beginGroup("recent");
        s->setValue("cnt", _recentList.size());
        for (int i = 0; i < _recentList.size(); ++i)
            s->setValue(QString().setNum(i + 1), _recentList[i]);
        s->endGroup();
    }
    else
        s->remove("recent");
    s->setValue("histSize", _drawArea->HistoryListSize());
    FBSettings::Close();
}

void FalconBoard::_LoadIcons()
{
    _iconPen = QIcon(":/FalconBoard/Resources/white.png");
    _iconExit = QIcon(":/FalconBoard/Resources/close.png");
    _iconEraser = QIcon(":/FalconBoard/Resources/eraser.png");
    _iconNew = QIcon(":/FalconBoard/Resources/new.png");
    _iconOpen = QIcon(":/FalconBoard/Resources/open.png");
    _iconSave = QIcon(":/FalconBoard/Resources/save.png");
    _iconSaveAs = QIcon(":/FalconBoard/Resources/saveas.png");
    _iconUndo = QIcon(":/FalconBoard/Resources/undo.png");
    _iconRedo = QIcon(":/FalconBoard/Resources/redo.png");
    _iconScreenShot = QIcon(":/FalconBoard/Resources/screenshot.png");
}

void FalconBoard::_SetupIconsForPenColors(ScreenMode sm)
{
    drawColors.SetDarkMode(sm != smSystem);

#ifndef _VIEWER
    ui.action_Black->setIcon(sm == smSystem ? _ColoredIcon(_iconPen, drawColors[penBlack]) : _iconPen);
    ui.action_Black->setText(drawColors.ActionName(penBlack));

    ui.action_Red->setIcon(_ColoredIcon(_iconPen,   drawColors[penRed]));
    ui.action_Red->setText(drawColors.ActionName(penRed));

    ui.action_Green->setIcon(_ColoredIcon(_iconPen, drawColors[penGreen]));
    ui.action_Green->setText(drawColors.ActionName(penGreen));

    ui.action_Blue->setIcon(_ColoredIcon(_iconPen,  drawColors[penBlue]));
    ui.action_Blue->setText(drawColors.ActionName(penBlue));

    ui.action_Yellow->setIcon(_ColoredIcon(_iconPen,drawColors[penYellow]));
    ui.action_Yellow->setText(drawColors.ActionName(penYellow));
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

    ui.mainToolBar->addAction(ui.action_Black);
    ui.mainToolBar->addAction(ui.action_Red);
    ui.mainToolBar->addAction(ui.action_Green);
    ui.mainToolBar->addAction(ui.action_Blue);
    ui.mainToolBar->addAction(ui.action_Yellow);
    ui.mainToolBar->addAction(ui.action_Eraser);

    _penGroup = new QActionGroup(this);
    _penGroup->addAction(ui.action_Black);
    _penGroup->addAction(ui.action_Red);
    _penGroup->addAction(ui.action_Green);
    _penGroup->addAction(ui.action_Blue);
    _penGroup->addAction(ui.action_Yellow);
    _penGroup->addAction(ui.action_Eraser);
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
    _psbPenWidth->setValue(_penWidth[0]);
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
    _psbGridSpacing->setMaximum(128);
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

int FalconBoard::_AddNewTab(QString fname, bool loadIt, bool force) // and new history record
{
    if (_pTabs->count() == MAX_NUMBER_OF_TABS)
        return -1;

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
#ifndef _VIEWER
    if (fname.isEmpty())
        fname = _NextUntitledName();
#endif
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

    _AddToRecentList(_drawArea->HistoryName());
    int cnt = _drawArea->RemoveHistory(index);
#ifndef _VIEWER
    _drawArea->HideRubberBand(true);
#endif
    _pTabs->removeTab(index);
    if (!cnt)
        _AddNewTab(QString(), false);

    _drawArea->SwitchToHistory(_nLastTab, !cnt);
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


/*========================================================
 * TASK:    if the actual or any history changed and 
 *          it/they need to be saved then asks for 
 *          a save confirmation then saves it/them
 * PARAMS:  index: save this history
 *          mustAsk: do not autosave it, always ask
 *          any:    true: should check all histories
 *                  false: only index-th history
 * GLOBALS:
 * RETURNS: srNoSave : no save was required or was saved
 *          srCanceed: save is cancelled
 *          srFailed : save error
 *          srSaveSuccess: saved
 * REMARKS: - 'any' must be set to true when the program
 *              is about to close
 *          - not only the active history can be saved
 *-------------------------------------------------------*/
SaveResult FalconBoard::_SaveIfYouWant(int index, bool mustAsk)
{
    _saveResult = srSaveSuccess;    // result: ok

    _drawArea->SwitchToHistory(index, false);    // do not redraw

    const QString & saveName = _drawArea->HistoryName();
    QMessageBox::StandardButton ret = QMessageBox::Save;

    bool isAutoSaveSet = ui.actionAutoSaveData->isChecked();
    bool isUntitled = saveName.isEmpty() || saveName.left(UNTITLED.length()) == UNTITLED;

    if (!isAutoSaveSet || mustAsk || isUntitled)
    {
        ret = QMessageBox::warning(this, tr(WindowTitle),
            QString(tr("<i>%1</i> have been modified.\n"
                "Do you want to save your changes?")).arg(saveName.isEmpty() ? _NextUntitledName() : saveName),
            QMessageBox::Save | QMessageBox::No
            | QMessageBox::Cancel);

        if (ret == QMessageBox::Cancel)
            return srCancelled;
        else if (ret == QMessageBox::No)
            return srNoSave;
    }

    // (ret == QMessageBox::Save)
    if (isUntitled)
    {
        on_actionSaveAs_triggered();     // sets _saveResult
        if (_saveResult == srSaveSuccess)
            _drawArea->SetHistoryName(_lastSaveName);
    }
    else 
        _SaveFile(saveName);             // sets _saveResult

    _drawArea->SwitchToHistory(_nLastTab, true);
    return _saveResult;
}

SaveResult FalconBoard::_SaveFile(const QString name)   // returns: 0: cancelled, -1 error, 1 saved
{
    _saveResult = _drawArea->Save(name);
    if (_saveResult == srSaveSuccess)
    {
        _AddToRecentList(name);
        _lastSaveName = name;
    }
    return _saveResult;
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
        case penBlack: ui.action_Black->setChecked(true); break;
        case penRed: ui.action_Red->setChecked(true); break;
        case penGreen: ui.action_Green->setChecked(true); break;
        case penBlue: ui.action_Blue->setChecked(true); break;
        case penYellow: ui.action_Yellow->setChecked(true); break;
        case penEraser:on_action_Eraser_triggered(); return;
    }
    _SetPenKind();
}

void FalconBoard::_SetPenKind()
{
    _eraserOn = _actPen == penEraser;
    if(_actPen != penNone)
	{
		_drawArea->SetPenKind(_actPen, _penWidth[_actPen - 1]);
		++_busy;
		_psbPenWidth->setValue(_penWidth[_actPen - 1]);
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

void FalconBoard::_SetBlackPen() { _SetPenKind(penBlack); }
void FalconBoard::_SetRedPen()   { _SetPenKind  (penRed); }
void FalconBoard::_SetGreenPen() { _SetPenKind(penGreen); }
void FalconBoard::_SetBluePen()  { _SetPenKind (penBlue); }
void FalconBoard::_SetYellowPen()  { _SetPenKind (penYellow); }

void FalconBoard::_SetPenWidth(FalconPenKind pk)
{
    _drawArea->SetPenKind(pk, _penWidth[pk-1]);
    ++_busy;
    _psbPenWidth->setValue(_penWidth[pk-1]);
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
    QBitmap mask = pm.createMaskFromColor(Qt::white, Qt::MaskOutColor);

    pmW.fill(colorW);
    pmW.setMask(mask);
    if (colorB.isValid())
    {
        mask = pm.createMaskFromColor(Qt::black, Qt::MaskOutColor);
        pmB.fill(colorB);
        pmB.setMask(mask);
        QPainter painter(&pmW);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawPixmap(0,0, pm.width(), pm.height(), pmB);
    }
    return QIcon(pmW);
}


#ifndef _VIEWER
void FalconBoard::_ConnectDisconnectScreenshotLabel(bool join )
{

    if (join)
    {
        connect(_plblScreen, &Snipper::SnipperCancelled, this, &FalconBoard::SlotForScreenShotCancelled);
        connect(_plblScreen, &Snipper::SnipperReady, this, &FalconBoard::SlotForScreenshotReady);
    }
    else
    {
        disconnect(_plblScreen, &Snipper::SnipperCancelled, this, &FalconBoard::SlotForScreenShotCancelled);
        disconnect(_plblScreen, &Snipper::SnipperReady, this, &FalconBoard::SlotForScreenshotReady);
    }
}
#endif
QString FalconBoard::_FileNameToTabText(QString fname)
{
    int i = fname.lastIndexOf('/'),  // get file name, -1 is Ok
        j = fname.lastIndexOf(".mwb");
    if (j < 0)
        j = fname.length();
    return fname.mid(i + 1, j - i - 1);
}
void FalconBoard::_SetResetChangedMark(int index)
{   
    if(index < 0)
        index = _pTabs->currentIndex();
    QString text = _pTabs->tabText(index);
    int n = text.length() - 1;

    if (text[n] == CHANGE_MARKER_CHAR)
        text.remove(--n, 2);

    if (_drawArea->IsModified(index))
        text = text + QChar(' ') + CHANGE_MARKER_CHAR;
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
    if (_drawArea->IsModified(index))
        text = text + QChar(' ') + CHANGE_MARKER_CHAR;
    _pTabs->setTabText(index, text);
}

void FalconBoard::_SetupMode(ScreenMode mode)
{
    QString ss,
        sPng = ".png";

    _screenMode = mode;

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

    _SetupIconsForPenColors(mode);      // pen selection

    switch (mode)
    {
        default:
        case smSystem:
            // already set : _drawArea->drawColors.SetDarkMode(false);   // light mode: dark colors
            // already set : ui.action_Black->setIcon(_ColoredIcon(_iconPen, _drawArea->drawColors[penBlack]));
            ui.actionExit   ->setIcon(_iconExit   );
            ui.action_Eraser->setIcon(_iconEraser );
            ui.actionNew    ->setIcon(_iconNew    );
            ui.actionLoad   ->setIcon(_iconOpen   );
            ui.actionRedo   ->setIcon(_iconRedo   );
            ui.actionUndo   ->setIcon(_iconUndo   );
            ui.actionSave   ->setIcon(_iconSave   );
            ui.action_Screenshot->setIcon(_iconScreenShot);
            _sBackgroundColor = "#FFFFFF";
            _sBackgroundHighLigtColor = "#D8EAF9";
            _sSelectedBackgroundColor = "#007acc",
            _sUnselectedBackgroundColor = "#d0d0d0",
            _sTextColor = "#000000";
            _sDisabledColor = "#AAAAAA";
            _sGridColor = "#d0d0d0";
            _sPageGuideColor = "#fcd475";
            _sToolBarColor = "#AAAAAA";
            break;
        case smDark:
            // already set : _drawArea->drawColors.SetDarkMode(true);
            // already set : ui.action_Black->setIcon(_ColoredIcon(_iconPen, Qt::black)); // white
            ui.actionExit->setIcon(_ColoredIcon(_iconExit, Qt::black, QColor(Qt::white)));
            ui.action_Eraser->setIcon(_ColoredIcon(_iconEraser, Qt::black, QColor(Qt::white)));
            ui.actionNew->setIcon(_ColoredIcon(_iconNew  , Qt::black, QColor(Qt::white)));
            ui.actionLoad->setIcon(_ColoredIcon(_iconOpen, Qt::black, QColor(Qt::white)));
            ui.actionRedo->setIcon(_ColoredIcon(_iconRedo, Qt::black, QColor(Qt::white)));
            ui.actionUndo->setIcon(_ColoredIcon(_iconUndo, Qt::black, QColor(Qt::white)));
            ui.actionSave->setIcon(_ColoredIcon(_iconSave, Qt::black, QColor(Qt::white)));
            ui.action_Screenshot->setIcon(_ColoredIcon(_iconScreenShot, Qt::black, QColor(Qt::white)));
            _sBackgroundColor = "#282828";
            _sSelectedBackgroundColor = "#007acc";
            _sBackgroundHighLigtColor = "#D8EAF9";
            _sUnselectedBackgroundColor = "#202020";
            _sTextColor = "#E1E1E1";
            _sDisabledColor = "#AAAAAA";
            _sGridColor = "#202020";
            _sPageGuideColor = "#413006";
            _sToolBarColor = "#202020";
            break;
        case smBlack:
            // already set : _drawArea->drawColors.SetDarkMode(true);
            // already set : ui.action_Black->setIcon(_ColoredIcon(_iconPen, Qt::black));     // white

            ui.actionExit->setIcon(_ColoredIcon(_iconExit, Qt::black, QColor(Qt::white)));
            ui.action_Eraser->setIcon(_ColoredIcon(_iconEraser, Qt::black, QColor(Qt::white)));
            ui.actionNew->setIcon(_ColoredIcon(_iconNew, Qt::black, QColor(Qt::white)));
            ui.actionLoad->setIcon(_ColoredIcon(_iconOpen, Qt::black, QColor(Qt::white)));
            ui.actionRedo->setIcon(_ColoredIcon(_iconRedo, Qt::black, QColor(Qt::white)));
            ui.actionUndo->setIcon(_ColoredIcon(_iconUndo, Qt::black, QColor(Qt::white)));
            ui.actionSave->setIcon(_ColoredIcon(_iconSave, Qt::black, QColor(Qt::white)));
            ui.action_Screenshot->setIcon(_ColoredIcon(_iconScreenShot, Qt::black, QColor(Qt::white)));
            _sBackgroundColor = "#000000";
            _sBackgroundHighLigtColor = "#D8EAF9";
            _sSelectedBackgroundColor = "#007acc";
            _sUnselectedBackgroundColor = "#101010";
            _sTextColor = "#CCCCCC";
            _sDisabledColor = "#888888";
            _sGridColor = "#202020";
            _sPageGuideColor = "#2e2204";
            _sToolBarColor = "#101010";
            break;
    }
    if(_eraserOn)
        _drawArea->SetCursor(csEraser);

    if (mode != smSystem)
        ss = "* {\n" 
                  "  background-color:" + _sBackgroundColor + ";\n"
                  "  color:" + _sTextColor + ";\n"
             "}\n"
             "QMenuBar::item, "
             "QMenu::separator, "
             "QMenu::item {\n"
             "  color:" + _sTextColor + ";\n"
             "}\n"
             "QMenuBar::item:selected,\n"
             "QMenu::item:selected {\n"
             "  color:"+_sBackgroundColor + ";\n"
             "  background-color:" + _sTextColor + ";\n"
             "}\n"
             "QMenu::item:disabled {\n"
             "  color:"+ _sDisabledColor + ";\n"
             "}\n"
             "QStatusBar, QToolBar {\n"
             " background-color:" + _sToolBarColor + ";\n"
             "}\n"
             "QTabBar::tab {\n"
             " color:" + _sTextColor + ";\n"
             " background-color:"+ _sUnselectedBackgroundColor+";\n"
             " selection-background-color:"+ _sSelectedBackgroundColor+";\n"
             "}\n"
             "QTabBar::tab:selected {\n background-color:" + _sSelectedBackgroundColor + ";\n"
             "}\n"
             "QToolTip {\n background-color:"+_sTextColor+";\n color:"+_sBackgroundColor+";\n"
             "}\n"
        ;

    ((QApplication*)(QApplication::instance()))->setStyleSheet(ss); // so it cascades down to all sub windows/dialogs, etc

    _drawArea->SetMode(mode != smSystem, _sBackgroundColor, _sGridColor, _sPageGuideColor);
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
    setWindowTitle(qs);
}

void FalconBoard::closeEvent(QCloseEvent* event)
{
#ifndef _VIEWER
    if (ui.actionAutoSaveBackgroundImage->isChecked())
        _SaveBackgroundImage();

    _saveCount = 0;
    // save all changed
    int n = 0;
    SaveResult sr = srSaveSuccess;
    while (sr != srCancelled && (n = _drawArea->IsModified(n, true)))    // returned : n = (index of first modified at or after 'n') + 1
        if ((sr = _SaveIfYouWant(n - 1, !ui.actionAutoSaveData->isChecked())) == srSaveSuccess)
            ++_saveCount;

    _saveResult = sr != srCancelled ? srSaveSuccess :srCancelled;

    if ((_saveResult == srSaveSuccess)) 
    {
        SaveState();
        event->accept();
    }
    else
		event->ignore();
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

    bool b = QMessageBox::question(this, "falconBoard", tr("Do you want to limit the editable area horizontally to the screen width?\n"
                                                           " You may change this any time in Options/Page"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
    ui.actionLimitedPage->setChecked(b);
    _drawArea->SetLimitedPage(b);

    _AddNewTab(QString(), false, true);

    //_drawArea->NewData();
    setWindowTitle(sWindowTitle);
    _backgroundImageName.clear();
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
        _AddToRecentList(_drawArea->HistoryName(index));
    }
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
                    _drawArea->SetHistoryName(fileName);

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
        on_action_Eraser_triggered();
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
    int n = _pTabs->currentIndex();
    if (IsOverwritable())
    {
        _drawArea->SetHistoryName(fileName);
        _SetTabText(-1, fileName);
        _drawArea->Load();
    }
    else
        n = _AddNewTab(fileName, true);   // sets current tab and adds history item and set it to current too
                            
    _SetWindowTitle(fileName);

#ifndef _VIEWER
    if (_eraserOn)
        on_action_Eraser_triggered();
#endif
}

void FalconBoard::on_action_Close_triggered()
{
    SlotForTabCloseRequested(_drawArea->ActHistoryIndex());
}

void FalconBoard::on_action_CloseAll_triggered()
{
    while (_drawArea->AnyHistoryToSave()>=0)
        on_action_Close_triggered();
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
        _drawArea->SetHistoryName(fileName);
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
        QMessageBox::warning(this, FB_WARNING, QString(tr("Please restart the program to change the language!")).arg(which));
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
    QString fname = _drawArea->HistoryName();
    if (fname.isEmpty())
        on_actionSaveAs_triggered();
    else
    {
        _SaveFile(fname);
        _SetTabText(-1, fname);
    }
}
void FalconBoard::on_actionSaveAs_triggered() // current tab
{
    QString fname = _pTabs->tabText(_pTabs->currentIndex());//    _drawArea->HistoryName(UNTITLED);
    int n = fname.length() - 1;
    if (fname.at(1) == CHANGE_MARKER_CHAR)
        fname.remove(--n, 2);

    //if (fname.isEmpty())
    //    fname = _NextUntitledName();
    QString initialPath = _lastDir + fname; 
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
                        initialPath, tr("FalconBoard Files (*.mwb);; All Files (*))"));
    if (fileName.isEmpty())
    {
        _saveResult = srCancelled;
        return;
    }
    _SaveLastDirectory(fileName);
    _SaveFile(fileName);    // and sets _SaveResult
    setWindowTitle(sWindowTitle + QString(" - %1").arg(fileName));
    _SetTabText(-1, fileName);
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
        QString(tr("Open source Whiteboard/blackboard application")+
            tr("<p>Version ")+ sVersion+ "</p>")+
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

void FalconBoard::on_actionLightMode_triggered()
{
    _SetupMode(smSystem);
}

void FalconBoard::on_actionDarkMode_triggered()
{
    _SetupMode(smDark);
}

void FalconBoard::on_actionBlackMode_triggered()
{
    _SetupMode(smBlack);
}

void FalconBoard::SlotForTabChanged(int index) // index <0 =>invalidate tab
{
    // this function is called only when the _pTabs->currentTab() changes
    if (!_pTabs->count())
        return;

    if (_dontCareForTabChange) // so it won't reflect in history change or loading data
    {
        _drawArea->SwitchToHistory(index, true, true); // invalidate 
        _dontCareForTabChange = false;      // care next time
    }
    else
    {
        _drawArea->SwitchToHistory(index, true);
        _SetWindowTitle(_drawArea->HistoryName());
    }
    _nLastTab = index;
}

void FalconBoard::SlotForTabCloseRequested(int index)
{
#ifndef _VIEWER
    _drawArea->HideRubberBand(true);
    if (!_drawArea->IsModified(index) || _SaveIfYouWant(index, true)!= srCancelled)
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
    _drawArea->MoveHistory(from, to); 
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
    _drawArea->SetGridOn(ui.actionShowGrid->isChecked(), ui.actionFixedGrid->isChecked());
}

void FalconBoard::on_actionGridSize_triggered()
{
    bool ok;
    int n = QInputDialog::getInt(this, tr("falconBoard - Grid spacing"),
                                         tr("Spacing in pixels:"), _nGridSpacing,
                                         5, 128, 10, &ok);
    if (ok && n != _nGridSpacing)
    {
        _nGridSpacing = n;
        emit GridSpacingChanged(n);
    }
            
}

void FalconBoard::slotGridSpacingChanged(int val)
{
    if (_busy)		// from program
        return;
    // from user
    _nGridSpacing = val;
    emit GridSpacingChanged(val);
}

void FalconBoard::slotGridSpacingEditingFinished()
{
    ui.centralWidget->setFocus();
}


void FalconBoard::on_actionShowPageGuides_triggered()
{
    _drawArea->SetPageGuidesOn(ui.actionShowPageGuides->isChecked());
}

#ifndef _VIEWER
void FalconBoard::on_action_Black_triggered()
{
    _drawArea->RecolorSelected(Qt::Key_1);
    _SetBlackPen();
    _SetCursor(csPen);
    _SetPenWidth(penBlack);
    ui.centralWidget->setFocus();
};
void FalconBoard::on_action_Red_triggered() 
{   
    _drawArea->RecolorSelected(Qt::Key_2);
    _SetRedPen();
    _SetCursor(csPen);
    _SetPenWidth(penRed);
    ui.centralWidget->setFocus();
};

void FalconBoard::on_action_Green_triggered()
{
	_drawArea->RecolorSelected(Qt::Key_3);
	_SetGreenPen();
	_SetCursor(csPen);
	_SetPenWidth(penGreen);
	ui.centralWidget->setFocus();
};
void FalconBoard::on_action_Blue_triggered()
{
	_drawArea->RecolorSelected(Qt::Key_4);
	_SetBluePen();
	_SetCursor(csPen);
	_SetPenWidth(penBlue);
	ui.centralWidget->setFocus();
};
void FalconBoard::on_action_Yellow_triggered()
{
	_drawArea->RecolorSelected(Qt::Key_5);
	_SetYellowPen();
	_SetCursor(csPen);
	_SetPenWidth(penYellow);
	ui.centralWidget->setFocus();
};

void FalconBoard::on_action_Eraser_triggered()
{
    _eraserOn = true;
    _SetPenWidth(_actPen = penEraser);
    _drawArea->SetCursor(csEraser);
    _SelectPenForAction(ui.action_Eraser);
	ui.centralWidget->setFocus();
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

void FalconBoard::on_action_Screenshot_triggered()
{
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    _plblScreen = new Snipper(nullptr);
    _plblScreen->setGeometry(screen->geometry());
    _ConnectDisconnectScreenshotLabel(true);

    hide();

    QThread::msleep(300);

    _plblScreen->setPixmap(screen->grabWindow(0));

    show();
    _plblScreen->show();
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

void FalconBoard::on_actionInfinitePage_triggered()
{
    if (_busy)
        return;
    ++_busy;
	bool b = ui.actionInfinitePage->isChecked();
	ui.actionLimitedPage->setChecked(!b);
	_drawArea->SetLimitedPage(!b);
    
    --_busy;
}

void FalconBoard::on_actionLimitedPage_triggered()
{
    if (_busy)
        return;
    ++_busy;
    bool b = ui.actionLimitedPage->isChecked();
    ui.actionInfinitePage->setChecked(!b);
    _drawArea->SetLimitedPage(b);

    --_busy;
}

void FalconBoard::on_actionUndo_triggered()
{
    _drawArea->Undo();                
    _SelectPen();
    if (_eraserOn)
        on_action_Eraser_triggered();
    else
        _SetCursor(csPen);

}

void FalconBoard::on_actionRedo_triggered()
{
    _drawArea->Redo();
    _SelectPen();
    if (_eraserOn)
        on_action_Eraser_triggered();
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
    _penWidth[_actPen-1] = val;
    _SetPenKind();
}

void FalconBoard::slotPenWidthEditingFinished()
{
    ui.centralWidget->setFocus();
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
    ui.centralWidget->setFocus();
}

void FalconBoard::SlotForPointerType(QTabletEvent::PointerType pt)   // only sent by tablet
{
    static bool isPenEraser = false;     // set to true if not erasemode just
    static FalconPenKind pk = penBlack;
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
    _plblScreen->hide();

    QPixmap pixmap;  // need a pixmap for transparency (used Qpixmap previously)
    pixmap = QPixmap(gmetry.size()); //  , Qpixmap::Format_ARGB32);

    QPainter *painter = new QPainter(&pixmap);   // need to delete it before the label is deleted
    painter->drawPixmap(QPoint(0,0), *_plblScreen->pixmap(), gmetry);
    delete painter;

    if (_useScreenshotTransparency)
    {
        QBitmap bm = MyCreateMaskFromColor(pixmap, _screenshotTransparencyColor, _transparencyFuzzyness);
        pixmap.setMask(bm);
    }

    _drawArea->AddScreenShotImage(pixmap);

    _ConnectDisconnectScreenshotLabel(false);
    delete _plblScreen;
    _plblScreen = nullptr;

}

void FalconBoard::SlotForScreenShotCancelled()
{
    _plblScreen->hide();
    _ConnectDisconnectScreenshotLabel(false);
    delete _plblScreen;
    _plblScreen = nullptr;
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
#endif

void FalconBoard::on_actionPageSetup_triggered()
{
    _drawArea->PageSetup(PageSetupDialog::wtdPageSetup);
}

void FalconBoard::on_actionExportToPdf_triggered()
{
    QString saveName = _drawArea->HistoryName();
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


