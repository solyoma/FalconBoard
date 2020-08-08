#include <QtWidgets/QApplication>
#include "FalconBoard.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	FalconBoard w;
#ifdef _VIEWER
	w.setWindowTitle("FalconBoard Viewer");
#endif
	w.show();
	return a.exec();
}
