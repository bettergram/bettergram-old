/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QObject>

namespace Bettergram {

class CryptoPrice;

/**
 * @brief The CryptoPriceList class contains list of CryptoPrice instances.
 */
class CryptoPriceList : public QObject {
	Q_OBJECT

public:
	typedef QList<CryptoPrice*>::const_iterator const_iterator;

	explicit CryptoPriceList(QObject *parent = nullptr);

	double marketCap() const;
	QString marketCapString() const;
	void setMarketCap(double marketCap);

	const_iterator begin() const;
	const_iterator end() const;

	CryptoPrice *at(int index) const;
	int count() const;

	void createTestData();

public slots:

signals:
	void marketCapChanged();

protected:

private:
	QList<CryptoPrice*> _list;
	double _marketCap = 0.0;

	void addTestData(const QString &name,
					 const QString &shortName,
					 double currentPrice,
					 double changeFor24Hours,
					 bool isCurrentPriceGrown);
	void clear();
};

} // namespace Bettergram
