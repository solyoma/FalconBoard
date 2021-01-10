﻿#include <QSpinBox>
#include <QLabel>
#include <QScreen>
#include <QPainter>
#include <QThread>
#include <QSettings>
#include "DrawArea.h"
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

    ui.actionSaveData->setVisible(false);
    ui.actionSaveBackgroundImage->setVisible(false);

    removeToolBar(ui.mainToolBar);
}
#endif

FalconBoard::FalconBoard(QWidget *parent)	: QMainWindow(parent)
{
	ui.setupUi(this);

    _drawArea = static_cast<DrawArea*>(ui.centralWidget);
    _drawArea->SetPenKind(_actPen, _penWidth[_actPen-1]); 

    _CreateAndAddActions();
    connect(&_signalMapper, SIGNAL(mapped(int)), SLOT(_sa_actionRecentFile_triggered(int)));

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
    RestoreState();

    _SetupIconsForPenColors(_screenMode);

    ui.centralWidget->setFocus();
}

void FalconBoard::RestoreState()
{
    QSettings s("FalconBoard.ini", QSettings::IniFormat);

    restoreGeometry(s.value("geometry").toByteArray());
    restoreState(s.value("windowState").toByteArray());

    QString qs = s.value("version", sVersion).toString();
    if (qs != sVersion)
        return;
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
    
    ui.actionSaveData->setChecked(s.value("saved", false).toBool());
    ui.actionSaveBackgroundImage->setChecked(s.value("saveb", false).toBool());

    qs = s.value("img", QString()).toString();
    if (!qs.isEmpty())
        _drawArea->OpenBackgroundImage(qs);
#endif
    qs = s.value("data", QString()).toString();
    if (!qs.isEmpty())
    {
        _saveName = qs;      // only load on show()  _LoadData(qs);
        setWindowTitle(sWindowTitle + QString(" - %1").arg(_saveName));
    }
    _lastDir  = s.value("lastDir",  "").toString();
    _lastFile = s.value("lastFile", "untitled.mwb").toString();
    _sImageName = s.value("bckgrnd", "").toString();
#ifndef _VIEWER
    if (!_sImageName.isEmpty() && !_drawArea->OpenBackgroundImage(_sImageName))
        _sImageName.clear();
#endif
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
	QSettings s("FalconBoard.ini", QSettings::IniFormat);

	s.setValue("geometry", saveGeometry());
	s.setValue("windowState", saveState());

	s.setValue("version", sVersion);
	s.setValue("mode", _screenMode == smSystem ? "s" : _screenMode == smDark ? "d" : "b");
	s.setValue("grid", (ui.actionShowGrid->isChecked() ? 1 : 0) + (ui.actionFixedGrid->isChecked() ? 2 : 0));
	s.setValue("pageG", ui.actionShowPageGuides->isChecked() ? 1 : 0);
    s.setValue("limited", ui.actionLimitedPage->isChecked());
#ifndef _VIEWER
	s.setValue("size", QString("%1,%2,%3,%4,%5").arg(_penWidth[0]).arg(_penWidth[1]).arg(_penWidth[2]).arg(_penWidth[3]).arg(_penWidth[4]));
	s.setValue("saved", ui.actionSaveData->isChecked());
	s.setValue("saveb", ui.actionSaveBackgroundImage->isChecked());
	if (ui.actionSaveBackgroundImage->isChecked())
		s.setValue("img", _sImageName);
#endif
	s.setValue("data", _saveName);
	s.setValue("lastDir", _lastDir);
	s.setValue("lastFile", _lastFile);
	s.setValue("bckgrnd", _sImageName);

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
    ui.action_Black->setText(sm == smSystem ? "Blac&k" : "&White");
    ui.action_Red->setIcon(_ColoredIcon(_iconPen,   drawColors[penRed]));
    ui.action_Green->setIcon(_ColoredIcon(_iconPen, drawColors[penGreen]));
    ui.action_Blue->setIcon(_ColoredIcon(_iconPen,  drawColors[penBlue]));
    ui.action_Yellow->setIcon(_ColoredIcon(_iconPen,drawColors[penYellow]));
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
    ui.mainToolBar->addAction(ui.action_Screenshot);

    ui.mainToolBar->addSeparator();
    _plblMsg = new QLabel();
    ui.mainToolBar->addWidget(_plblMsg);

    // more than one valueChanged() function exists
    connect(_psbPenWidth, QOverload<int>::of(&QSpinBox::valueChanged), this, &FalconBoard::slotPenWidthChanged);
    connect(_psbPenWidth, &QSpinBox::editingFinished, this, &FalconBoard::slotPenWidthEditingFinished);

    connect(_drawArea, &DrawArea::CanUndo, this, &FalconBoard::SlotForUndo);
    connect(_drawArea, &DrawArea::CanRedo, this, &FalconBoard::SlotForRedo);
    connect(_drawArea, &DrawArea::WantFocus, this, &FalconBoard::SlotForFocus);
    connect(_drawArea, &DrawArea::TextToToolbar, this, &FalconBoard::SlotForLabel);
    connect(_drawArea, &DrawArea::IncreaseBrushSize, this, &FalconBoard::SlotIncreaseBrushSize);
    connect(_drawArea, &DrawArea::DecreaseBrushSize, this, &FalconBoard::SlotDecreaseBrushSize);

    connect(ui.actionClearHistory, &QAction::triggered, _drawArea, &DrawArea::ClearHistory);
    
#endif
}

#ifndef _VIEWER
void FalconBoard::_AddSaveVisibleAsMenu()
{
    QMenu *pSaveAsVisibleMenu = new QMenu(QApplication::tr("Save Visible &As..."), this);
    for (QAction* action : qAsConst(_saveAsActs))
        pSaveAsVisibleMenu->addAction(action);
    ui.menu_File->insertMenu(ui.actionPageSetup, pSaveAsVisibleMenu);
    ui.menu_File->insertSeparator(ui.actionPageSetup);
}

bool FalconBoard::_SaveIfYouWant(bool mustAsk)
{
    if (_drawArea->IsModified() )
    {
        QMessageBox::StandardButton ret;
        if (!ui.actionSaveData->isChecked() || mustAsk || _saveName.isEmpty())
        {
            ret = QMessageBox::warning(this, tr(WindowTitle),
                tr("Data have been modified.\n"
                    "Do you want to save your changes?"),
                QMessageBox::Save | QMessageBox::Discard
                | QMessageBox::Cancel);
        }
        else
            ret = QMessageBox::Save;

        if (ret == QMessageBox::Save)
        {
            if (_saveName.isEmpty())
                on_actionSaveAs_triggered();
            else 
                return _SaveFile();
        }
        else if (ret == QMessageBox::Cancel)
            return false;
    }
    return true;
}

bool FalconBoard::_SaveFile()
{
    if (_drawArea->Save(_saveName))
    {
        _AddToRecentList(_saveName);
        return true;
    }
    return false;
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

void FalconBoard::_SetPenKind(MyPenKind newPen)
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

void FalconBoard::_SetPenWidth(MyPenKind pk)
{
    _drawArea->SetPenKind(pk, _penWidth[pk-1]);
    ++_busy;
    _psbPenWidth->setValue(_penWidth[pk-1]);
    --_busy;
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
            _sTextColor = "#000000";
            _sDisabledColor = "#AAAAAA";
            _sGridColor = "#d0d0d0";
            _sPageGuideColor = "#fcd475";
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
            _sTextColor = "#E1E1E1";
            _sDisabledColor = "#AAAAAA";
            _sGridColor = "#202020";
            _sPageGuideColor = "#413006";
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
            _sTextColor = "#CCCCCC";
            _sDisabledColor = "#888888";
            _sGridColor = "#202020";
            _sPageGuideColor = "#2e2204";
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
        ;

    setStyleSheet(ss);

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

    QMenu* pMenu;
    QAction* pAction;

    // add items

    QString s;
    for (int i = 0; i < _recentList.size(); ++i)
    {
        s = QString("%1. %2").arg(i + 1).arg(_recentList[i]);
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

void FalconBoard::closeEvent(QCloseEvent* event)
{
#ifndef _VIEWER
    if (_SaveIfYouWant())
	{
		event->accept();
#endif
	    SaveState();        // viewer: always
#ifndef _VIEWER
	}
	else
		event->ignore();
#endif
}

void FalconBoard::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);

    if (!_firstShown)
    {
        _firstShown = true;
        if (!_saveName.isEmpty())
            _LoadData(_saveName);
    }
}

#ifndef _VIEWER
void FalconBoard::on_actionNew_triggered()
{
    if (!_SaveIfYouWant(true))    // must ask if data changed, false: cancelled
        return;

    bool b = QMessageBox::question(this, "falconBoard", tr("Do you want to limit the editable area horizontally to the screen width?\n"
                                                           " You may change this any time in Options/Page"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
    ui.actionLimitedPage->setChecked(b);
    _drawArea->SetLimitedPage(b);
    _drawArea->NewData();
    _saveName.clear();
    setWindowTitle(sWindowTitle);
    _backgroundImageName.clear();
}
#endif
void FalconBoard::_LoadData(QString fileName)
{
    if (!fileName.isEmpty())
    {
        _drawArea->Load(fileName);
#ifndef _VIEWER
        _SetPenKind();
#endif
        _saveName = fileName;

        _AddToRecentList(_saveName);
    }
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

void FalconBoard::on_actionLoad_triggered()
{
#ifndef _VIEWER
    _SaveIfYouWant(true);   // must ask if data changed
#endif
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Load Data"), 
                                                    _lastDir, // QDir::currentPath(),
                                                    tr("FalconBoard files (*.mwb);;All files (*)"));
    _SaveLastDirectory(fileName);
    _LoadData(fileName);
    setWindowTitle(sWindowTitle + QString(" - %1").arg(_saveName));

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
    QString& fileName = _recentList[which];
    _SaveLastDirectory(fileName);
    _LoadData(fileName);
    setWindowTitle(sWindowTitle + QString(" - %1").arg(_saveName));
//    _busy = false;
}

void FalconBoard::_SaveLastDirectory(QString fileName)
{
    if (!fileName.isEmpty())
    {
        int n = fileName.lastIndexOf('/');
        _lastDir = fileName.left(n + 1);
        _lastFile = fileName.mid(n + 1);
    }
}

#ifndef _VIEWER
void FalconBoard::on_actionSave_triggered()
{
    if (_saveName.isEmpty())
        on_actionSaveAs_triggered();
    else
        _SaveFile();
}
void FalconBoard::on_actionSaveAs_triggered()
{
    QString initialPath = _lastDir + _lastFile; // QDir::currentPath() + "/untitled.mwb";
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
                        initialPath, tr("FalconBoard Files (*.mwb);; All Files (*))"));
    if (fileName.isEmpty())
        return;
    _SaveLastDirectory(fileName);
    _saveName = fileName;
    _SaveFile();
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
        tr("<p><i>To start of document</i><br>&nbsp;&nbsp;Ctrl+Home</p>") +
        tr("<p><i>To lowest position used so far</i><br>&nbsp;&nbsp;End</p>") +
        tr("<p><i>Up/Down/Left/Right</i> 10 pixels with the arrow keys,<br>100 pixels if you hold down Ctrl End</p>")
#ifndef _VIEWER
        +
        tr("<p>Left and right bracket keys '[',']' change brush size</p>")+
        tr("<p><i>Select colors</i><br>&nbsp;&nbsp;1, 2, 3, 4, 5, eraser: E</p>")+
        tr("<p><i>Draw rectangle</i> around selected area<br>&nbsp;&nbsp;R key")+
        tr("Insert vertical space from top of selected area - F5</p>")+
        tr("<p><i>Recolor selected</i><br>&nbsp;&nbsp;1, 2, 3, 4, 5 </p>")+
        tr("<p><i>Copy selected</i><br>&nbsp;&nbsp;Ctrl+Ins, Ctrl+C</p>") +
        tr("<p><i>Cut selected</i><br>&nbsp;&nbsp;Ctrl+X, Shift+Del</p>") +
        tr("<p><i>Delete selected</i><br>&nbsp;&nbsp;Del, BackSpace</p>") +
        "<br>" +
        tr("<p><i>Select</i> drawings with right button.</p>") +
        tr("<p>Selected drawings can be deleted, copied or cut out by keyboard shortcuts./p>") +
        tr("<p>To paste selection select destination with right button and use keyboard shortcut.</p>")+
        tr("<p>Draw horizontal or vertical lines by holding down a Shift key while drawing.</p>")+
        tr("<p>Take a screenshot of an area using the right mouse button - F4</p>")
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

void FalconBoard::on_actionShowGrid_triggered()
{
    _drawArea->SetGridOn(ui.actionShowGrid->isChecked(), ui.actionFixedGrid->isChecked());
}

void FalconBoard::on_actionFixedGrid_triggered()
{
    _drawArea->SetGridOn(ui.actionShowGrid->isChecked(), ui.actionFixedGrid->isChecked());
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
}

void FalconBoard::SlotForRedo(bool b)
{
    ui.actionRedo->setEnabled(b);
}

void FalconBoard::SlotForFocus()
{
    ui.centralWidget->setFocus();
}

void FalconBoard::SlotForPointerType(QTabletEvent::PointerType pt)   // only sent by tablet
{
    static bool penEraser = false;     // set to true if not erasemode just
    static MyPenKind pk = penBlack;
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

    QImage image;
    image =QImage(gmetry.size(), QImage::Format_ARGB32);

    QPainter *painter = new QPainter(&image);   // need to delete it before the label is deleted
    painter->drawImage(QPoint(0,0), plblScreen->pixmap()->toImage(), gmetry);
    delete painter;
    _drawArea->AddScreenShotImage(image);

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
void FalconBoard::SlotForPenKindChange(MyPenKind pk)
{
    _SetPenKind(pk);
}
#endif

void FalconBoard::on_actionPageSetup_triggered()
{
    _drawArea->PageSetup();
}


