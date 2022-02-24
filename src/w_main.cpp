#include <QActionGroup>
#include <QCloseEvent>
#include <QFileInfo>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QSignalMapper>
#include <QStyle>
#include "file.h"
#include "player.h"
#include "slot_proxy.h"
#include "ui_w_main.h"
#include "w_main.h"
#include "w_file.h"

#define ACTION(p, a, s) p.connect(pimpl->ui.action##a, SIGNAL(triggered()), SLOT(s))
#define FILE_ACTION(a, s)  ACTION(pimpl->file_proxy, a, s)
#define WFILE_ACTION(a, s) ACTION(pimpl->wfile_proxy, a, s)
#define STDICON(a, i) pimpl->ui.action##a->setIcon(QApplication::style()->standardIcon(QStyle::i))
#define STDSHORTCUT(a) pimpl->ui.action##a->setShortcut(QKeySequence::a)

struct WMain::Impl
{
	Ui_WMain ui;
	SlotProxy file_proxy, wfile_proxy;
	QActionGroup output_devices;
	QSignalMapper device_mapper;
	Player *player;

	Impl(WMain *owner) : output_devices(owner) { }
};

static void
enum_clb(const char *id, const char *name, void *me)
{
	((WMain *)me)->add_output_device(id, name);
}

WMain::WMain(Player *_player)
	: pimpl(this)
{
	pimpl->ui.setupUi(this);
	pimpl->ui.statusbar->addPermanentWidget(pimpl->ui.file_compatible);
	pimpl->player = _player;

	FILE_ACTION(Undo, undo());
	FILE_ACTION(Redo, redo());
	WFILE_ACTION(Info, showInfo());
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
	connect(&pimpl->device_mapper, SIGNAL(mapped(QString)), pimpl->player, SLOT(set_output_device(QString)));
	connect(pimpl->player, SIGNAL(outputDeviceSet(QString)), this, SLOT(output_device_set(QString)));
	vmd_enum_devices(VMD_OUTPUT_DEVICE, enum_clb, this);
}

void
WMain::add_output_device(const char *id, const char *name)
{
	QString label = QString(id) + QString("\t") + QString::fromLocal8Bit(name);
	QAction *act = pimpl->ui.menuOutputDevices->addAction(label);
	act->setData(QString(id));
	pimpl->output_devices.addAction(act);
	connect(act, SIGNAL(triggered()), &pimpl->device_mapper, SLOT(map()));
	pimpl->device_mapper.setMapping(act, QString(id));

	static bool set = false;
	if (!set && pimpl->player->set_output_device(id))
		set = true;
}

void
WMain::open(File *f)
{
	WFile *wfile = new WFile(f, pimpl->player, &pimpl->ui);
	pimpl->ui.tabs->setCurrentIndex(pimpl->ui.tabs->addTab(wfile, ""));
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
	return qobject_cast<WFile *>(pimpl->ui.tabs->currentWidget());
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

	foreach (const QString &i, files) {
		try {
			open(new File(i));
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
		idx = pimpl->ui.tabs->currentIndex();
		if (idx < 0)
			return false;
	}
	pimpl->ui.tabs->setCurrentIndex(idx);
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
		pimpl->ui.tabs->removeTab(idx);
	return close;
}

void
WMain::output_device_set(QString id)
{
	foreach (QAction *act, pimpl->ui.menuOutputDevices->findChildren<QAction *>()) {
		act->setCheckable(false);
		act->setChecked(false);
		if (act->data() == id) {
			act->setCheckable(true);
			act->setChecked(true);
		}
	}
}

void
WMain::current_changed()
{
	File *f = file();
	pimpl->file_proxy.setTarget(f);
	pimpl->wfile_proxy.setTarget(wfile());

	/* File */
	pimpl->ui.actionSave->setEnabled(f && !f->saved());
	pimpl->ui.actionSaveAs->setEnabled(f != NULL);
	pimpl->ui.actionClose->setEnabled(f != NULL);
	pimpl->ui.actionInfo->setEnabled(f != NULL);

	/* Edit */
	pimpl->ui.menuEdit->setEnabled(f != NULL);
	if (f && f->revision()->prev() != NULL) {
		pimpl->ui.actionUndo->setText("Undo " + f->revision()->descr());
		pimpl->ui.actionUndo->setEnabled(true);
	} else {
		pimpl->ui.actionUndo->setText("Undo");
		pimpl->ui.actionUndo->setEnabled(false);
	}
	if (f && f->revision()->next() != NULL) {
		pimpl->ui.actionRedo->setText("Redo " + f->revision()->next()->descr());
		pimpl->ui.actionRedo->setEnabled(true);
	} else {
		pimpl->ui.actionRedo->setText("Redo");
		pimpl->ui.actionRedo->setEnabled(false);
	}

	/* Track */
	pimpl->ui.menuTrack->setEnabled(f != NULL);

	/* Status Bar */
	QString file_status;
	if (f == NULL)
		file_status = "No file";
	else if (vmd_file_is_compatible(f))
		file_status = "Compatible";
	else
		file_status = "Non-compatible";
	pimpl->ui.file_compatible->setText(file_status);
}

void
WMain::closeEvent(QCloseEvent *ev)
{
	while (pimpl->ui.tabs->count() > 0) {
		if (!close_tab(0)) {
			ev->ignore();
			return;
		}
	}
	ev->accept();
}
