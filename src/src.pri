exists(../conf.pri):include(../conf.pri)

HEADERS += \
	$$PWD/qzmqsocket.h \
	$$PWD/qzmqreqmessage.h \
	$$PWD/qzmqreprouter.h

SOURCES += \
	$$PWD/qzmqsocket.cpp \
	$$PWD/qzmqreprouter.cpp
