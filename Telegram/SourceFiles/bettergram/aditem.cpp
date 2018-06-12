#include "aditem.h"

namespace Bettergram {

const int AdItem::_defaultDuration = 60;

int AdItem::defaultDuration()
{
	return _defaultDuration;
}

AdItem::AdItem(QObject *parent) :
	QObject(parent),
	_duration(_defaultDuration)
{
}

AdItem::AdItem(const QString &id,
			   const QString &text,
			   const QUrl &url,
			   int duration,
			   QObject *parent) :
	QObject(parent),
	_id(id),
	_text(text),
	_url(url),
	_duration(duration)
{
}

QString AdItem::id() const
{
	return _id;
}

QString AdItem::text() const
{
	return _text;
}

QUrl AdItem::url() const
{
	return _url;
}

int AdItem::duration() const
{
	return _duration;
}

bool AdItem::isEmpty() const
{
	return _id.isEmpty();
}

bool AdItem::isEqual(const AdItem &adItem) const
{
	return _id == adItem._id
			&& _text == adItem._text
			&& _url == adItem._url
			&& _duration == adItem._duration;
}

void AdItem::clear()
{
	_id.clear();
	_text.clear();
	_url.clear();
	_duration = _defaultDuration;
}

void AdItem::update(const AdItem &adItem)
{
	if(isEqual(adItem)) {
		return;
	}

	_id = adItem._id;
	_text = adItem._text;
	_url = adItem._url;
	_duration = adItem._duration;

	emit updated();
}

} // namespace Bettergrams
