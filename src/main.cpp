#include <QApplication>
#include <vomid.h>
#include "player.h"
#include "w_main.h"

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	Player player;
	WMain main_window(&player);

	main_window.show();

	vmd_file_t f;
	vmd_file_init(&f);

	return app.exec();
}
