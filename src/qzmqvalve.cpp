#include "qzmqvalve.h"

#include <QPointer>
#include "qzmqsocket.h"

namespace QZmq {

class Valve::Private : public QObject
{
	Q_OBJECT

public:
	Valve *q;
	QZmq::Socket *sock;
	bool isOpen;
	bool pendingRead;

	Private(Valve *_q) :
		QObject(_q),
		q(_q),
		sock(0),
		isOpen(false),
		pendingRead(false)
	{
	}

	void setup(QZmq::Socket *_sock)
	{
		sock = _sock;
		connect(sock, SIGNAL(readyRead()), SLOT(sock_readyRead()));
	}

	void tryRead()
	{
		QPointer<QObject> self = this;

		while(isOpen && sock->canRead())
		{
			QList<QByteArray> msg = sock->read();
			emit q->readyRead(msg);
			if(!self)
				return;
		}
	}

private slots:
	void sock_readyRead()
	{
		tryRead();
	}

	void queuedRead()
	{
		pendingRead = false;
		tryRead();
	}
};

Valve::Valve(QZmq::Socket *sock, QObject *parent) :
	QObject(parent)
{
	d = new Private(this);
	d->setup(sock);
}

Valve::~Valve()
{
	delete d;
}

bool Valve::isOpen() const
{
	return d->isOpen;
}

void Valve::open()
{
	if(!d->isOpen)
	{
		d->isOpen = true;
		if(!d->pendingRead && d->sock->canRead())
		{
			d->pendingRead = true;
			QMetaObject::invokeMethod(d, "queuedRead", Qt::QueuedConnection);
		}
	}
}

void Valve::close()
{
	d->isOpen = false;
}

}

#include "qzmqvalve.moc"
