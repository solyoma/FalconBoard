#include <QString>
#include <QStringList>
#include <QtWidgets/QApplication>

// from the application
#include "common.h"
#include "FalconBoard.h"


int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	FBSettings::Init();
	QSettings *s = FBSettings::Open();
	int ixLang = s->value("lang", -1).toInt();
	bool allowOnlyOneInstanceRunning = s->value("single", false).toBool();
	FBSettings::Close();

	QStringList fileNames = GetTranslations();	// sorted list of language string like "hu_HU"
	
	QTranslator translator;

	QString qs, qsn;
	qs = QLocale::system().name();
	if (ixLang < 0)
		ixLang = fileNames.indexOf("FalconBard_" + qs.left(2)+".qm"); // TODO: en_US or en_GB both are "en"

	if (ixLang >= 0)
	{
		qsn = ":/FalconBoard/translations/" + fileNames[ixLang];
		bool loaded = translator.load(qsn);
		if (loaded)
			qs = translator.language();

		if (loaded && qs != "en_US")	 // only set when not American English
			a.installTranslator(&translator);
	}
	// set ip window and languages
	FalconBoard w(a.screens()[0]->size());
	w.SetLanguages(fileNames, ixLang);

	if (allowOnlyOneInstanceRunning)
	{
		// prepare parameters for other instance
		QByteArray arguments;
		if (argc > 1)
		{
			for (int i = 1; i < argc; ++i)
			{
				arguments.append(argv[i]);
				arguments.append('\0');
			}
		}
		else
			arguments.append(TO_FRONT);

		// Try to connect to the named pipe
		// DEBUG
		qDebug("Trying to connect to pipe named '%s'", pipeName.toStdString().c_str());
		QLocalSocket socket;
		socket.connectToServer(pipeName, QIODevice::WriteOnly);
		if (socket.waitForConnected(1000))	// see if a server with 'pipeName' is running
		{
			// Another instance of the application is already running
			// DEBUG
			qDebug("  Another instance is running");
			// Send the command line arguments over the pipe
			// DEBUG
			qDebug("  writing '%s' to socket", arguments.constData());
			socket.write(arguments);
			socket.flush();
			bool success = socket.waitForBytesWritten(1000);
			// DEBUG
			qDebug("  write operation %s", success ? "successful" : "unsuccessful");
			return success ? 0 : -1;
		}
		else
		// DEBUG
			qDebug("Connection failed w. error '%s'",socket.errorString().toStdString().c_str());

		// Application is not yet running. Create a local server to listen for connections from other instances
		w.StartListenerThread(&a); // create new listener thread and start listeining on 'pipeName'
	}

	a.setAttribute(Qt::AA_SynthesizeMouseForUnhandledTouchEvents);
	a.setWindowIcon(QIcon(":/FalconBoard/Resources/falconboard.png"));

	// after a new translation is added add Language names into
	//  FalconBoard::_PopulateLanguageMenu()


//	FalconBoard w;
#if defined(_DEBUG)
	#ifdef _VIEWER
		w.setWindowTitle(QObject::tr("FalconBoard Viewer - Debug version"));
	#else
		w.setWindowTitle(QObject::tr("FalconBoard - Debug version"));
	#endif
#else
	#ifdef _VIEWER
		w.setWindowTitle(QObject::tr("FalconBoard Viewer"));
	#endif
#endif
	w.show();
	int res = a.exec();
	//thread.terminate();
	return res;
}
