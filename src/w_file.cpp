#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QScrollBar>
#include "file.h"
#include "ui_w_file.h"
#include "ui_w_main.h"
#include "w_file.h"
#include "w_file_info.h"
#include "w_piano.h"
#include "w_track.h"

WFile::WFile(File *_file, Player *_player, Ui_WMain *_main_ui)
	:file_(_file),
	player_(_player),
	main_ui_(_main_ui)
{
	file_->setParent(this);
	ui->setupUi(this);
	ui->scroll_area->setFocusPolicy(Qt::NoFocus);
	ui->scroll_area->horizontalScrollBar()->setFocusPolicy(Qt::NoFocus);
	ui->scroll_area->verticalScrollBar()->setFocusPolicy(Qt::NoFocus);

	connect(file_, SIGNAL(acted()), this, SLOT(update_label()));
	connect(file_, SIGNAL(acted()), this, SLOT(update_tracks()));
	update_tracks();
}

WPiano *
WFile::piano() const
{
	return qobject_cast<WPiano *>(ui->scroll_area->widget());
}

vmd_track_t *
WFile::track() const
{
	WPiano *p = piano();
	return p ? p->track() : NULL;
}

WPiano *
WFile::open_track(vmd_track_t *track)
{
	WPiano *prev = piano();
	if (prev != NULL && prev->track() == track)
		return prev;

	vmd_time_t t = prev ? prev->cursorTime() : 0;
	int x = ui->scroll_area->horizontalScrollBar()->value();

	WPiano *piano = track ? new WPiano(file_, track, t, player_) : NULL;
	ui->scroll_area->setWidget(piano);
	if (piano != NULL) {
		piano->setFocus(Qt::OtherFocusReason);
		piano->adjust_y();
	}
	ui->scroll_area->horizontalScrollBar()->setValue(x);
	update_tracks();
	return piano;
}

void
WFile::update_tracks()
{
	int i;
	if (vmd_track_idx(track()) < 0) {
		// WTF: doesn't work
		// ui->scroll_area->setWidget(NULL);
		delete piano();
	}
	while (ui->tracks->count() > file()->tracks) {
		QLayoutItem *item = ui->tracks->itemAt(ui->tracks->count() - 1);
		ui->tracks->removeItem(item);
		delete item->widget();
	}
	for (i = 0; i < ui->tracks->count(); i++)
		static_cast<WTrack *>(ui->tracks->itemAt(i)->widget())->update_track();
	for (i = ui->tracks->count(); i < file()->tracks; i++)
		ui->tracks->addWidget(new WTrack(this, i));
}

void
WFile::update_label()
{
	QTabWidget *tabs = main_ui_->tabs;
	QString label = QFileInfo(file()->filename()).fileName();
	if (label.isEmpty())
		label = "Untitled";
	if (!file()->saved())
		label = "* " + label;
	tabs->setTabText(tabs->indexOf(this), label);
}

void
WFile::addStandard()
{
	file()->add_track();
}

void
WFile::addDrums()
{
	file()->add_track(VMD_CHANMASK_DRUMS);
}

void
WFile::addTet()
{
	bool ok;
	int n = QInputDialog::getInt(this, "vomid", "Enter scale base:", 17, 2, 100, 1, &ok);
	if (ok) {
		vmd_track_t *track = file()->add_track();
		vmd_track_set_notesystem(track, vmd_notesystem_tet(n));
	}
}

void
WFile::addScala()
{
	QString fn = QFileDialog::getOpenFileName(
		this,
		QString(),
		QString(),
		"Scala files (*.scl);;All files (*)"
	);

	vmd_notesystem_t ns = vmd_notesystem_import(fn.toLocal8Bit().data());
	if (ns.pitches != NULL) {
		vmd_track_t *track = file()->add_track();
		vmd_track_set_notesystem(track, ns);
	}
}

void
WFile::showInfo()
{
	WFileInfo *info = new WFileInfo(this);
	info->show();
}
