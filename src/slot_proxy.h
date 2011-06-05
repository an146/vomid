/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#ifndef SLOT_PROXY_H
#define SLOT_PROXY_H

#include "util.h"

class QObject;

/* Simple slot proxy, which connect()s source objects' signals
 * to slots of a target object, set anytime by setTarget()
 */
class SlotProxy
{
public:
	SlotProxy();
	void connect(QObject *src, const char *signal, const char *slot);
	//TODO: disconnect()
	void setTarget(QObject *target);

private:
	struct Impl;
	pimpl_ptr<Impl> impl;
};

#endif /* SLOT_PROXY_H */
