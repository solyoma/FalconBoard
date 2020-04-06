#include "MyWhiteboard.h"
#include <QSpinBox>
#include <QLabel>
#include <QScreen>
#include <QPainter>
#include <QThread>
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
    ui.centralWidget->setFocus();
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
        // more than one valueChanged() function exists
    connect(_psbPenWidth, QOverload<int>::of(&QSpinBox::valueChanged), this, &MyWhiteboard::slotPenWidthChanged);

    connect(_drawArea, &DrawArea::CanUndo, this, &MyWhiteboard::SlotForUndo);
    connect(_drawArea, &DrawArea::CanRedo, this, &MyWhiteboard::SlotForRedo);
    connect(_drawArea, &DrawArea::WantFocus, this, &MyWhiteboard::SlotForFocus);


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

bool MyWhiteboard::_SaveIfYouWant()
{
    if (_drawArea->IsModified()) 
    {
        QMessageBox::StandardButton ret;
        ret = QMessageBox::warning(this, tr("MyWhiteboard"),
            tr("Data have been modified.\n"
                "Do you want to save your changes?"),
            QMessageBox::Save | QMessageBox::Discard
            | QMessageBox::Cancel);
        if (ret == QMessageBox::Save)
            return _SaveFile();
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
    QString ss;

    switch (mode)
    {
        default:
        case smLight:
            ui.action_Black->setIcon(QIcon(":/MyWhiteboard/Resources/black.png"));
            _sBackgroundColor = "#FFFFFF";
            _sTextColor = "#000000";
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
    if(mode != smLight)
        ss = "* {\n background-color:" + _sBackgroundColor + ";\n  color:" + _sTextColor +";\n}\n"
                 "QMenuBar::item, QMenu::separator, QMenu::item {\ncolor:" + _sTextColor + ";\n}";
    setStyleSheet(ss);

    _drawArea->SetMode(mode != smLight, _sBackgroundColor);      // set penBlack color to White
}

void MyWhiteboard::closeEvent(QCloseEvent* event)
{
    {
        if (_SaveIfYouWant())
            event->accept();
        else
            event->ignore();
    }
}

void MyWhiteboard::on_actionNew_triggered()
{
    _drawArea->NewData();
    _saveName.clear();
    _backgroundImageName.clear();
}

void MyWhiteboard::on_actionLoad_triggered()
{
    _SaveIfYouWant();
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Load Data"), 
                                                    QDir::currentPath(),
                                                    tr("MyWhiteboard files (*.mwb);;All files (*)"));
    if (!fileName.isEmpty())
    {
        _drawArea->Load(fileName);
        _SetPenKind();
        _saveName = fileName;
    }

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

void MyWhiteboard::on_actionSaveAs_triggered()
{
    QString initialPath = QDir::currentPath() + "/untitled.mwb";
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
                        initialPath, tr("MyWhiteboard Files (*.mwb);; All Files (*))"));
    if (fileName.isEmpty())
        return;
    _saveName = fileName;
    _SaveFile();
}

void MyWhiteboard::on_actionLoadBackground_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                    tr("Open Background Image"), QDir::currentPath());
    if (!fileName.isEmpty())
        _drawArea->OpenBackgroundImage(fileName);
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
        tr("<p>Based on Qt's <b>Scribble</b> example "
            "but with undo, marking and moveing areas of screen "
            "</p>"));
}

void MyWhiteboard::on_actionLightMode_triggered()
{
    _SetupMode(smLight);
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
