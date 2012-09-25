/*
 * Copyright (C) 2012 Justin Karneges
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

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
