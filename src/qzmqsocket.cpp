#include "qzmqsocket.h"

#include <stdio.h>
#include <assert.h>
#include <QStringList>
#include <QSocketNotifier>
#include "zmq.h"

namespace QZmq {

static void *g_context = 0;

class Socket::Private : public QObject
{
	Q_OBJECT

public:
	Socket *q;
	void *sock;
	QSocketNotifier *sn_read;
	bool canWrite, canRead;
	QList<QList<QByteArray> > pendingWrites;

	Private(Socket *_q, Socket::Type type) :
		QObject(_q),
		q(_q),
		canWrite(false),
		canRead(false)
	{
		if(!g_context)
			g_context = zmq_init(1);

		int ztype;
		switch(type)
		{
			case Socket::Pair: ztype = ZMQ_PAIR; break;
			case Socket::Dealer: ztype = ZMQ_DEALER; break;
			case Socket::Router: ztype = ZMQ_ROUTER; break;
			case Socket::Req: ztype = ZMQ_REQ; break;
			case Socket::Rep: ztype = ZMQ_REP; break;
			case Socket::Push: ztype = ZMQ_PUSH; break;
			case Socket::Pull: ztype = ZMQ_PULL; break;
			case Socket::Pub: ztype = ZMQ_PUB; break;
			case Socket::Sub: ztype = ZMQ_SUB; break;
			default:
				assert(0);
		}

		sock = zmq_socket(g_context, ztype);
		int fd;
		size_t zmq_fd_size = sizeof(int);
		zmq_getsockopt(sock, ZMQ_FD, &fd, &zmq_fd_size);
		sn_read = new QSocketNotifier(fd, QSocketNotifier::Read, this);
		connect(sn_read, SIGNAL(activated(int)), SLOT(sn_read_activated()));
		sn_read->setEnabled(true);
	}

	~Private()
	{
		zmq_close(sock);

		zmq_term(g_context);
		g_context = 0;
	}

	QList<QByteArray> read()
	{
		if(!canRead)
			return QList<QByteArray>();

		canRead = false;

		QList<QByteArray> in;

		qint64 more; // Multipart detection
		while (1) {
			zmq_msg_t reply;
			zmq_msg_init(&reply);
			zmq_recv(sock, &reply, 0);
			QByteArray buf((const char *)zmq_msg_data(&reply), zmq_msg_size(&reply));
			zmq_msg_close(&reply);
			in += buf;

			size_t more_size = sizeof (more);
			zmq_getsockopt(sock, ZMQ_RCVMORE, &more, &more_size);
			if(!more)
				break;
		}

		processEvents();

		return in;
	}

	void write(const QList<QByteArray> &message)
	{
		if(canWrite)
		{
			canWrite = false;

			printf("writing immediately\n");
			for(int n = 0; n < message.count(); ++n)
			{
				zmq_msg_t request;
				zmq_msg_init_size(&request, message[n].size());
				memcpy(zmq_msg_data(&request), message[n].data(), message[n].size());
				zmq_send(sock, &request, n + 1 < message.count() ? ZMQ_SNDMORE : 0);
			}

			QMetaObject::invokeMethod(q, "messagesWritten", Qt::QueuedConnection, Q_ARG(int, 1));

			processEvents();
		}
		else
		{
			printf("can't write yet, queuing\n");
			pendingWrites += message;
		}
	}

	int readEventsFlags()
	{
		size_t zmq_events_size = sizeof(quint32);
		quint32 zmq_events;
		zmq_getsockopt(sock, ZMQ_EVENTS, &zmq_events, &zmq_events_size);
		QStringList list;
		if(zmq_events & ZMQ_POLLIN)
			list += "ZMQ_POLLIN";
		if(zmq_events & ZMQ_POLLOUT)
			list += "ZMQ_POLLOUT";
		printf("events: %s\n", qPrintable(list.join(", ")));
		return (int)zmq_events;
	}

	void processEvents()
	{
		int flags = readEventsFlags();

		while(true)
		{
			if(flags & ZMQ_POLLOUT)
			{
				canWrite = true;

				if(!pendingWrites.isEmpty())
				{
					canWrite = false;

					QList<QByteArray> message = pendingWrites.takeFirst();
					for(int n = 0; n < message.count(); ++n)
					{
						zmq_msg_t request;
						zmq_msg_init_size(&request, message[n].size());
						memcpy(zmq_msg_data(&request), message[n].data(), message[n].size());
						zmq_send(sock, &request, n + 1 < message.count() ? ZMQ_SNDMORE : 0);
					}

					printf("wrote pending item\n");
					QMetaObject::invokeMethod(q, "messagesWritten", Qt::QueuedConnection, Q_ARG(int, 1));

					flags = readEventsFlags();
					continue;
				}
			}

			if(flags & ZMQ_POLLIN)
			{
				canRead = true;

				QMetaObject::invokeMethod(q, "readyRead", Qt::QueuedConnection);
			}

			break;
		}
	}

public slots:
	void sn_read_activated()
	{
		printf("sn_read_activated\n");

		processEvents();
	}
};

Socket::Socket(Type type, QObject *parent) :
	QObject(parent)
{
	d = new Private(this, type);
}

Socket::~Socket()
{
	delete d;
}

void Socket::connectToAddress(const QString &addr)
{
	zmq_connect(d->sock, addr.toUtf8().data());
}

void Socket::bind(const QString &addr)
{
	zmq_bind(d->sock, addr.toUtf8().data());
}

bool Socket::canRead() const
{
	return d->canRead;
}

QList<QByteArray> Socket::read()
{
	return d->read();
}

void Socket::write(const QList<QByteArray> &message)
{
	d->write(message);
}

}

#include "qzmqsocket.moc"
