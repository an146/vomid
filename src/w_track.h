/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#ifndef W_TRACK_H
#define W_TRACK_H

#include <QFrame>
#include "util.h"

class Ui_WTrack;
class WFile;

class WTrack : public QFrame
{
	Q_OBJECT

public:
	WTrack(WFile *, int);
	void update_track();

public slots:
	void open();

private:
	pimpl_ptr<Ui_WTrack> ui;
	WFile *wfile;
	int idx;
};

#endif /* W_TRACK_H */
