#include "file.h"
#include "ui_w_file_info.h"
#include "w_file.h"
#include "w_file_info.h"

WFileInfo::WFileInfo(WFile *_wfile)
	: QDialog(NULL),
	wfile_(_wfile)
{
	ui->setupUi(this);
	ui->fileName->setText(file()->filename());

	for (int i = 0; i < file()->tracks; i++) {
		vmd_track_t *track = file()->track[i];
		QTreeWidgetItem *ti = new QTreeWidgetItem(ui->channelsTree);
		int channels = 0;
		for (int j = 0; j < VMD_CHANNELS; j++) {
			if (track->channel_usage[j] > 0) {
				QTreeWidgetItem *ci = new QTreeWidgetItem(ti);
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
}

File *WFileInfo::file() const
{
	return wfile_->file();
}
