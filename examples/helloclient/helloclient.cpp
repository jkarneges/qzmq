#include <stdio.h>
#include <QCoreApplication>
#include <QTimer>
#include "qzmqsocket.h"

class App : public QObject
{
	Q_OBJECT

private:
	QZmq::Socket sock;

public:
	App() :
		sock(QZmq::Socket::Req)
	{
		QTimer *t = new QTimer(this);
		connect(t, SIGNAL(timeout()), SLOT(keepAlive()));
		t->start(5000);
	}

public slots:
	void start()
	{
		connect(&sock, SIGNAL(readyRead()), SLOT(sock_readyRead()));
		connect(&sock, SIGNAL(messagesWritten(int)), SLOT(sock_messagesWritten(int)));
		sock.connectToAddress("tcp://localhost:5555");
		sock.write(QList<QByteArray>() << "hello");
	}

signals:
	void quit();

private slots:
	void keepAlive()
	{
		//printf("still here\n");
	}

	void sock_readyRead()
	{
		printf("read\n");
		QList<QByteArray> resp = sock.read();
		printf("%s\n", resp[0].data());
		emit quit();
	}

	void sock_messagesWritten(int count)
	{
		printf("written: %d\n", count);
	}
};

int main(int argc, char **argv)
{
	QCoreApplication qapp(argc, argv);
	App app;
	QObject::connect(&app, SIGNAL(quit()), &qapp, SLOT(quit()));
	QTimer::singleShot(0, &app, SLOT(start()));
	return qapp.exec();
}

#include "helloclient.moc"
