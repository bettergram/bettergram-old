/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/

#include "cryptopricelist.h"
#include "cryptoprice.h"
#include <logs.h>

namespace Bettergram {

CryptoPriceList::CryptoPriceList(QObject *parent) : QObject(parent)
{
}

CryptoPriceList::const_iterator CryptoPriceList::begin() const
{
	return _list.begin();
}

CryptoPriceList::const_iterator CryptoPriceList::end() const
{
	return _list.end();
}

CryptoPrice *CryptoPriceList::at(int index) const
{
	if (index < 0 || index >= _list.size()) {
		LOG(("Index is out of bounds"));
		return nullptr;
	}

	return _list.at(index);
}

int CryptoPriceList::count() const
{
	return _list.count();
}

void CryptoPriceList::createTestData()
{
	clear();

	addTestData("Bitcoin", "BTC", 7935.96, -3.22, false);
	addTestData("Ethereum", "ETH", 625.64, -8.43, false);
	addTestData("Ripple", "XRP", 0.6246, -6.59, true);
	addTestData("Bitcoin Cash", "BCH", 1064.28, 9.45, true);
	addTestData("EOS", "EOS", 11.5, -10.6358, true);
	addTestData("Litecoin", "LTC", 125.46, -5.28, true);
	addTestData("Cardano", "ADA", 0.2151, -9.72, true);
	addTestData("Stellar", "XLM", 0.2882, -6.81, true);
	addTestData("TRON", "TRX", 0.071258, -8.4789, true);
	addTestData("IOTA", "MIOTA", 1.55, 8.77, true);
}

void CryptoPriceList::addTestData(const QString &name,
								  const QString &shortName,
								  double currentPrice,
								  double changeFor24Hours,
								  bool isCurrentPriceGrown)
{
	CryptoPrice *price = new CryptoPrice(name, shortName, this);

	price->setCurrentPrice(currentPrice);
	price->setChangeFor24Hours(changeFor24Hours);
	price->setIsCurrentPriceGrown(isCurrentPriceGrown);

	_list.push_back(price);
}

void CryptoPriceList::clear()
{
	while (!_list.isEmpty()) {
		_list.takeFirst()->deleteLater();
	}
}

} // namespace Bettergrams
