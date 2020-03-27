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
    _drawArea->setPenColor(_actColor);
    _drawArea->setPenWidth(_actPenWidth); 

    _CreateAndAddActions();
}

void MyWhiteboard::_CreateAndAddActions()
{
    const QList<QByteArray> imageFormats = QImageWriter::supportedImageFormats();
    for (const QByteArray& format : imageFormats) {
        QString text = tr("%1...").arg(QString::fromLatin1(format).toUpper());

        QAction* action = new QAction(text, this);
        action->setData(format);
        connect(action, &QAction::triggered, this, &MyWhiteboard::on_actionSaveAs_triggered);
        _saveAsActs.append(action);
    }

    ui.mainToolBar->addAction(ui.actionExit);
    ui.mainToolBar->addSeparator();
    ui.mainToolBar->addAction(ui.actionOpen);
    ui.mainToolBar->addAction(ui.actionSave);
    ui.mainToolBar->addAction(ui.actionSaveAs);
    ui.mainToolBar->addSeparator();
    ui.mainToolBar->addAction(ui.action_Print);
    ui.mainToolBar->addSeparator();

    ui.mainToolBar->addAction(ui.action_Black);
    ui.mainToolBar->addAction(ui.action_Red);
    ui.mainToolBar->addAction(ui.action_Green);
    ui.mainToolBar->addAction(ui.action_Blue);
    ui.mainToolBar->addAction(ui.action_Ereaser);
    ui.mainToolBar->addSeparator();

    // TODO add toolbar items for actions undo redo, open, save, saveas, print, erease, exit here
    ui.mainToolBar->addSeparator();

    ui.mainToolBar->addWidget(new QLabel(tr("Pen Width:")));
    _psbPenWidth = new QSpinBox();
    _psbPenWidth->setMinimum(1);
    _psbPenWidth->setMaximum(200);
    _psbPenWidth->setSingleStep(1);
    _psbPenWidth->setValue(_actPenWidth);

    ui.mainToolBar->addWidget(_psbPenWidth);
        // more than one valeChanged() function exists
    connect(_psbPenWidth, QOverload<int>::of(&QSpinBox::valueChanged), this, &MyWhiteboard::slotPenWidthChanged);

    connect(_drawArea, &DrawArea::canUndo, this, &MyWhiteboard::slotForUndo);
    connect(_drawArea, &DrawArea::canRedo, this, &MyWhiteboard::slotForRedo);

    connect(ui.actionClearArea, &QAction::triggered, _drawArea, &DrawArea::clearHistory);
}

bool MyWhiteboard::_ShouldSave()
{
    if (_drawArea->isModified()) {
        QMessageBox::StandardButton ret;
        ret = QMessageBox::warning(this, tr("MyWhiteboard"),
            tr("The image has been modified.\n"
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
    if (_saveName.isEmpty())
        return false;
    return _drawArea->saveImage(_saveName, _fileFormat.constData());
}

void MyWhiteboard::closeEvent(QCloseEvent* event)
{
    {
        if (_ShouldSave())
            event->accept();
        else
            event->ignore();
    }
}

void MyWhiteboard::on_actionOpen_triggered()
{
    if (_ShouldSave()) 
    {
        QString fileName = QFileDialog::getOpenFileName(this,
                                                        tr("Open File"), QDir::currentPath());
        if (!fileName.isEmpty())
            _drawArea->openImage(fileName);
    }
}

void MyWhiteboard::on_actionSave_triggered()
{
    _SaveFile();
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

void MyWhiteboard::on_actionSaveAs_triggered()
{
    QAction* action = qobject_cast<QAction*>(sender());
    _fileFormat = action->data().toByteArray();

    QString initialPath = QDir::currentPath() + "/untitled." + _fileFormat;

    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
        initialPath,
        tr("%1 Files (*.%2);;All Files (*)")
        .arg(QString::fromLatin1(_fileFormat.toUpper()))
        .arg(QString::fromLatin1(_fileFormat)));
    if (fileName.isEmpty())
        return ;
    _saveName = fileName;
    _SaveFile();
}

void MyWhiteboard::on_actionUndo_triggered()
{
    _drawArea->Undo();
}

void MyWhiteboard::on_actionRedo_triggered()
{
    _drawArea->Redo();
}

void MyWhiteboard::slotForUndo(bool b)
{
    ui.actionUndo->setEnabled(b);
}

void MyWhiteboard::slotForRedo(bool b)
{
    ui.actionRedo->setEnabled(b);
}
