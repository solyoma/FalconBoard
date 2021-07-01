#include <QString>
#include <QStringList>
#include <QtWidgets/QApplication>
#include "FalconBoard.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	a.setWindowIcon(QIcon(":/FalconBoard/Resources/falconboard.png"));
	QString homePath = QDir::homePath() +
#if defined (Q_OS_Linux)   || defined (Q_OS_Darwin) || defined (__linux__)
		"/.falconBoard";
#elif defined(Q_OS_WIN)
		"/Appdata/Local/FalconBoard";
#endif

	// after a new translation is added add Language names into
	//  FalconBoard::_PopulateLanguageMenu()

	QStringList fileNames = GetTranslations();	// sorted list of language string like "hu_HU"
	
	QTranslator translator;
	QSettings *s = new QSettings(homePath + 
#ifdef _VIEWER
		"/FalconBoardViewer.ini",
#else
		"/FalconBoard.ini", 
#endif
		QSettings::IniFormat);
	int ixLang = s->value("lang", -1).toInt();
	delete s;  
	QString qs, qsn;
	qs = QLocale::system().name();
	if (ixLang < 0)
		ixLang = fileNames.indexOf("FalconBard_" + qs.left(2)+".qm");

	if (ixLang >= 0)
	{
		qsn = ":/FalconBoard/translations/" + fileNames[ixLang];
		bool loaded = translator.load(qsn);
		if (loaded)
			qs = translator.language();

		if (loaded && qs != "en_US")	 // only set when not American English
			a.installTranslator(&translator);
	}

	FalconBoard w;
#ifdef _VIEWER
	w.setWindowTitle("FalconBoard Viewer");
#endif
	w.SetLanguages(fileNames, ixLang);
	w.show();
	return a.exec();
}
