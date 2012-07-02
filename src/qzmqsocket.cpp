#include "qzmqsocket.h"

#include <stdio.h>
#include <assert.h>
#include <QStringList>
#include <QTimer>
#include <QSocketNotifier>
#include <QMutex>
#include <zmq.h>
#include "qzmqcontext.h"

namespace QZmq {

static bool get_rcvmore(void *sock)
{
	qint64 more;
	size_t opt_len = sizeof(more);
	int ret = zmq_getsockopt(sock, ZMQ_RCVMORE, &more, &opt_len);
	assert(ret == 0);
	return more ? true : false;
}

static int get_fd(void *sock)
{
	int fd;
	size_t opt_len = sizeof(fd);
	int ret = zmq_getsockopt(sock, ZMQ_FD, &fd, &opt_len);
	assert(ret == 0);
	return fd;
}

static int get_events(void *sock)
{
	quint32 events;
	size_t opt_len = sizeof(events);
	int ret = zmq_getsockopt(sock, ZMQ_EVENTS, &events, &opt_len);
	assert(ret == 0);
	return (int)events;
}

static void set_subscribe(void *sock, const char *data, int size)
{
	size_t opt_len = size;
	int ret = zmq_setsockopt(sock, ZMQ_SUBSCRIBE, data, opt_len);
	assert(ret == 0);
}

static void set_unsubscribe(void *sock, const char *data, int size)
{
	size_t opt_len = size;
	zmq_setsockopt(sock, ZMQ_UNSUBSCRIBE, data, opt_len);
	// note: we ignore errors, such as unsubscribing a nonexisting filter
}

static void set_linger(void *sock, int value)
{
	size_t opt_len = sizeof(value);
	int ret = zmq_setsockopt(sock, ZMQ_LINGER, &value, opt_len);
	assert(ret == 0);
}

Q_GLOBAL_STATIC(QMutex, g_mutex)

class Global
{
public:
	Context context;
	int refs;

	Global() :
		refs(0)
	{
	}
};

static Global *global = 0;

static Context *addGlobalContextRef()
{
	QMutexLocker locker(g_mutex());

	if(!global)
		global = new Global;

	++(global->refs);
	return &(global->context);
}

static void removeGlobalContextRef()
{
	QMutexLocker locker(g_mutex());

	assert(global);
	assert(global->refs > 0);

	--(global->refs);
	if(global->refs == 0)
	{
		delete global;
		global = 0;
	}
}

class Socket::Private : public QObject
{
	Q_OBJECT

public:
	Socket *q;
	bool usingGlobalContext;
	Context *context;
	void *sock;
	QSocketNotifier *sn_read;
	bool canWrite, canRead;
	QList<QByteArray> pendingRead;
	bool readComplete;
	QList<QList<QByteArray> > pendingWrites;
	QTimer *updateTimer;
	bool pendingUpdate;
	int shutdownWaitTime;

	Private(Socket *_q, Socket::Type type, Context *_context) :
		QObject(_q),
		q(_q),
		canWrite(false),
		canRead(false),
		readComplete(false),
		pendingUpdate(false),
		shutdownWaitTime(-1)
	{
		if(_context)
		{
			usingGlobalContext = false;
			context = _context;
		}
		else
		{
			usingGlobalContext = true;
			context = addGlobalContextRef();
		}

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

		sock = zmq_socket(context->context(), ztype);
		assert(sock != NULL);

		sn_read = new QSocketNotifier(get_fd(sock), QSocketNotifier::Read, this);
		connect(sn_read, SIGNAL(activated(int)), SLOT(sn_read_activated()));
		sn_read->setEnabled(true);

		updateTimer = new QTimer(this);
		connect(updateTimer, SIGNAL(timeout()), SLOT(update_timeout()));
		updateTimer->setSingleShot(true);
	}

	~Private()
	{
		set_linger(sock, shutdownWaitTime);
		zmq_close(sock);

		if(usingGlobalContext)
			removeGlobalContextRef();
	}

	QList<QByteArray> read()
	{
		if(readComplete)
		{
			QList<QByteArray> out = pendingRead;
			pendingRead.clear();
			readComplete = false;

			if(canRead && !pendingUpdate)
			{
				pendingUpdate = true;
				updateTimer->start();
			}

			return out;
		}
		else
			return QList<QByteArray>();
	}

	void write(const QList<QByteArray> &message)
	{
		assert(!message.isEmpty());

		pendingWrites += message;

		if(canWrite && !pendingUpdate)
		{
			pendingUpdate = true;
			updateTimer->start();
		}
	}

	void processEvents(bool *readyRead, int *messagesWritten)
	{
		bool again;
		do
		{
			again = false;

			int flags = get_events(sock);

			if(flags & ZMQ_POLLOUT)
			{
				canWrite = true;
				again = tryWrite(messagesWritten) || again;
			}

			if(flags & ZMQ_POLLIN)
			{
				canRead = true;
				again = tryRead(readyRead) || again;
			}
		} while(again);
	}

	bool tryWrite(int *messagesWritten)
	{
		if(!pendingWrites.isEmpty())
		{
			bool writeComplete = false;
			QByteArray buf = pendingWrites.first().takeFirst();
			if(pendingWrites.first().isEmpty())
			{
				pendingWrites.removeFirst();
				writeComplete = true;
			}

			zmq_msg_t msg;
			int ret = zmq_msg_init_size(&msg, buf.size());
			assert(ret == 0);
			memcpy(zmq_msg_data(&msg), buf.data(), buf.size());
			ret = zmq_send(sock, &msg, !writeComplete ? ZMQ_SNDMORE : 0);
			assert(ret == 0);
			ret = zmq_msg_close(&msg);
			assert(ret == 0);

			canWrite = false;

			if(writeComplete)
				++(*messagesWritten);

			return true;
		}

		return false;
	}

	// return true if a read was performed
	bool tryRead(bool *readyRead)
	{
		if(!readComplete)
		{
			zmq_msg_t msg;
			int ret = zmq_msg_init(&msg);
			assert(ret == 0);
			ret = zmq_recv(sock, &msg, 0);
			assert(ret == 0);
			QByteArray buf((const char *)zmq_msg_data(&msg), zmq_msg_size(&msg));
			ret = zmq_msg_close(&msg);
			assert(ret == 0);

			pendingRead += buf;
			canRead = false;

			if(!get_rcvmore(sock))
			{
				readComplete = true;
				*readyRead = true;
			}

			return true;
		}

		return false;
	}

public slots:
	void sn_read_activated()
	{
		bool readyRead = false;
		int messagesWritten = 0;

		processEvents(&readyRead, &messagesWritten);

		if(readyRead)
			emit q->readyRead();

		if(messagesWritten > 0)
			emit q->messagesWritten(messagesWritten);
	}

	void update_timeout()
	{
		pendingUpdate = false;

		bool readyRead = false;
		int messagesWritten = 0;

		if(canWrite)
		{
			bool ret = tryWrite(&messagesWritten);
			assert(ret);
			processEvents(&readyRead, &messagesWritten);
		}

		if(canRead)
		{
			bool ret = tryRead(&readyRead);
			assert(ret);
			processEvents(&readyRead, &messagesWritten);
		}

		if(readyRead)
			emit q->readyRead();

		if(messagesWritten > 0)
			emit q->messagesWritten(messagesWritten);
	}
};

Socket::Socket(Type type, QObject *parent) :
	QObject(parent)
{
	d = new Private(this, type, 0);
}

Socket::Socket(Type type, Context *context, QObject *parent) :
	QObject(parent)
{
	d = new Private(this, type, context);
}

Socket::~Socket()
{
	delete d;
}

void Socket::setShutdownWaitTime(int msecs)
{
	d->shutdownWaitTime = msecs;
}

void Socket::subscribe(const QByteArray &filter)
{
	set_subscribe(d->sock, filter.data(), filter.size());
}

void Socket::unsubscribe(const QByteArray &filter)
{
	set_unsubscribe(d->sock, filter.data(), filter.size());
}

void Socket::connectToAddress(const QString &addr)
{
	int ret = zmq_connect(d->sock, addr.toUtf8().data());
	assert(ret == 0);
}

bool Socket::bind(const QString &addr)
{
	int ret = zmq_bind(d->sock, addr.toUtf8().data());
	if(ret != 0)
		return false;

	return true;
}

bool Socket::canRead() const
{
	return d->readComplete;
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
