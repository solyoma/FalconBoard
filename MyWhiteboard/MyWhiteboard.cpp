#include "MyWhiteboard.h"
#include <QSpinBox>
#include <QLabel>
#include "DrawArea.h"

MyWhiteboard::MyWhiteboard(QWidget *parent)	: QMainWindow(parent)
{
	ui.setupUi(this);
    CreateAndAddActions();
}

void MyWhiteboard::CreateAndAddActions()
{
    const QList<QByteArray> imageFormats = QImageWriter::supportedImageFormats();
    for (const QByteArray& format : imageFormats) {
        QString text = tr("%1...").arg(QString::fromLatin1(format).toUpper());

        QAction* action = new QAction(text, this);
        action->setData(format);
        connect(action, &QAction::triggered, this, &MyWhiteboard::_saveAsActs);
        _saveAsActs.append(action);
    }
    ui.mainToolBar->addAction(QIcon(":/Resources/black.png"), QString(), this, "blackPen");
    ui.mainToolBar->addAction(QIcon(":/Resources/red.png"), QString(), this, "redPen");
    ui.mainToolBar->addAction(QIcon(":/Resources/green.png"), QString(), this, "greenPen");
    ui.mainToolBar->addAction(QIcon(":/Resources/blue.png"), QString(), this, "bluePen");
    ui.mainToolBar->addSeparator();

    ui.mainToolBar->addWidget(new QLabel(tr("Pen Width:")));
    QSpinBox* psb = new QSpinBox();
    psb->setMinimum(1);
    psb->setMaximum(200);
    psb->setSingleStep(1);
    ui.mainToolBar->addWidget(psb);
}

bool MyWhiteboard::shouldSave()
{
    if (_drawArea->isModified()) {
        QMessageBox::StandardButton ret;
        ret = QMessageBox::warning(this, tr("MyWhiteboard"),
            tr("The image has been modified.\n"
                "Do you want to save your changes?"),
            QMessageBox::Save | QMessageBox::Discard
            | QMessageBox::Cancel);
        if (ret == QMessageBox::Save)
            return saveFile();
        else if (ret == QMessageBox::Cancel)
            return false;
    }
    return true;
}

bool MyWhiteboard::saveFile()
{
    if (_saveName.isEmpty())
        return false;
    return _drawArea->saveImage(_saveName, _fileFormat.constData());
}

void MyWhiteboard::closeEvent(QCloseEvent* event)
{
    {
        if (shouldSave())
            event->accept();
        else
            event->ignore();
    }
}

void MyWhiteboard::on_actionOpen_triggered()
{
    if (shouldSave()) 
    {
        QString fileName = QFileDialog::getOpenFileName(this,
                                                        tr("Open File"), QDir::currentPath());
        if (!fileName.isEmpty())
            _drawArea->openImage(fileName);
    }
}

void MyWhiteboard::on_actionSave_triggered()
{
    saveFile();
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
    _drawArea->clearImage();
}

void MyWhiteboard::ona_actionSaveAs_triggered()
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
    saveFile();
}
