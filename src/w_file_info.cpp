#include <vomid.h>
#include "file.h"
#include "ui_w_file_info.h"
#include "w_file.h"
#include "w_file_info.h"
#include "w_piano.h"

enum {
	TYPE_TRACK = QTreeWidgetItem::UserType,
	TYPE_CHANNEL = QTreeWidgetItem::UserType + 1
};

struct TrackItem : public QTreeWidgetItem
{
	TrackItem(QTreeWidget *tree) : QTreeWidgetItem(tree, TYPE_TRACK) { }

	vmd_track_t *track;
};

struct ChannelItem : public QTreeWidgetItem
{
	ChannelItem(TrackItem *ti) : QTreeWidgetItem(ti, TYPE_CHANNEL), track(ti->track) { }

	vmd_track_t *track;
	vmd_channel_t *channel;
};

WFileInfo::WFileInfo(WFile *_wfile)
	: QDialog(NULL),
	wfile_(_wfile)
{
	ui->setupUi(this);
	ui->fileName->setText(file()->filename());

	for (int i = 0; i < file()->tracks; i++) {
		vmd_track_t *track = file()->track[i];
		TrackItem *ti = new TrackItem(ui->channelsTree);
		ti->track = track;

		int channels = 0;
		for (int j = 0; j < VMD_CHANNELS; j++) {
			if (track->channel_usage[j] > 0) {
				ChannelItem *ci = new ChannelItem(ti);
				ci->channel = &file()->channel[j];
				ci->setText(0, "Channel " + QString::number(j));
				ci->setText(1, QString::number(track->channel_usage[j]));
				channels++;
			}
		}

		QString track_text = "Track " + QString::number(i + 1);
		if (channels == 0)
			track_text += " (empty)";
		else if (channels >= 2)
			track_text += " (" + QString::number(channels) + " channels)";
		ti->setText(0, track_text);
		ti->setText(1, QString::number(vmd_bst_size(&track->notes)));
	}
	ui->channelsTree->header()->setResizeMode(QHeaderView::ResizeToContents);
	connect(ui->channelsTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)),
	        this, SLOT(channelsTreeItemDoubleClicked(QTreeWidgetItem *, int)));
}

File *WFileInfo::file() const
{
	return wfile_->file();
}

void WFileInfo::channelsTreeItemDoubleClicked(QTreeWidgetItem *item, int)
{
	if (item->type() == TYPE_TRACK) {
		TrackItem *ti = static_cast<TrackItem *>(item);
		WPiano *piano = wfile_->open_track(ti->track);
		piano->setSelection(NULL);
	} else if (item->type() == TYPE_CHANNEL) {
		ChannelItem *ci = static_cast<ChannelItem *>(item);
		WPiano *piano = wfile_->open_track(ci->track);
		vmd_note_t *sel = NULL;
		VMD_BST_FOREACH(vmd_bst_node_t *i, &ci->track->notes) {
			vmd_note_t *n = vmd_track_note(i);
			if (n->channel != ci->channel)
				continue;
			n->next = sel;
			sel = n;
		}
		piano->setSelection(sel);
	}
}
