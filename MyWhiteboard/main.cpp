#include <QtWidgets/QApplication>
#include "MyWhiteboard.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	MyWhiteboard w;
	w.show();
	return a.exec();
}
