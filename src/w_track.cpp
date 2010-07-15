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
	if (program_menu == NULL) {
		program_menu = new QMenu();
		for (int i = 0; i < VMD_PROGRAMS; i++) {
			QAction *act = program_menu->addAction(vmd_gm_program_name[i]);
			act->setData(i);
		}
	}
	update_track();
	connect(ui->program, SIGNAL(triggered(QAction *)), this, SLOT(program_chosen(QAction *)));
	connect(ui->volume, SIGNAL(valueChanged(int)), this, SLOT(volume_set(int)));
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
		ui->program->setMenu(NULL);
		ui->program->setEnabled(false);
	} else {
		ui->program->setText(vmd_gm_program_name[vmd_track_get_ctrl(track, VMD_CCTRL_PROGRAM)]);
		ui->program->setMenu(program_menu);
		ui->program->setEnabled(true);
	}
	ui->volume->blockSignals(true);
	ui->volume->setValue(vmd_track_get_ctrl(track, VMD_CCTRL_VOLUME));
	ui->volume->update();
	ui->volume->blockSignals(false);
}

void
WTrack::open(bool on)
{
	wfile->open_track(on ? wfile->file()->track[idx] : NULL);
}

void
WTrack::program_chosen(QAction *act)
{
	vmd_track_set_ctrl(wfile->file()->track[idx], VMD_CCTRL_PROGRAM, act->data().toInt());
	wfile->file()->commit("Set Program");
}

void
WTrack::volume_set(int v)
{
	vmd_track_set_ctrl(wfile->file()->track[idx], VMD_CCTRL_VOLUME, v);
	wfile->file()->commit("Set Volume");
}
