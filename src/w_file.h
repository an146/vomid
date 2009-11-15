/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#ifndef W_FILE_H
#define W_FILE_H

#include <QFrame>
#include "util.h"

struct vmd_track_t;
class File;
class Player;
class Ui_WFile;
class Ui_WMain;
class WPiano;

class WFile : public QFrame
{
	Q_OBJECT

public:
	WFile(File *, Player *, Ui_WMain *);

	File *file() const { return file_; }
	WPiano *piano() const;
	vmd_track_t *track() const;

	void open_track(vmd_track_t *);

public slots:
	void update_label();
	void update_tracks();

	void addStandard();
	void addDrums();
	void addTet();
	void addScala();

private:
	File *file_;
	Player *player_;

	pimpl_ptr<Ui_WFile> ui;
	Ui_WMain *main_ui_;
};

#endif /* W_FILE_H */
