﻿#include <QSpinBox>
#include <QLabel>
#include <QScreen>
#include <QPainter>
#include <QThread>
#include <QSettings>
#include "DrawArea.h"
#include "screenshotTransparency.h"
#include "FalconBoard.h"
#include "myprinter.h"   // for MyPrinterData

#ifdef _VIEWER
void FalconBoard::_RemoveMenus()
{
    QList<QAction*> pMenuActions = ui.menuBar->actions();
    ui.menuBar->removeAction(pMenuActions[1] ); // edit
    ui.menuBar->removeAction(pMenuActions[2]);  // clear
    ui.actionSave->setVisible(false);
    ui.actionSaveAs->setVisible(false);
    ui.actionSaveVisible->setVisible(false);
    ui.actionLoadBackground->setVisible(false);
    ui.actionScreenshotTransparency->setVisible(false);

    ui.actionAutoSaveData->setVisible(false);
    ui.actionAutoSaveBackgroundImage->setVisible(false);
    ui.menuGrid->removeAction(ui.menuGrid->actions()[2]);
    pMenuActions[3]->menu()->removeAction(pMenuActions[3]->menu()->actions()[1]);

    removeToolBar(ui.mainToolBar);
}
#endif

// ************************ helper **********************
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
	ui.setupUi(this);

    _homePath = QDir::homePath() +
#if defined (Q_OS_Linux)   || defined (Q_OS_Darwin) || defined(__linux__)
        "/.falconBoard";
#elif defined(Q_OS_WIN)
    "/Appdata/Local/FalconBoard";
#endif
    if (!QDir(_homePath).exists())
        QDir(_homePath).mkdir(_homePath);

    _drawArea = static_cast<DrawArea*>(ui.centralWidget);
    _drawArea->SetPenKind(_actPen, _penWidth[_actPen-1]); 

    _CreateAndAddActions();
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

    RestoreState();     // sets up all history item names, and loads the last used one

    _SetupIconsForPenColors(_screenMode);

    ui.centralWidget->setFocus();
}


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

    QString qsSettingsName = _homePath +
#ifdef _VIEWER
        "/FalconBoardViewer.ini";
#else
        "/FalconBoard.ini";
#endif
    bool bIniExisted = QFile::exists(qsSettingsName);
    QString qs;
    QSettings s(qsSettingsName, QSettings::IniFormat);
    if (bIniExisted)
    {

        restoreGeometry(s.value("geometry").toByteArray());
        restoreState(s.value("windowState").toByteArray());
        qs = s.value("version", "0").toString();       // (immediate toInt() looses digits)
        long ver = qs.toInt(0, 0);                                   // format Major.mInor.Sub

        if ((ver & 0xFFFFFF00) != (nVersion & 0xFFFFFF00))        // sub version number not used
        {
            QFile::remove(qsSettingsName);
            _AddNewTab();
            return;
        }
    }
    qs = s.value("mode", "s").toString();

    switch (qs[0].unicode())
    {
        case 'b': ui.actionBlackMode->setChecked(true);  on_actionBlackMode_triggered(); break;
        case 'd': ui.actionDarkMode->setChecked(true); on_actionDarkMode_triggered(); break;
        case 's': // default on form
        default: break;
    }
    int n = s.value("grid", 0).toInt();
    ui.actionShowGrid->setChecked(n & 1);
    ui.actionFixedGrid->setChecked(n & 2);
    _drawArea->SetGridOn(n & 1, n & 2);
    n = s.value("pageG", 0).toInt(0);
    ui.actionShowPageGuides->setChecked(n);
    _drawArea->SetPageGuidesOn(n);
    bool b = s.value("limited", true).toBool();
    ui.actionLimitedPage->setChecked(b);    // default: checked
#ifndef _VIEWER
    _drawArea->SetLimitedPage(b);
#endif

    MyPrinterData data;
    
    data.screenPageWidth = s.value("hpxs", 1920).toInt();
    data.flags = s.value("pflags", 0).toInt();		        // bit): print background image, bit 1: white background
    _drawArea->SetPrinterData(data);

#ifndef _VIEWER
    qs = s.value("size", "3,3,3,3,30,3").toString();      // black, red, green, blue, eraser yellow
    QStringList qsl = qs.split(',');
    if (qsl.size() != NUM_COLORS)
    {
        qsl.clear();
        qsl << "3" << "3" << "3" << "3" << "30" << "3";
    }
    for (int i = 0; i < NUM_COLORS; ++i)
        _penWidth[i] = qsl[i].toInt();
    _psbPenWidth->setValue(_penWidth[0]);
    
    ui.actionAutoSaveData->setChecked(s.value("saved", false).toBool());
    ui.actionAutoSaveBackgroundImage->setChecked(s.value("saveb", false).toBool());

    qs = s.value("img", QString()).toString();
    if (!qs.isEmpty())
        _drawArea->OpenBackgroundImage(qs);
    if ((_useScreenshotTransparency = s.value("transp", false).toBool()))
    {
        _screenshotTransparencyColor = s.value("transc", "#ffffff").toString();
        ui.actionScreenshotTransparency->setChecked(_useScreenshotTransparency);
    }
    
    _nGridSpacing = s.value("gridspacing", 64).toInt();
    if (_nGridSpacing < 5)
        _nGridSpacing = 64;
    _psbGridSpacing->setValue(_nGridSpacing);
    emit GridSpacingChanged(_nGridSpacing);
#endif

    _lastDir  = s.value("lastDir",  "").toString();
    if (!_lastDir.isEmpty() && _lastDir[_lastDir.size() - 1] != '/')
        _lastDir += "/";
    _lastPDFDir  = s.value("lastPDFDir",  "").toString();
    if (!_lastPDFDir.isEmpty() && _lastPDFDir[_lastPDFDir.size() - 1] != '/')
        _lastPDFDir += "/";
            // tabs

    s.beginGroup("tabs");
    int nFilesToRestore = s.value("tabSize", 0).toInt();
    if (nFilesToRestore)
    {
        bool b = false; // window title set?
        for(int n = 1; n <= nFilesToRestore && n < 10;++n)
        {
            qs = QString("fn%1").arg(n);
            qs = s.value(qs, QString()).toString();
            _AddNewTab(qs, false);  // do not load data yet
            if (!qs.isEmpty() || !b)
               b = true, setWindowTitle(sWindowTitle + QString(" - %1").arg(qs));
            
        }
        _nLastTab = s.value("lastTab", 0).toInt();
    }
    else        // nothing to restore: create first tab
        _AddNewTab();    // + new empty history

    if (_nLastTab > _pTabs->count())
        _nLastTab = 0;

    s.endGroup();       // tabs

    _sImageName = s.value("bckgrnd", "").toString();
#ifndef _VIEWER
    if (!_sImageName.isEmpty() && !_drawArea->OpenBackgroundImage(_sImageName))
        _sImageName.clear();
#endif

    // recent documents
    s.beginGroup("recent");
    int cnt = s.value("cnt", 0).toInt();
    
    for (int i = 0; i < cnt; ++i)
    {
        qs = s.value(QString().setNum(i + 1), "").toString();
        if (!qs.isEmpty())
        {
            _recentList.push_back(qs);
            if (_recentList.size() == 9)
                break;
        }
    }
    _PopulateRecentMenu();

    s.endGroup();
}

void FalconBoard::SaveState()
{

	QSettings s(_homePath + 
#ifdef _VIEWER
        "/FalconBoardViewer.ini",
#else
        "/FalconBoard.ini",
#endif
        QSettings::IniFormat);

	s.setValue("geometry", saveGeometry());
	s.setValue("windowState", saveState());
    QString qsVersion("0x%1");
    qsVersion = qsVersion.arg(nVersion, 8, 16, QLatin1Char('0') );
	s.setValue("version", qsVersion);
    if (_actLanguage < 0)
        s.remove("lang");
    else
        s.setValue("lang", _actLanguage);

    s.setValue("mode", _screenMode == smSystem ? "s" : _screenMode == smDark ? "d" : "b");
    s.setValue("grid", (ui.actionShowGrid->isChecked() ? 1 : 0) + (ui.actionFixedGrid->isChecked() ? 2 : 0));
    s.setValue("pageG", ui.actionShowPageGuides->isChecked() ? 1 : 0);
    s.setValue("limited", ui.actionLimitedPage->isChecked());
#ifndef _VIEWER
    s.setValue("size", QString("%1,%2,%3,%4,%5").arg(_penWidth[0]).arg(_penWidth[1]).arg(_penWidth[2]).arg(_penWidth[3]).arg(_penWidth[4]));
    s.setValue("saved", ui.actionAutoSaveData->isChecked());
    s.setValue("saveb", ui.actionAutoSaveBackgroundImage->isChecked());
    if (ui.actionAutoSaveBackgroundImage->isChecked())
		s.setValue("img", _sImageName);
    if (_useScreenshotTransparency)
    {
        s.setValue("transp", _useScreenshotTransparency);
        s.setValue("transc", _screenshotTransparencyColor.name());
    }
    else
    {
        s.remove("transp");
        s.remove("transc");
    }
#endif
    s.remove("tabs");
    s.beginGroup("tabs");
    int nFilesToRestore = _pTabs->count();
    if (nFilesToRestore == 0 || _drawArea->HistoryName().isEmpty())
    {
        s.remove("tabSize");
        s.remove("lastTab");
    }
    else
    {
    	s.setValue("lastTab", _nLastTab);
        s.setValue("tabSize", nFilesToRestore);
        if (nFilesToRestore)
        {
            QString qs, qsn;
            for (int n = 1; n <= nFilesToRestore && n < 10; ++n)
            {
                qs = QString("fn%1").arg(n);
                qsn = _drawArea->HistoryName(n-1);
                s.setValue(qs, qsn);
            }
        }
    }
    s.endGroup();

	s.setValue("lastDir", _lastDir);
	s.setValue("lastPDFDir", _lastPDFDir);
	s.setValue("bckgrnd", _sImageName);
    s.setValue("gridspacing", _nGridSpacing);

    if (_recentList.size())
    {
        s.beginGroup("recent");
        s.setValue("cnt", _recentList.size());
        for (int i = 0; i < _recentList.size(); ++i)
            s.setValue(QString().setNum(i + 1), _recentList[i]);
        s.endGroup();
    }
    else
        s.remove("recent");
    s.setValue("histSize", _drawArea->HistoryListSize());

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

    ui.mainToolBar->addWidget(new QLabel(tr("Grid size:")));
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

int FalconBoard::_AddNewTab(QString fname, bool loadIt) // and new history record
{
    if (_pTabs->count() == MAX_NUMBER_OF_TABS)
        return -1;

    int n = _pTabs->addTab(_FileNameToTabText( QString(fname.isEmpty() ? UNTITLED : fname)) );
    if (n > 0)  // else tab switch is called 
        _pTabs->setCurrentIndex(n);

    _drawArea->AddHistory(fname, loadIt);
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
    _pTabs->removeTab(index);
    if (!cnt)
        _AddNewTab();
    _drawArea->SwitchToHistory(_nLastTab, false);
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
 * RETURNS: 1 : no save was required or was saved
 *          -1: save error
*           0 : save is cancelled
 * REMARKS: - 'any' must be set to true when the program
 *              is about to close
 *          - not only the active history can be saved
 *-------------------------------------------------------*/
SaveResult FalconBoard::_SaveIfYouWant(int index, bool mustAsk)
{
//*    int ci = _nLastTab; // actual tab
    _saveResult = srSaveSuccess;    // result: ok
//*    int n;

    _drawArea->SwitchToHistory(index, false);    // do not redraw

    const QString & saveName = _drawArea->HistoryName();
    QMessageBox::StandardButton ret = QMessageBox::Save;
    if (!ui.actionAutoSaveData->isChecked() || mustAsk || saveName.isEmpty())
    {
        ret = QMessageBox::warning(this, tr(WindowTitle),
            QString(tr("<i>%1</i> have been modified.\n"
                "Do you want to save your changes?")).arg(saveName.isEmpty() ? UNTITLED : saveName),
            QMessageBox::Save | QMessageBox::No
            | QMessageBox::Cancel);
    }

    if (ret == QMessageBox::Save)
    {
        if (saveName.isEmpty())
            on_actionSaveAs_triggered();     // sets _saveResult
        else 
            _SaveFile(saveName);             // sets _saveResult
    }
    else if (ret == QMessageBox::Cancel)
        return srCancelled;

    _drawArea->SwitchToHistory(_nLastTab, false);
    return _saveResult;
}

SaveResult FalconBoard::_SaveFile(const QString name)   // returns: 0: cancelled, -1 error, 1 saved
{
    _saveResult = _drawArea->Save(name);
    if (_saveResult == srSaveSuccess)
        _AddToRecentList(name);
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


void FalconBoard::_SetCursor(DrawArea::CursorShape cs)
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
    
    ScreenShotTransparencyDialog *dlg = new ScreenShotTransparencyDialog(this, _screenshotTransparencyColor, _useScreenshotTransparency);
    if (dlg->exec())
        dlg->GetResult(_screenshotTransparencyColor, _useScreenshotTransparency);
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
        connect(plblScreen, &Snipper::SnipperCancelled, this, &FalconBoard::SlotForScreenShotCancelled);
        connect(plblScreen, &Snipper::SnipperReady, this, &FalconBoard::SlotForScreenshotReady);
    }
    else
    {
        disconnect(plblScreen, &Snipper::SnipperCancelled, this, &FalconBoard::SlotForScreenShotCancelled);
        disconnect(plblScreen, &Snipper::SnipperReady, this, &FalconBoard::SlotForScreenshotReady);
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
void FalconBoard::_SetTabText(int index, QString fname)
{
    if (index >= _pTabs->count())
        return;
    if (index < 0)
        index = _pTabs->currentIndex();
    if (fname.isEmpty())
        fname = UNTITLED;

    _pTabs->setTabToolTip(index, fname);
    QString text = _FileNameToTabText(fname);
    if (_drawArea->IsModified(index))
        text = text + QChar(0x2757);
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
    {
        QIcon icon = ui.action_Eraser->icon();
        _drawArea->SetEraserCursor(&icon);
    }

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
    bool res = true;        // accept event
#ifndef _VIEWER
    if (ui.actionAutoSaveBackgroundImage->isChecked())
        _SaveBackgroundImage();

    int n = 0;
    _saveResult = srSaveSuccess;
    // save all changed
    while ((_saveResult = srSaveSuccess) && (n = _drawArea->IsModified(n,true)))    // returned : n = index of modified + 1
    {
        res &= _SaveIfYouWant(n - 1, !ui.actionAutoSaveData->isChecked()) == srSaveSuccess ? true : false;
    }
    if( (_saveResult == srSaveSuccess) && res)
		event->accept();
    else
		event->ignore();
#endif
    SaveState();        // viewer: always
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
    _AddNewTab();
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
        _AddToRecentList(_drawArea->HistoryName(UNTITLED));
    }
    _drawArea->EnableRedraw(true);

    return res;
}

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
    if (!IsOverwritable())
        n = _AddNewTab(fileName, true);   // sets current tab and adds history item and set it to current too
    else
        _drawArea->SetHistoryName(fileName);
                            
    if (n>= 0 && !_LoadData(-1))
        fileName.clear();   // load error
    if (n >= 0)
        _SetTabText(n, fileName);
    _SetWindowTitle(fileName);

#ifndef _VIEWER
    if (_eraserOn)
        on_action_Eraser_triggered();
#endif
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
            _SetTabText(_pTabs->currentIndex(), fileName);
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
        QMessageBox::warning(this, tr("FalconBoard - Warning"), QString(tr("Please restart the program to change the language!")).arg(which));
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
    QString fname = _drawArea->HistoryName(UNTITLED);
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
    QString fname = _drawArea->HistoryName(UNTITLED);
    if (fname.isEmpty())
        fname = UNTITLED;
    QString initialPath = _lastDir + fname; // QDir::currentPath() + "/untitled.mwb";
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

    QString initialPath = QDir::currentPath() + "/untitled." + _fileFormat;

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
        QString(tr("Open source White/blackboard application")+
            tr("<p>Version ")+ sVersion+ "</p>")+
            tr("<p>© A. Sólyom (2020)</p><br>"
            "<p>https://github.com/solyoma/FalconBoard</p>"
            "<p>Based on Qt's <b>Scribble</b> example.</p>"));
}

void FalconBoard::on_actionHelp_triggered()
{
    QMessageBox::about(this, tr(WindowTitle "help"),
        tr("<p><b>Move paper</b><br>")+
        tr("  with the mouse or pen while holding down the spacebar,<br>"
           "  using the arrow keys alone or with Ctrl.</p>")+
        tr("<p><b>Keyboard Shortcuts not shown on menus/buttons:</b></p>")+
        tr("<p><i>To start of line</i><br>&nbsp;&nbsp;Home</p>")+
        tr("<p><i>To end of line</i><br>&nbsp;&nbsp;End</p>")+
        tr("<p><i>To start of document</i><br>&nbsp;&nbsp;Ctrl+Home</p>") +
        tr("<p><i>To End of Document</i><br>&nbsp;&nbsp;Ctrl+End</p>")+
        tr("<p><i>To lowest position used so far</i><br>&nbsp;&nbsp;End</p>") +
        tr("<p><i>Up/Down/Left/Right</i> 10 pixels with the arrow keys,<br>100 pixels if you hold down Ctrl</p>")
#ifndef _VIEWER
        +
        tr("<p>F4<br>&nbsp;&nbsp;Take a screenshot of an area using the right mouse button. "
              " An image can be selected by clicking/tapping on it while holding down the Ctrl key.</p>")+
        tr("<p><i>Select brush colors</i><br>&nbsp;&nbsp;1, 2, 3, 4, 5. Eraser: E</p>")+
        tr("<p>Left and right bracket keys '[',']' change brush size</p>")+
        tr("<p>Draw a straight line from last position by holding down a Shift key while pressing Left, ")+
        tr("draw horizontal or vertical line by <i>first</i> pressing Left, then holding Shift key while drawing. ")+
        tr("Line orientation depends on which direction you start to draw.</p>")+

        tr("<p><b>Area selection</b><br>")+
        tr(   "Press and hold the <i>right mouse (pen) button</i> or "
              "press and hold <i>Ctrl</i> plus the <i>left mouse button</i> "
              "and move the mouse (pen) around. Release the buttons when finished. "
              "Hold the Shift key to get a square selection.")+
        tr("If the selection contains one or more complete lines or even eraser strokes (!) "
           "then the selection will shrink around those.</p>")+
        tr("<p><b>Keyboard shortcuts when there is an active selection</b></p>")+
        tr("<p>1, 2, 3, 4, 5 <br>&nbsp;&nbsp;<i>Recolor <b>completely</b> selected drawings</i></p>")+
        tr("<p>0, 8, 9, H, V<br>&nbsp;&nbsp;<i>Rotate or flip <b>completely</b> selected drawings</i></p>")+
        tr("<p>Ctrl+Ins, Ctrl+C<br>&nbsp;&nbsp;<i>Copy selected</i></p>") +
        tr("<p>Ctrl+X, Shift+Del<br>&nbsp;&nbsp;<i>Cut selected</i></p>") +
        tr("<p>Del, BackSpace<br>&nbsp;&nbsp;<i>Delete selected</i></p>") +
        tr("<p>R key<br>&nbsp;&nbsp;<i>Draw a rectangle</i> around the selected area.")+
        tr("<p>C key<br>&nbsp;&nbsp;<i>Draw an ellipse (or circle)</i> inside the selected area.")+
        tr("<p><i>Move</i> selected drawings with Left button, move copy by Alt+Left button.</p>")+
        tr("<p>Copy selection with Ctrl+C, Paste by selecting an area at destination then use Ctrl+V.</p>")+
        tr("<p>F5<br>&nbsp;&nbsp;<i>Insert vertical space</i> from top of selected area<br>"
           "(Moves all drawings down below the top of this selection)</p>")
#endif
    );
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
    if (!_drawArea->IsModified(index) || _SaveIfYouWant(index, true))
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
    _drawArea->SetGridOn(ui.actionShowGrid->isChecked(), ui.actionFixedGrid->isChecked());
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
    if (!_drawArea->RecolorSelected(Qt::Key_1))
    {
        _SetBlackPen();
        _SetCursor(DrawArea::csPen);
        _SetPenWidth(penBlack);
        ui.centralWidget->setFocus();
    }
};
void FalconBoard::on_action_Red_triggered() 
{
    if (!_drawArea->RecolorSelected(Qt::Key_2))
    {
        _SetRedPen();
        _SetCursor(DrawArea::csPen);
        _SetPenWidth(penRed);
        ui.centralWidget->setFocus();
    }
};
void FalconBoard::on_action_Green_triggered()
{
    if (!_drawArea->RecolorSelected(Qt::Key_3))
    {
        _SetGreenPen();
        _SetCursor(DrawArea::csPen);
        _SetPenWidth(penGreen);
        ui.centralWidget->setFocus();
    }
};
void FalconBoard::on_action_Blue_triggered()
{
    if (!_drawArea->RecolorSelected(Qt::Key_4))
    {
        _SetBluePen();
        _SetCursor(DrawArea::csPen);
        _SetPenWidth(penBlue);
        ui.centralWidget->setFocus();
    }
};
void FalconBoard::on_action_Yellow_triggered()
{
    if (!_drawArea->RecolorSelected(Qt::Key_5))
    {
        _SetYellowPen();
        _SetCursor(DrawArea::csPen);
        _SetPenWidth(penYellow);
        ui.centralWidget->setFocus();
    }
};

void FalconBoard::on_action_Eraser_triggered()
{
    _eraserOn = true;
    _SetPenWidth(_actPen = penEraser);
    QIcon icon = ui.action_Eraser->icon();
    _drawArea->SetCursor(DrawArea::csEraser, &icon);
    _SelectPenForAction(ui.action_Eraser);
    SlotForFocus();
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

void FalconBoard::on_actionDrawEllipse_triggered()
{
    _drawArea->SynthesizeKeyEvent(Qt::Key_C);
}

void FalconBoard::on_action_Screenshot_triggered()
{
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    plblScreen = new Snipper(nullptr);
    plblScreen->setGeometry(screen->geometry());
    _ConnectDisconnectScreenshotLabel(true);

    hide();

    QThread::msleep(300);

    plblScreen->setPixmap(screen->grabWindow(0));

    show();
    plblScreen->show();
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
        _SetCursor(DrawArea::csPen);

}

void FalconBoard::on_actionRedo_triggered()
{
    _drawArea->Redo();
    _SelectPen();
    if (_eraserOn)
        on_action_Eraser_triggered();
    else
        _SetCursor(DrawArea::csPen);
}

void FalconBoard::on_action_InsertVertSpace_triggered()
{
    _drawArea->InsertVertSpace();
}

void FalconBoard::SlotForRubberBandSelection(int on)
{
    ui.action_InsertVertSpace->setEnabled(on);
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
    _SetTabText(-1, _drawArea->HistoryName());
}

void FalconBoard::SlotForRedo(bool b)
{
    ui.actionRedo->setEnabled(b);
    _SetTabText(-1, _drawArea->HistoryName());
}

void FalconBoard::SlotForFocus()
{
    ui.centralWidget->setFocus();
}

void FalconBoard::SlotForPointerType(QTabletEvent::PointerType pt)   // only sent by tablet
{
    static bool penEraser = false;     // set to true if not erasemode just
    static FalconPenKind pk = penBlack;
    switch (pt)                         // eraser end of stylus used
    {
        case QTabletEvent::Eraser:      // only when eraser side selected
            if (!_eraserOn)
            {
                pk = _actPen;           // save for restoring
                penEraser = true;
                on_action_Eraser_triggered();
            }
            break;
        default:
            if (penEraser)
            {
                _SetCursor(DrawArea::csPen);
                _SetPenKind(pk);
                penEraser = false;
            }
            break;
    }
}

void FalconBoard::SlotForScreenshotReady(QRect gmetry)
{
    plblScreen->hide();

    QPixmap pixmap;  // need a pixmap for transparency (used Qpixmap previously)
    pixmap = QPixmap(gmetry.size()); //  , Qpixmap::Format_ARGB32);

    QPainter *painter = new QPainter(&pixmap);   // need to delete it before the label is deleted
    painter->drawPixmap(QPoint(0,0), *plblScreen->pixmap(), gmetry);
    delete painter;

    if (_useScreenshotTransparency)
    {
        QBitmap bm = pixmap.createMaskFromColor(_screenshotTransparencyColor);
        pixmap.setMask(bm);
    }

    _drawArea->AddScreenShotImage(pixmap);

    _ConnectDisconnectScreenshotLabel(false);
    delete plblScreen;
    plblScreen = nullptr;

}

void FalconBoard::SlotForScreenShotCancelled()
{
    plblScreen->hide();
    _ConnectDisconnectScreenshotLabel(false);
    delete plblScreen;
    plblScreen = nullptr;
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
    _drawArea->PageSetup();
}

void FalconBoard::on_actionExportToPdf_triggered()
{
    QString saveName = _drawArea->HistoryName(UNTITLED);
    int pos = saveName.lastIndexOf('/');
    QString name = _lastPDFDir + saveName.mid(pos+1);
    _drawArea->ExportPdf(name, _lastPDFDir);
}


