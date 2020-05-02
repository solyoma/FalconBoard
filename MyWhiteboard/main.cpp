#include <QtWidgets/QApplication>
#include "MyWhiteboard.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	MyWhiteboard w;
#ifdef _VIEWER
	w.setWindowTitle("MyWhiteboard Viewer");
#endif
	w.show();
	return a.exec();
}
