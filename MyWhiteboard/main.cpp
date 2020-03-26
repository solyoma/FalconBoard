#include "MyWhiteboard.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	MyWhiteboard w;
	w.show();
	return a.exec();
}
