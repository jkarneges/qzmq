#ifndef QZMQVALVE_H
#define QZMQVALVE_H

#include <QObject>

namespace QZmq {

class Socket;

class Valve : public QObject
{
	Q_OBJECT

public:
	Valve(QZmq::Socket *sock, QObject *parent = 0);
	~Valve();

	bool isOpen() const;

	void open();
	void close();

signals:
	void readyRead(const QList<QByteArray> &message);

private:
	class Private;
	friend class Private;
	Private *d;
};

}

#endif
