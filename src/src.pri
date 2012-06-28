exists(../conf.pri):include(../conf.pri)

HEADERS += \
	$$PWD/qzmqcontext.h \
	$$PWD/qzmqsocket.h \
	$$PWD/qzmqreqmessage.h \
	$$PWD/qzmqreprouter.h

SOURCES += \
	$$PWD/qzmqcontext.cpp \
	$$PWD/qzmqsocket.cpp \
	$$PWD/qzmqreprouter.cpp
