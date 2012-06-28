#ifndef QZMQCONTEXT_H
#define QZMQCONTEXT_H

namespace QZmq {

class Context
{
public:
	Context(int ioThreads = 1);
	~Context();

	// the zmq context
	void *context() { return context_; }

private:
	void *context_;
};

}

#endif
