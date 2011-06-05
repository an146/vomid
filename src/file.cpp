#include <stdexcept>
#include "file.h"

File::File()
	:filename_(),
	revision_(NULL),
	saved_revision_(NULL)
{
	vmd_file_init(this);
	revision_ = new FileRevision(this, "");
}

File::File(QString fn)
	:filename_(fn),
	revision_(NULL),
	saved_revision_(NULL)
{
	vmd_bool_t native;
	if (vmd_file_import(this, fn.toLocal8Bit().data(), &native) != VMD_OK)
		throw std::runtime_error("Invalid file");
	revision_ = new FileRevision(this, "");
	if (native)
		saved_revision_ = revision_;
}

File::~File()
{
	while (revision_->prev() != NULL)
		revision_ = revision_->prev();
	delete revision_;
	vmd_file_fini(this);
}

void
File::save_as(QString fn)
{
	vmd_file_export(this, fn.toLocal8Bit().data());
	filename_ = fn;
	saved_revision_ = revision_;
	emit acted();
}

void
File::commit(QString descr)
{
	FileRevision *newrev;

	try {
		newrev = new FileRevision(this, descr);
	} catch (const std::exception &ex) {
		revert();
		qWarning("%s: failed", descr.toAscii().data());
		return;
	}

	delete revision_->next_;
	revision_->next_ = newrev;
	revision_ = newrev;
	emit acted();
}

void
File::update(FileRevision *rev)
{
	vmd_file_update(this, rev->rev_);
	revision_ = rev;
	emit acted();
}

void
File::revert()
{
	update(revision_);
}

vmd_track_t *
File::add_track(vmd_chanmask_t cm)
{
	if (tracks == VMD_MAX_TRACKS - 1)
		throw std::runtime_error("Max number of tracks reached");
	vmd_track_t *ret = track[tracks++] = vmd_track_create(this, cm);
	commit("Add Track");
	return ret;
}

void
File::undo()
{
	if (FileRevision *prev = revision()->prev()) {
		update(prev);
		emit acted();
	}
}

void
File::redo()
{
	if (FileRevision *next = revision()->next()) {
		update(next);
		emit acted();
	}
}

FileRevision::FileRevision(File *f, QString _descr)
	:rev_(vmd_file_commit(f)),
	descr_(_descr),
	prev_(f->revision()),
	next_(NULL)
{
	if (rev_ == NULL)
		throw std::runtime_error("File commit failed");
}

FileRevision::~FileRevision()
{
	delete next_;
}
