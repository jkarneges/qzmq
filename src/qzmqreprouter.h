#ifndef QZMQREPROUTER_H
#define QZMQREPROUTER_H

#include <QObject>

namespace QZmq {

class ReqMessage;

class RepRouter : public QObject
{
	Q_OBJECT

public:
	RepRouter(QObject *parent = 0);
	~RepRouter();

	void connectToAddress(const QString &addr);
	void bind(const QString &addr);

	bool canRead() const;

	ReqMessage read();
	void write(const ReqMessage &message);

signals:
	void readyRead();
	void messagesWritten(int count);

private:
	Q_DISABLE_COPY(RepRouter)

	class Private;
	friend class Private;
	Private *d;
};

}

#endif
