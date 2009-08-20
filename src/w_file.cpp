#include <QFileInfo>
#include <QScrollBar>
#include "file.h"
#include "ui_w_file.h"
#include "ui_w_main.h"
#include "w_file.h"
#include "w_piano.h"
#include "w_track.h"

WFile::WFile(File *_file, Player *_player, Ui_WMain *_main_ui)
	:file_(_file),
	player_(_player),
	main_ui_(_main_ui)
{
	ui->setupUi(this);
	update_tracks();
	ui->scroll_area->setFocusPolicy(Qt::NoFocus);
	ui->scroll_area->horizontalScrollBar()->setFocusPolicy(Qt::NoFocus);
	ui->scroll_area->verticalScrollBar()->setFocusPolicy(Qt::NoFocus);

	connect(file_, SIGNAL(acted()), this, SLOT(update_label()));
}

static vmd_time_t
cursor_time(WPiano *p)
{
	if (p == NULL)
		return 0;
	else if (p->x_time() <= p->cursor_time() && p->cursor_time() < p->r_time())
		return p->cursor_time();
	else
		return (p->x_time() + p->r_time()) / 2;
}

void
WFile::open_track(vmd_track_t *track)
{
	WPiano *prev = qobject_cast<WPiano *>(ui->scroll_area->widget());
	vmd_time_t t = cursor_time(prev);
	int x = ui->scroll_area->horizontalScrollBar()->value();

	WPiano *w = new WPiano(file_, track, t, player_);
	ui->scroll_area->setWidget(w);
	w->setFocus(Qt::OtherFocusReason);
	w->adjust_y();
	ui->scroll_area->horizontalScrollBar()->setValue(x);
}

void
WFile::update_tracks()
{
	int i;
	while (ui->tracks->count() > file()->tracks)
		ui->tracks->removeItem(ui->tracks->itemAt(ui->tracks->count() - 1));
	for (i = 0; i < ui->tracks->count(); i++)
		static_cast<WTrack *>(ui->tracks->itemAt(i)->widget())->update();
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
