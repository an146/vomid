#include <QPushButton>
#include <QScrollBar>
#include "file.h"
#include "ui_w_track.h"
#include "w_file.h"
#include "w_track.h"

WTrack::WTrack(WFile *_wfile, int _idx)
	:wfile(_wfile), idx(_idx)
{
	ui->setupUi(this);
	update_track();
}

void
WTrack::update_track()
{
	vmd_track_t *track = wfile->file()->track[idx];

	ui->n->setText(QString::number(idx + 1));
	ui->name->setText(track->name);
	if (vmd_track_is_drums(track)) {
		ui->instr->setText("DRUMS");
		ui->instr->setEnabled(false);
	} else {
		ui->instr->setText(vmd_gm_instr_name[vmd_track_get_program(track)]);
		ui->instr->setEnabled(true);
	}
	ui->volume->setValue(track->value[VMD_TVALUE_VOLUME]);
}

void
WTrack::open()
{
	wfile->open_track(wfile->file()->track[idx]);
}
