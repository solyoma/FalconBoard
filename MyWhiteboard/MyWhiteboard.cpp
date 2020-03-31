#include "MyWhiteboard.h"
#include <QSpinBox>
#include <QLabel>
#include "DrawArea.h"

MyWhiteboard::MyWhiteboard(QWidget *parent)	: QMainWindow(parent)
{
	ui.setupUi(this);

    _actPenWidth = 3;
    _actColor = "black";
    _drawArea = static_cast<DrawArea*>(ui.centralWidget);
    _drawArea->SetPenColor(_actColor);
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
    ui.mainToolBar->addSeparator();

    // TODO add toolbar items for actions undo redo, open, save, saveas, print, erease, exit here
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
    if (_drawArea->IsModified()) {
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

void MyWhiteboard::_SetCursor(DrawArea::CursorShape cs)
{
    _drawArea->SetCursor(cs);
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

void MyWhiteboard::on_actionClearArea_triggered()
{
    _drawArea->ClearArea();
}

void MyWhiteboard::on_actionUndo_triggered()
{
    _drawArea->Undo();
    if (_eraserOn)
        on_action_Eraser_triggered();
    else
        _SetCursor(DrawArea::csPen);
}

void MyWhiteboard::on_actionRedo_triggered()
{
    _drawArea->Redo();
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
    switch (pt)
    {
        case QTabletEvent::Eraser:
            if (!_eraserOn)
                on_action_Eraser_triggered();
            break;
        default:
            if (_eraserOn)
            {
                _SetCursor(DrawArea::csPen);
                _SetCursor(DrawArea::csPen);
                _SetPenColor(_actColor);
                _SetPenWidth();
            }
            break;
    }
}
