/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#ifndef FILE_H
#define FILE_H

#include <QObject>
#include <QString>
#include <vomid.h>

class FileRevision;

class File : public QObject, public vmd_file_t
{
	Q_OBJECT

public:
	File();
	File(QString);
	~File();

	FileRevision *revision() { return revision_; }
	FileRevision *saved_revision() { return saved_revision_; }
	QString filename() const { return filename_; }
	bool saved() const { return revision_ == saved_revision_; }

	void save_as(QString);
	void commit(QString);
	void update(FileRevision *);
	void add_track(const vmd_notesystem_t *);

public slots:
	void undo();
	void redo();

signals:
	void acted();

private:
	QString filename_;
	FileRevision *revision_;
	FileRevision *saved_revision_;
};

class FileRevision
{
	friend class File;

public:
	QString name() const { return name_; }
	FileRevision *prev() { return prev_; }
	FileRevision *next() { return next_; }

protected:
	FileRevision(File *, QString);
	~FileRevision();

private:
	vmd_file_rev_t *rev_;
	QString name_;
	FileRevision *prev_, *next_;
};

#endif /* FILE_H */
