#include <QCloseEvent>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include "file.h"
#include "slot_proxy.h"
#include "ui_w_main.h"
#include "w_main.h"
#include "w_file.h"

#define ACTION(p, a, s) p->connect(ui->action##a, SIGNAL(triggered()), SLOT(s))
#define FILE_ACTION(a, s)  ACTION(file_proxy, a, s)
#define WFILE_ACTION(a, s) ACTION(wfile_proxy, a, s)
#define STDICON(a, i) ui->action##a->setIcon(QApplication::style()->standardIcon(QStyle::i))
#define STDSHORTCUT(a) ui->action##a->setShortcut(QKeySequence::a)

WMain::WMain(Player *_player)
	:file_proxy(),
	wfile_proxy(),
	player(_player)
{
	ui->setupUi(this);

	FILE_ACTION(Undo, undo());
	FILE_ACTION(Redo, redo());
	WFILE_ACTION(TrkStandard, addStandard());
	WFILE_ACTION(TrkDrums, addDrums());
	WFILE_ACTION(TrkTet, addTet());
	WFILE_ACTION(TrkScala, addScala());

	STDICON(New,     SP_FileIcon);
	STDICON(Open,    SP_DialogOpenButton);
	STDICON(SaveAs,  SP_DialogSaveButton);
	STDICON(Undo,    SP_ArrowBack);
	STDICON(Redo,    SP_ArrowForward);
	STDSHORTCUT(New);
	STDSHORTCUT(Open);
	STDSHORTCUT(Save);
	STDSHORTCUT(SaveAs);
	STDSHORTCUT(Close);
	STDSHORTCUT(Undo);
	STDSHORTCUT(Redo);

	current_changed();
}

void
WMain::open(File *f)
{
	WFile *wfile = new WFile(f, player, ui);
	ui->tabs->setCurrentIndex(ui->tabs->addTab(wfile, ""));
	wfile->update_label();
	connect(f, SIGNAL(acted()), this, SLOT(current_changed()));
}

File *
WMain::file()
{
	WFile *w = wfile();
	return w ? w->file() : NULL;
}

WFile *
WMain::wfile()
{
	return qobject_cast<WFile *>(ui->tabs->currentWidget());
}

void
WMain::menu_new()
{
	open(new File());
}

void
WMain::menu_open()
{
	QStringList files = QFileDialog::getOpenFileNames(
		this,
		QString(),
		QString(),
		"Midi files (*.mid);;All files (*)"
	);

	for (QStringList::const_iterator i = files.begin(); i != files.end(); ++i) {
		try {
			open(new File(*i));
		} catch (...) {
			qWarning("Failed to open file");
		}
	}
}

void
WMain::menu_save()
{
	File *f = file();
	if (f == NULL)
		return;
	else if (f->filename().isEmpty())
		menu_saveas();
	else
		f->save_as(f->filename());
}

void
WMain::menu_saveas()
{
	File *f = file();
	if (f == NULL)
		return;

	QFileDialog fd(
		this,
		QString(),
		QString(),
		"Midi files (*.mid);;All files (*)"
	);
	fd.setAcceptMode(QFileDialog::AcceptSave);
	fd.setDefaultSuffix("mid");
	if (fd.exec())
		f->save_as(fd.selectedFiles().first());
}

bool
WMain::close_tab(int idx)
{
	if (idx < 0) {
		idx = ui->tabs->currentIndex();
		if (idx < 0)
			return false;
	}
	ui->tabs->setCurrentIndex(idx);
	File *f = file();
	bool close = true;
	if (!f->saved()) {
		int btn = QMessageBox::question(
			this,
			"vomid",
			"Save this file?",
			QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
			QMessageBox::Save
		);
		switch (btn) {
		case QMessageBox::Save:
			menu_save();
			close = f->saved();
			break;
		case QMessageBox::Cancel:
			close = false;
			break;
		default:
			close = true;
			break;
		}
	}
	if (close)
		ui->tabs->removeTab(idx);
	return close;
}

void
WMain::current_changed()
{
	File *f = file();
	file_proxy->setTarget(f);
	wfile_proxy->setTarget(wfile());

	/* File */
	ui->actionSave->setEnabled(f && !f->saved());
	ui->actionSaveAs->setEnabled(f != NULL);
	ui->actionClose->setEnabled(f != NULL);

	/* Edit */
	ui->menuEdit->setEnabled(f != NULL);
	if (f && f->revision()->prev() != NULL) {
		ui->actionUndo->setText("Undo " + f->revision()->name());
		ui->actionUndo->setEnabled(true);
	} else {
		ui->actionUndo->setText("Undo");
		ui->actionUndo->setEnabled(false);
	}
	if (f && f->revision()->next() != NULL) {
		ui->actionRedo->setText("Redo " + f->revision()->next()->name());
		ui->actionRedo->setEnabled(true);
	} else {
		ui->actionRedo->setText("Redo");
		ui->actionRedo->setEnabled(false);
	}

	/* Track */
	ui->menuTrack->setEnabled(f != NULL);
}

void
WMain::closeEvent(QCloseEvent *ev)
{
	while (ui->tabs->count() > 0) {
		if (!close_tab(0)) {
			ev->ignore();
			return;
		}
	}
	ev->accept();
}
