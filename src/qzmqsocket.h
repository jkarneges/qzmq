#ifndef QZMQSOCKET_H
#define QZMQSOCKET_H

#include <QObject>

namespace QZmq {

class Context;

class Socket : public QObject
{
	Q_OBJECT

public:
	enum Type
	{
		Pair,
		Dealer,
		Router,
		Req,
		Rep,
		Push,
		Pull,
		Pub,
		Sub
	};

	Socket(Type type, QObject *parent = 0);
	Socket(Type type, Context *context, QObject *parent = 0);
	~Socket();

	void connectToAddress(const QString &addr);
	bool bind(const QString &addr);

	bool canRead() const;

	QList<QByteArray> read();
	void write(const QList<QByteArray> &message);

signals:
	void readyRead();
	void messagesWritten(int count);

private:
	Q_DISABLE_COPY(Socket)

	class Private;
	friend class Private;
	Private *d;
};

}

#endif
