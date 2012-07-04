#ifndef QZMQREQMESSAGE_H
#define QZMQREQMESSAGE_H

namespace QZmq {

class ReqMessage
{
public:
	ReqMessage()
	{
	}

	ReqMessage(const QList<QByteArray> &headers, const QList<QByteArray> &content) :
		headers_(headers),
		content_(content)
	{
	}

	ReqMessage(const QList<QByteArray> &rawMessage)
	{
		bool collectHeaders = true;
		foreach(const QByteArray &part, rawMessage)
		{
			if(part.isEmpty())
			{
				collectHeaders = false;
				continue;
			}

			if(collectHeaders)
				headers_ += part;
			else
				content_ += part;
		}
	}

	bool isNull() const { return headers_.isEmpty() && content_.isEmpty(); }

	QList<QByteArray> headers() const { return headers_; }
	QList<QByteArray> content() const { return content_; }

	ReqMessage createReply(const QList<QByteArray> &content)
	{
		return ReqMessage(headers_, content);
	}

	QList<QByteArray> toRawMessage() const
	{
		QList<QByteArray> out;
		out += headers_;
		out += QByteArray();
		out += content_;
		return out;
	}

private:
	QList<QByteArray> headers_;
	QList<QByteArray> content_;
};

}

#endif
