#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include "file.h"
#include "ui_w_track.h"
#include "w_file.h"
#include "w_track.h"

QMenu *WTrack::program_menu = NULL;

WTrack::WTrack(WFile *_wfile, int _idx)
	:wfile(_wfile), idx(_idx)
{
	ui->setupUi(this);
	update_track();
	if (program_menu == NULL) {
		program_menu = new QMenu();
		for (int i = 0; i < VMD_PROGRAMS; i++) {
			QAction *act = program_menu->addAction(vmd_gm_program_name[i]);
			act->setData(i);
		}
	}
	ui->program->setMenu(program_menu);
	connect(ui->program, SIGNAL(triggered(QAction *)), this, SLOT(program_chosen(QAction *)));
}

void
WTrack::update_track()
{
	vmd_track_t *track = wfile->file()->track[idx];

	ui->n->setChecked(wfile->track() == track);
	ui->n->setText(QString::number(idx + 1));
	ui->name->setText(track->name);
	if (track->chanmask == VMD_CHANMASK_DRUMS) {
		ui->program->setText("DRUMS");
		ui->program->setEnabled(false);
	} else {
		ui->program->setText(vmd_gm_program_name[vmd_track_get_program(track)]);
		ui->program->setEnabled(true);
	}
	ui->volume->setValue(track->value[VMD_TVALUE_VOLUME]);
}

void
WTrack::open(bool on)
{
	wfile->open_track(on ? wfile->file()->track[idx] : NULL);
}

void
WTrack::program_chosen(QAction *act)
{
	vmd_track_set_program(wfile->file()->track[idx], act->data().toInt());
	update_track();
}
