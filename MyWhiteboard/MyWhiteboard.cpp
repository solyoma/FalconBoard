#include "MyWhiteboard.h"
#include <QSpinBox>
#include <QLabel>
#include <QScreen>
#include <QPainter>
#include <QThread>
#include <QSettings>
#include "DrawArea.h"

MyWhiteboard::MyWhiteboard(QWidget *parent)	: QMainWindow(parent)
{
	ui.setupUi(this);

    _drawArea = static_cast<DrawArea*>(ui.centralWidget);
    _drawArea->SetPenKind(_actPen);
    _drawArea->SetPenWidth(_penWidth); 

    _CreateAndAddActions();
    _AddSaveAsVisibleMenu();

    QCoreApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents); // for tablet

    connect(_drawArea, &DrawArea::PointerTypeChange, this, &MyWhiteboard::SlotForPointerType);

    RestoreState();

    ui.centralWidget->setFocus();
}

static const QString sVersion = "1.0";
void MyWhiteboard::RestoreState()
{
    QSettings s("MyWhiteboard.ini", QSettings::IniFormat);

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

    int n = s.value("size", 3).toInt();
    _penWidth = n;
    _psbPenWidth->setValue(n);
    n = s.value("esize", 30).toInt();
    _eraserWidth = n;
    
    ui.actionSaveData->setChecked(s.value("saved", false).toBool());
    ui.actionSaveBackgroundImage->setChecked(s.value("saveb", false).toBool());

    qs = s.value("img", QString()).toString();
    if (!qs.isEmpty())
        _drawArea->OpenBackgroundImage(qs);

    qs = s.value("data", QString()).toString();
    if (!qs.isEmpty())
        _saveName = qs;      // only load on show()  _LoadData(qs);
    _lastDir  = s.value("lastDir",  "").toString();
    _lastFile = s.value("lastFile", "untitled.mwb").toString();
}

void MyWhiteboard::SaveState()
{
    QSettings s("MyWhiteboard.ini",QSettings::IniFormat);

    s.setValue("geometry", saveGeometry());
    s.setValue("windowState", saveState());

    s.setValue("version", sVersion);
    s.setValue("mode", _screenMode == smSystem ? "s" :_screenMode == smDark ? "d" : "b");
    s.setValue("size", _penWidth);
    s.setValue("esize", _eraserWidth);
    s.setValue("saved", ui.actionSaveData->isChecked());
    s.setValue("saveb", ui.actionSaveBackgroundImage->isChecked());
    if (/* !_sImageName.isEmpty() &&*/ ui.actionSaveBackgroundImage->isChecked())
        s.setValue("img", _sImageName);
//    if (/* ui.actionUndo->isEnabled()  && !_saveName.isEmpty() && */ ui.actionSaveData->isChecked() )
        s.setValue("data", _saveName);
/*    if(!_lastDir.isEmpty()) */
        s.setValue("lastDir", _lastDir);
/*    if(!_lastFile.isEmpty() && _lastFile != "untitled.mwb")*/
        s.setValue("lastFile", _lastFile);
}

void MyWhiteboard::_CreateAndAddActions()
{                           // add filetype selection submenu for save image
    const QList<QByteArray> imageFormats = QImageWriter::supportedImageFormats();
    for (const QByteArray& format : imageFormats) {
        QString text = tr("%1...").arg(QString::fromLatin1(format).toUpper());

        QAction* action = new QAction(text, this);
        action->setData(format);
        connect(action, &QAction::triggered, this, &MyWhiteboard::SaveVisibleAsTriggered);
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
    ui.mainToolBar->addAction(ui.action_Print);
    ui.mainToolBar->addSeparator();

    ui.mainToolBar->addAction(ui.action_Black);
    ui.mainToolBar->addAction(ui.action_Red);
    ui.mainToolBar->addAction(ui.action_Green);
    ui.mainToolBar->addAction(ui.action_Blue);
    ui.mainToolBar->addAction(ui.action_Eraser);

    _penGroup = new QActionGroup(this);
    _penGroup->addAction(ui.action_Black);
    _penGroup->addAction(ui.action_Red);
    _penGroup->addAction(ui.action_Green);
    _penGroup->addAction(ui.action_Blue);
    _penGroup->addAction(ui.action_Eraser);

    _modeGroup = new QActionGroup(this);
    _modeGroup->addAction(ui.actionLightMode);
    _modeGroup->addAction(ui.actionDarkMode);
    _modeGroup->addAction(ui.actionBlackMode);

    ui.mainToolBar->addSeparator();


    ui.mainToolBar->addWidget(new QLabel(tr("Pen Width:")));
    _psbPenWidth = new QSpinBox();
    _psbPenWidth->setMinimum(1);
    _psbPenWidth->setMaximum(200);
    _psbPenWidth->setSingleStep(1);
    _psbPenWidth->setValue(_penWidth);
    QRect rect = _psbPenWidth->geometry();
    rect.setWidth(30);
    _psbPenWidth->setGeometry(rect);
    ui.mainToolBar->addWidget(_psbPenWidth);

    ui.mainToolBar->addSeparator();
    ui.mainToolBar->addAction(ui.action_Screenshot);

    ui.mainToolBar->addSeparator();
    _plblMsg = new QLabel();
    ui.mainToolBar->addWidget(_plblMsg);

    // more than one valueChanged() function exists
    connect(_psbPenWidth, QOverload<int>::of(&QSpinBox::valueChanged), this, &MyWhiteboard::slotPenWidthChanged);

    connect(_drawArea, &DrawArea::CanUndo, this, &MyWhiteboard::SlotForUndo);
    connect(_drawArea, &DrawArea::CanRedo, this, &MyWhiteboard::SlotForRedo);
    connect(_drawArea, &DrawArea::WantFocus, this, &MyWhiteboard::SlotForFocus);
    connect(_drawArea, &DrawArea::TextToToolbar, this, &MyWhiteboard::SlotForLabel);

    connect(ui.actionClearHistory, &QAction::triggered, _drawArea, &DrawArea::ClearHistory);
}

void MyWhiteboard::_AddSaveAsVisibleMenu()
{
    QMenu *pSaveAsVisibleMenu = new QMenu(QApplication::tr("Save Visible &As..."), this);
    for (QAction* action : qAsConst(_saveAsActs))
        pSaveAsVisibleMenu->addAction(action);
    ui.menu_File->insertMenu(ui.action_Print, pSaveAsVisibleMenu);
    ui.menu_File->insertSeparator(ui.action_Print);
}

bool MyWhiteboard::_SaveIfYouWant(bool mustAsk)
{
    if (_drawArea->IsModified() ) 
    {
        QMessageBox::StandardButton ret;
        if (!ui.actionSaveData->isChecked() || mustAsk || _saveName.isEmpty())
        {
            ret = QMessageBox::warning(this, tr("MyWhiteboard"),
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

bool MyWhiteboard::_SaveFile()
{
    return _drawArea->Save(_saveName);
}

bool MyWhiteboard::_SaveBackgroundImage()
{
    return _drawArea->SaveVisibleImage(_backgroundImageName, _fileFormat.constData());
}

void MyWhiteboard::_SelectPenForAction(QAction* paction)
{
    paction->setChecked(true);
}

void MyWhiteboard::_SelectPen()     // call after '_actPen' is set
{
    switch (_actPen)
    {
        default:
        case penBlack: ui.action_Black->setChecked(true); break;
        case penRed: ui.action_Red->setChecked(true); break;
        case penGreen: ui.action_Green->setChecked(true); break;
        case penBlue: ui.action_Blue->setChecked(true); break;
        case penEraser:on_action_Eraser_triggered(); return;
    }
    _SetPenKind();
}

void MyWhiteboard::_SetPenKind()
{
    _eraserOn = _actPen == penEraser;
    _drawArea->SetPenKind(_actPen);
    _busy = true;
    _psbPenWidth->setValue(_penWidth);
    _busy = false;
    ui.centralWidget->setFocus();
}

void MyWhiteboard::_SetPenKind(MyPenKind newPen)
{
    _actPen = newPen;
    _SelectPen();
}


void MyWhiteboard::_SetCursor(DrawArea::CursorShape cs)
{
    _drawArea->SetCursor(cs);
}

void MyWhiteboard::_SetBlackPen() { _SetPenKind(penBlack); }
void MyWhiteboard::_SetRedPen()   { _SetPenKind  (penRed); }
void MyWhiteboard::_SetGreenPen() { _SetPenKind(penGreen); }
void MyWhiteboard::_SetBluePen()  { _SetPenKind (penBlue); }

void MyWhiteboard::_SetPenWidth()
{
    int pw = _eraserOn ? _eraserWidth : _penWidth;
    if (_eraserOn)
        _drawArea->SetEraserWidth(pw);
    else
        _drawArea->SetPenWidth(pw);
    _busy = true;
    _psbPenWidth->setValue(pw);
    _busy = false;
}

void MyWhiteboard::_ConnectDisconnectScreenshotLabel(bool join )
{

    if (join)
    {
        connect(plblScreen, &Snipper::SnipperCancelled, this, &MyWhiteboard::SlotForScreenShotCancelled);
        connect(plblScreen, &Snipper::SnipperReady, this, &MyWhiteboard::SlotForScreenshotReady);
    }
    else
    {
        disconnect(plblScreen, &Snipper::SnipperCancelled, this, &MyWhiteboard::SlotForScreenShotCancelled);
        disconnect(plblScreen, &Snipper::SnipperReady, this, &MyWhiteboard::SlotForScreenshotReady);
    }
}

void MyWhiteboard::_SetupMode(ScreenMode mode)
{
    QString ss,
        sWhite = "white_",  // prefix for dark modes
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

    switch (mode)
    {
        default:
        case smSystem:
            ui.action_Black->setIcon(QIcon(":/MyWhiteboard/Resources/black.png"));
            _sBackgroundColor = "#FFFFFF";
            _sTextColor = "#000000";
            sWhite.clear();
            break;
        case smDark:
            _sBackgroundColor = "#282828";
            _sTextColor = "#E1E1E1";
            ui.action_Black->setIcon(QIcon(":/MyWhiteboard/Resources/white.png")); // it still be called 'penBlack' though
            break;
        case smBlack:
            _sBackgroundColor = "#191919";
            _sTextColor = "#CCCCCC";
            ui.action_Black->setIcon(QIcon(":/MyWhiteboard/Resources/white.png"));
            break;
    }
    ui.actionExit->setIcon(QIcon(":/MyWhiteboard/Resources/" + sWhite + actions[0] + sPng));
    ui.action_Eraser->setIcon(QIcon(":/MyWhiteboard/Resources/" + sWhite + actions[1] + sPng));
    ui.actionNew->setIcon(QIcon(":/MyWhiteboard/Resources/" + sWhite + actions[2] + sPng));
    ui.actionLoad->setIcon(QIcon(":/MyWhiteboard/Resources/" + sWhite + actions[3] + sPng));
    ui.actionRedo->setIcon(QIcon(":/MyWhiteboard/Resources/" + sWhite + actions[4] + sPng));
    ui.actionSave->setIcon(QIcon(":/MyWhiteboard/Resources/" + sWhite + actions[5] + sPng));
    ui.action_Screenshot->setIcon(QIcon(":/MyWhiteboard/Resources/" + sWhite + actions[6] + sPng));
    ui.actionUndo->setIcon(QIcon(":/MyWhiteboard/Resources/" + sWhite + actions[7] + sPng));

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
             "}\n";

    setStyleSheet(ss);

    _drawArea->SetMode(mode != smSystem, _sBackgroundColor);      // set penBlack color to White
}

void MyWhiteboard::closeEvent(QCloseEvent* event)
{
	if (_SaveIfYouWant())
	{
		event->accept();
	}
	else
		event->ignore();
	SaveState();        // TODO: set what to save state
}

void MyWhiteboard::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);

    if (!_firstShown)
    {
        _firstShown = true;
        if (!_saveName.isEmpty())
            _LoadData(_saveName);
    }
}

void MyWhiteboard::on_actionNew_triggered()
{
    _SaveIfYouWant(true);   // must ask if data changed
    _drawArea->NewData();
    _saveName.clear();
    _backgroundImageName.clear();
}

void MyWhiteboard::_LoadData(QString fileName)
{
    if (!fileName.isEmpty())
    {
        _drawArea->Load(fileName);
        _SetPenKind();
        _saveName = fileName;
    }
}

void MyWhiteboard::on_actionLoad_triggered()
{
    _SaveIfYouWant(true);   // must ask if data changed
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Load Data"), 
                                                    _lastDir, // QDir::currentPath(),
                                                    tr("MyWhiteboard files (*.mwb);;All files (*)"));
    _SaveLastDirectory(fileName);
    _LoadData(fileName);

    if (_eraserOn)
        on_action_Eraser_triggered();
}

void MyWhiteboard::on_actionSave_triggered()
{
    if (_saveName.isEmpty())
        on_actionSaveAs_triggered();
    else
        _SaveFile();
}
void MyWhiteboard::_SaveLastDirectory(QString fileName)
{
    if (!fileName.isEmpty())
    {
        int n = fileName.lastIndexOf('/');
        _lastDir = fileName.left(n + 1);
        _lastFile = fileName.mid(n + 1);
    }
}

void MyWhiteboard::on_actionSaveAs_triggered()
{
    QString initialPath = _lastDir + _lastFile; // QDir::currentPath() + "/untitled.mwb";
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
                        initialPath, tr("MyWhiteboard Files (*.mwb);; All Files (*))"));
    if (fileName.isEmpty())
        return;
    _SaveLastDirectory(fileName);
    _saveName = fileName;
    _SaveFile();
}

void MyWhiteboard::on_actionLoadBackground_triggered()
{
    _sImageName = QFileDialog::getOpenFileName(this,
                    tr("Open Background Image"), QDir::currentPath());
    if (!_sImageName.isEmpty())
        _drawArea->OpenBackgroundImage(_sImageName);
}

void MyWhiteboard::on_actionSaveVisible_triggered()
{
    if (_backgroundImageName.isEmpty())
        SaveVisibleAsTriggered();
    else
        _SaveBackgroundImage();
}

void MyWhiteboard::SaveVisibleAsTriggered()
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

void MyWhiteboard::on_actionAbout_triggered()
{
    QMessageBox::about(this, tr("About MyWhiteboard"),
        tr("<p>Based on Qt's <b>Scribble</b> example.</p>"
            "<p>Enhanced in many ways by A. Sólyom (2020)"
            "<p>Enhanced in many ways by A. Sólyom (2020)"
            "</p>"));
}

void MyWhiteboard::on_actionHelp_triggered()
{
    QMessageBox::about(this, tr("MyWhiteboard help"),
        tr("<p><b>Move paper</b><br>")+
        tr("  with the mouse or pen while holding down the spacebar,<br>"
           "  using the arrow keys alone or with Ctrl.</p>")+
        tr("<p><b>Keyboard Shortcuts not shown on menus/buttons:</b></p>")+
        tr("<p><i>To start of line</i><br>&nbsp;&nbsp;Home</p>")+
        tr("<p><i>To start of document</i><br>&nbsp;&nbsp;Ctrl+Home</p>") +
        tr("<p><i>To lowest position used so far</i><br>&nbsp;&nbsp;End</p>") +
        tr("<p><i>Up/Down/Left/Right</i> 10 pixels with the arrow keys,<br>100 pixels if you hold down Ctrl End</p>") +
        tr("<p><i>Select colors</i><br>&nbsp;&nbsp;Alt+1, ..., Alt+4</p>")+
        tr("<p><i>Draw rectangle</i> around selected area<br>&nbsp;&nbsp;R key")+
        tr("<p><i>Recolor selected</i><br>&nbsp;&nbsp;Ctrl+Alt+1, ..., Ctrl+Alt+4 </p>")+
        tr("<p><i>Copy selected</i><br>&nbsp;&nbsp;Ctrl+Ins, Ctrl+C</p>") +
        tr("<p><i>Cut selected</i><br>&nbsp;&nbsp;Ctrl+X, Shift+Del</p>") +
        tr("<p><i>Delete selected</i><br>&nbsp;&nbsp;Del, BackSpace</p>") +
        "<br>" +
        tr("<p><i>Select</i> drawings with right button.</p>") +
        tr("<p>Selected drawings can be deleted, copied or cut out by keyboard shortcuts./p>") +
        tr("<p>To paste selection select destination with right button and use keyboard shortcut.</p>")+
        tr("<p>Draw horizontal or vertical lines by holding down a Shift key while drawing.</p>")

    );
}

void MyWhiteboard::on_actionLightMode_triggered()
{
    _SetupMode(smSystem);
}

void MyWhiteboard::on_actionDarkMode_triggered()
{
    _SetupMode(smDark);
}

void MyWhiteboard::on_actionBlackMode_triggered()
{
    _SetupMode(smBlack);
}

void MyWhiteboard::on_action_Eraser_triggered()
{
    _eraserOn = true;
    _drawArea->SetPenKind(_actPen = penEraser);
    _drawArea->SetCursor(DrawArea::csEraser);
    _SetPenWidth();
    _SelectPenForAction(ui.action_Eraser);
    SlotForFocus();
}

void MyWhiteboard::on_action_Screenshot_triggered()
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

void MyWhiteboard::on_actionClearCanvas_triggered()
{
    _drawArea->ClearCanvas();
}

void MyWhiteboard::on_actionClearBackgroundImage_triggered()
{
    _drawArea->ClearBackground();
    _sImageName.clear();
}

void MyWhiteboard::on_actionUndo_triggered()
{
    _drawArea->Undo();                
    _SelectPen();
    if (_eraserOn)
        on_action_Eraser_triggered();
    else
        _SetCursor(DrawArea::csPen);

}

void MyWhiteboard::on_actionRedo_triggered()
{
    _drawArea->Redo();
    _SelectPen();
    if (_eraserOn)
        on_action_Eraser_triggered();
    else
        _SetCursor(DrawArea::csPen);
}

void MyWhiteboard::slotPenWidthChanged(int val)
{
    if (_busy)		// from program
        return;
    // from user
    if (_eraserOn)
        _eraserWidth = val;
    else
        _penWidth = val;

    _SetPenWidth();
}

void MyWhiteboard::SlotForUndo(bool b)
{
    ui.actionUndo->setEnabled(b);
}

void MyWhiteboard::SlotForRedo(bool b)
{
    ui.actionRedo->setEnabled(b);
}

void MyWhiteboard::SlotForFocus()
{
    ui.centralWidget->setFocus();
}

void MyWhiteboard::SlotForPointerType(QTabletEvent::PointerType pt)   // only sent by tablet
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
                _SetPenWidth();
                penEraser = false;
            }
            break;
    }
}

void MyWhiteboard::SlotForScreenshotReady(QRect geometry)
{
    plblScreen->hide();

    QSize size(geometry.width(), geometry.height());
    QImage image(size, QImage::Format_ARGB32);

    QPainter *painter = new QPainter(&image);   // need to delete it before the label is deleted
    painter->drawImage(QPoint(0,0), plblScreen->pixmap()->toImage(), geometry);
    delete painter;

    _drawArea->SetBackgroundImage(image);
    _ConnectDisconnectScreenshotLabel(false);
    delete plblScreen;
    plblScreen = nullptr;

}

void MyWhiteboard::SlotForScreenShotCancelled()
{
    plblScreen->hide();
    _ConnectDisconnectScreenshotLabel(false);
    delete plblScreen;
    plblScreen = nullptr;
}

void MyWhiteboard::SlotForLabel(QString text)
{
    _plblMsg->setText(text);
}
