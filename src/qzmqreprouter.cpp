#include "qzmqreprouter.h"

#include "qzmqsocket.h"
#include "qzmqreqmessage.h"

namespace QZmq {

class RepRouter::Private : public QObject
{
	Q_OBJECT

public:
	RepRouter *q;
	Socket *sock;

	Private(RepRouter *_q) :
		QObject(_q),
		q(_q)
	{
		sock = new Socket(Socket::Router, this);
		connect(sock, SIGNAL(readyRead()), SLOT(sock_readyRead()));
		connect(sock, SIGNAL(messagesWritten(int)), SLOT(sock_messagesWritten(int)));
	}

public slots:
	void sock_readyRead()
	{
		emit q->readyRead();
	}

	void sock_messagesWritten(int count)
	{
		emit q->messagesWritten(count);
	}
};

RepRouter::RepRouter(QObject *parent) :
	QObject(parent)
{
	d = new Private(this);
}

RepRouter::~RepRouter()
{
	delete d;
}

void RepRouter::connectToAddress(const QString &addr)
{
	d->sock->connectToAddress(addr);
}

void RepRouter::bind(const QString &addr)
{
	d->sock->bind(addr);
}

bool RepRouter::canRead() const
{
	return d->sock->canRead();
}

ReqMessage RepRouter::read()
{
	return ReqMessage(d->sock->read());
}

void RepRouter::write(const ReqMessage &message)
{
	d->sock->write(message.toRawMessage());
}

}

#include "qzmqreprouter.moc"
