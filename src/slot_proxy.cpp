#include <QList>
#include <QObject>
#include <QPointer>
#include "slot_proxy.h"
using namespace std;

struct Connection {
	QPointer<QObject> src;
	const char *signal;
	const char *slot;
};

struct SlotProxy::Impl
{
	QPointer<QObject> target;
	QList<Connection> conns;
};

SlotProxy::SlotProxy()
{
}

#define TARGET (pimpl->target)
#define CONNS  (pimpl->conns)

void SlotProxy::connect(QObject *src, const char *signal, const char *slot)
{
	Connection conn = {src, signal, slot};
	pimpl->conns.push_back(conn);
	if (TARGET != NULL)
		QObject::connect(src, signal, TARGET, slot);
}

void SlotProxy::setTarget(QObject *t)
{
	for (QList<Connection>::iterator i = CONNS.begin(); i != CONNS.end(); ++i) {
		if (i->src == NULL)
			continue;
		if (TARGET != NULL)
			QObject::disconnect(i->src, i->signal, TARGET, i->slot);
		if (t != NULL)
			QObject::connect(i->src, i->signal, t, i->slot);
	}
	TARGET = t;
}
