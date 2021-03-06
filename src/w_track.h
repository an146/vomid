/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#ifndef W_TRACK_H
#define W_TRACK_H

#include <QFrame>
#include "util.h"

class QAction;
class QMenu;
class Ui_WTrack;
class WFile;

class WTrack : public QFrame
{
	Q_OBJECT

public:
	WTrack(WFile *, int);
	void update_track();

public slots:
	void open(bool);
	void program_chosen(QAction *);
	void volume_set(int);

private:
	pimpl_ptr<Ui_WTrack> ui;
	WFile *wfile;
	int idx;
	static QMenu *program_menu;
};

#endif /* W_TRACK_H */
