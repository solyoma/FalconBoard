#include <QString>
#include <QStringList>
#include <QtWidgets/QApplication>
#include "common.h"
#include "FalconBoard.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	a.setWindowIcon(QIcon(":/FalconBoard/Resources/falconboard.png"));

	FalconBoard::screenSize = a.screens()[0]->size();
	FBSettings::Init();
	// after a new translation is added add Language names into
	//  FalconBoard::_PopulateLanguageMenu()

	QStringList fileNames = GetTranslations();	// sorted list of language string like "hu_HU"
	
	QTranslator translator;

	QSettings *s = FBSettings::Open();
	int ixLang = s->value("lang", -1).toInt();
	FBSettings::Close();

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

	FalconBoard w;
#ifdef _VIEWER
	w.setWindowTitle(QObject::tr("FalconBoard Viewer"));
#endif
	w.SetLanguages(fileNames, ixLang);
	w.show();
	return a.exec();
}
