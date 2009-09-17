/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#ifndef SLOT_PROXY_H
#define SLOT_PROXY_H

#include "util.h"

class QObject;

class SlotProxy
{
public:
	SlotProxy();
	void connect(QObject *, const char *, const char *);
	void setTarget(QObject *);

private:
	struct Impl;
	pimpl_ptr<Impl> pimpl;
};

#endif /* SLOT_PROXY_H */
