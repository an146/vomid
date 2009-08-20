#include <QCloseEvent>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include "file.h"
#include "ui_w_main.h"
#include "w_main.h"
#include "w_file.h"

#define STDICON(a, i) ui->action##a->setIcon(QApplication::style()->standardIcon(QStyle::i))
#define STDSHORTCUT(a) ui->action##a->setShortcut(QKeySequence::a)

WMain::WMain(Player *_player)
	:player(_player)
{
	ui->setupUi(this);
	connect(ui->tabs, SIGNAL(currentChanged(int)), this, SLOT(update_menus()));
	update_menus();
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
}

void
WMain::open(File *f)
{
	WFile *wfile = new WFile(f, player, ui);
	ui->tabs->setCurrentIndex(ui->tabs->addTab(wfile, ""));
	wfile->update_label();
	connect(f, SIGNAL(acted()), this, SLOT(update_menus()));
}

File *
WMain::file()
{
	WFile *wfile = qobject_cast<WFile *>(ui->tabs->currentWidget());
	return wfile ? wfile->file() : NULL;
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

	for (QStringList::const_iterator i = files.begin(); i != files.end(); ++i)
		open(new File(*i));
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

	QString fn = QFileDialog::getSaveFileName(
		this,
		QString(),
		QString(),
		"Midi files (*.mid);;All files (*)"
	);

	if (!fn.isEmpty())
		f->save_as(fn);
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
WMain::update_menus()
{
	File *f = file();

	/* File */
	ui->actionSave->setEnabled(f && !f->saved());
	ui->actionSaveAs->setEnabled(f != NULL);
	ui->actionClose->setEnabled(f != NULL);

	/* Edit */
	ui->menuEdit->setEnabled(f != NULL);
	ui->actionUndo->disconnect();
	if (f && f->revision()->prev() != NULL) {
		ui->actionUndo->setText("Undo " + f->revision()->name());
		ui->actionUndo->setEnabled(true);
		connect(ui->actionUndo, SIGNAL(triggered()), f, SLOT(undo()));
	} else {
		ui->actionUndo->setText("Undo");
		ui->actionUndo->setEnabled(false);
	}
	ui->actionRedo->disconnect();
	if (f && f->revision()->next() != NULL) {
		ui->actionRedo->setText("Redo " + f->revision()->next()->name());
		ui->actionRedo->setEnabled(true);
		connect(ui->actionRedo, SIGNAL(triggered()), f, SLOT(redo()));
	} else {
		ui->actionRedo->setText("Redo");
		ui->actionRedo->setEnabled(false);
	}
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
