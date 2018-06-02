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
	enum class SortOrder {
		None,

		NameAscending,
		NameDescending,

		PriceAscending,
		PriceDescending,

		ChangeFor24hAscending,
		ChangeFor24hDescending,
	};

	typedef QList<CryptoPrice*>::const_iterator const_iterator;

	explicit CryptoPriceList(QObject *parent = nullptr);

	double marketCap() const;
	QString marketCapString() const;
	void setMarketCap(double marketCap);

	const_iterator begin() const;
	const_iterator end() const;

	CryptoPrice *at(int index) const;
	int count() const;

	SortOrder sortOrder() const;
	void setSortOrder(const SortOrder &sortOrder);

	void updateData(double marketCap, const QList<CryptoPrice> &priceList);

	void createTestData();

public slots:

signals:
	void marketCapChanged();
	void sortOrderChanged();
	void updated();

protected:

private:
	QList<CryptoPrice*> _list;
	double _marketCap = 0.0;
	SortOrder _sortOrder = SortOrder::None;

	static bool containsName(const QList<CryptoPrice> &priceList, const QString &name);

	static bool sortByName(const CryptoPrice *price1, const CryptoPrice *price2);
	static bool sortByPrice(const CryptoPrice *price1, const CryptoPrice *price2);
	static bool sortBy24h(const CryptoPrice *price1, const CryptoPrice *price2);

	CryptoPrice *findByName(const QString &name);
	void sort();

	void addTestData(const QUrl &url,
					 const QString &name,
					 const QString &shortName,
					 double currentPrice,
					 double changeFor24Hours,
					 bool isCurrentPriceGrown);
	void clear();
};

} // namespace Bettergram
