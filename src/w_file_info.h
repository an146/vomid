/* (C)opyright 2010 Anton Novikov
 * See LICENSE file for license details.
 */

#ifndef W_FILE_INFO_H
#define W_FILE_INFO_H

#include <QDialog>
#include "util.h"

class File;
class Ui_FileInfoDialog;
class WFile;

class WFileInfo : public QDialog
{
	Q_OBJECT

public:
	WFileInfo(WFile *);

	File *file() const;

private:
	WFile *wfile_;
	pimpl_ptr<Ui_FileInfoDialog> ui;
};

#endif /* W_FILE_INFO_H */
