/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#ifndef W_MAIN_H
#define W_MAIN_H

#include <QMainWindow>
#include "util.h"

class File;
class Player;
class WFile;

class WMain : public QMainWindow
{
	Q_OBJECT

public:
	WMain(Player *);
	void add_output_device(const char *id, const char *name);
	void open(File *);
	File *file();
	WFile *wfile();

public slots:
	bool close_tab(int = -1);
	void output_device_set(QString);

	void menu_new();
	void menu_open();
	void menu_save();
	void menu_saveas();
	void current_changed();

protected:
	void closeEvent(QCloseEvent *);

private:
	struct Impl;
	pimpl_ptr<Impl> pimpl;
};

#endif /* W_MAIN_H */
