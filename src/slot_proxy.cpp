#include <QList>
#include <QObject>
#include <QPointer>
#include "slot_proxy.h"
using namespace std;

struct SlotProxy::Impl
{
	struct Connection {
		QPointer<QObject> src;
		const char *signal;
		const char *slot;
	};

	QPointer<QObject> target;
	QList<Connection> conns;
};

SlotProxy::SlotProxy()
{
}

void SlotProxy::connect(QObject *src, const char *signal, const char *slot)
{
	impl->conns.push_back((Impl::Connection){src, signal, slot});
	if (impl->target != NULL)
		QObject::connect(src, signal, impl->target, slot);
}

void SlotProxy::setTarget(QObject *target)
{
	foreach (const Impl::Connection &i, impl->conns) {
		if (i.src == NULL)
			continue;
		if (impl->target != NULL)
			QObject::disconnect(i.src, i.signal, impl->target, i.slot);
		if (target != NULL)
			QObject::connect(i.src, i.signal, target, i.slot);
	}
	impl->target = target;
}
