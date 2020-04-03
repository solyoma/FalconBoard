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

    _actPenWidth = 3;
    _actPen = penBlack;
    _drawArea = static_cast<DrawArea*>(ui.centralWidget);
    _drawArea->SetPenColor(_actPen);
    _drawArea->SetPenWidth(_actPenWidth); 

    _CreateAndAddActions();
    _AddSaveAsVisibleMenu();

    QCoreApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents); // for tablet

    connect(_drawArea, &DrawArea::PointerTypeChange, this, &MyWhiteboard::SlotForPointerType);
    ui.centralWidget->setFocus();
}

void MyWhiteboard::_CreateAndAddActions()
{
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

    ui.mainToolBar->addSeparator();


    ui.mainToolBar->addWidget(new QLabel(tr("Pen Width:")));
    _psbPenWidth = new QSpinBox();
    _psbPenWidth->setMinimum(1);
    _psbPenWidth->setMaximum(200);
    _psbPenWidth->setSingleStep(1);
    _psbPenWidth->setValue(_actPenWidth);
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

void MyWhiteboard::SelectPen(QAction* paction)
{
    paction->setChecked(true);
}

void MyWhiteboard::SelectPen(MyPenKind color)
{
    if (color == penEraser)
    {
        on_action_Eraser_triggered();
        return;
    }

    switch (_actPen)
    {
    default:
    case penBlack: ui.action_Black->setChecked(true); break;
    case penRed: ui.action_Red->setChecked(true); break;
    case penGreen: ui.action_Green->setChecked(true); break;
    case penBlue: ui.action_Blue->setChecked(true); break;
    }
    _SetPenColor();
}

void MyWhiteboard::_SetPenColor()
{
    _eraserOn = _actPen == penEraser;
    _drawArea->SetPenColor(_actPen);
    _busy = true;
    _psbPenWidth->setValue(_actPenWidth);
    _busy = false;
    ui.centralWidget->setFocus();
}

void MyWhiteboard::_SetPenColor(MyPenKind color)
{
    _actPen = color;
    _SetPenColor();
}


void MyWhiteboard::_SetCursor(DrawArea::CursorShape cs)
{
    _drawArea->SetCursor(cs);
}

void MyWhiteboard::_SetBlackPen() { _SetPenColor(penBlack);  SelectPen(ui.action_Black); }
void MyWhiteboard::_SetRedPen() { _SetPenColor  (penRed);    SelectPen(ui.action_Red); }
void MyWhiteboard::_SetGreenPen() { _SetPenColor(penGreen);  SelectPen(ui.action_Green); }
void MyWhiteboard::_SetBluePen() { _SetPenColor (penBlue);   SelectPen(ui.action_Blue); }

void MyWhiteboard::_SetPenWidth()
{
    int pw = _eraserOn ? _eraserWidth : _actPenWidth;
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
        _SetPenColor();
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

void MyWhiteboard::on_action_Eraser_triggered()
{
    _eraserOn = true;
    _drawArea->SetPenColor(_actPen = penEraser);
    _drawArea->SetCursor(DrawArea::csEraser);
    _SetPenWidth();
    SelectPen(ui.action_Eraser);
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
    SelectPen(_actPen);
    if (_eraserOn)
        on_action_Eraser_triggered();
    else
        _SetCursor(DrawArea::csPen);

}

void MyWhiteboard::on_actionRedo_triggered()
{
    _drawArea->Redo();
    SelectPen(_actPen);
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

void MyWhiteboard::SlotForPointerType(QTabletEvent::PointerType pt)
{
    static bool penEraser = false;     // set to true if not erasemode just
    switch (pt)                         // eraser end of stylus used
    {
        case QTabletEvent::Eraser:
            if (!_eraserOn)
            {
                penEraser = true;
                on_action_Eraser_triggered();
            }
            break;
        default:
            if (penEraser)
            {
                _SetCursor(DrawArea::csPen);
                _SetPenColor(_actPen);
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
