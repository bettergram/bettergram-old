#pragma once

#include <QObject>
#include <QUrl>

namespace Bettergram {

/**
 * @brief The AdItem class
 */
class AdItem : public QObject {
	Q_OBJECT

public:
	static int defaultDuration();

	explicit AdItem(QObject *parent = nullptr);

	explicit AdItem(const QString &id,
					const QString &text,
					const QUrl &url,
					int duration,
					QObject *parent = nullptr);

	QString id() const;
	QString text() const;
	QUrl url() const;
	int duration() const;

	bool isEmpty() const;
	bool isEqual(const AdItem &adItem) const;

	void clear();
	void update(const AdItem &adItem);

signals:
	void updated();

private:
	static const int _defaultDuration;

	QString _id;
	QString _text;
	QUrl _url;

	/// Duration of the ad in seconds.
	int _duration = 60;
};

} // namespace Bettergram
