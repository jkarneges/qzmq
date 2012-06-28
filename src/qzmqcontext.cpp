#include "qzmqcontext.h"

#include <assert.h>
#include <zmq.h>

namespace QZmq {

Context::Context(int ioThreads)
{
	context_ = zmq_init(ioThreads);
	assert(context_);
}

Context::~Context()
{
	zmq_term(context_);
}

}
