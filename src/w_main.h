/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#ifndef W_MAIN_H
#define W_MAIN_H

#include <QMainWindow>
#include "util.h"

class File;
class Player;
class Ui_WMain;

class WMain : public QMainWindow
{
	Q_OBJECT

public:
	WMain(Player *);
	void open(File *);
	File *file();

public slots:
	bool close_tab(int = -1);

	void menu_new();
	void menu_open();
	void menu_save();
	void menu_saveas();

	void update_menus();

protected:
	void closeEvent(QCloseEvent *);

private:
	pimpl_ptr<Ui_WMain> ui;
	Player *player;
};

#endif /* W_MAIN_H */
