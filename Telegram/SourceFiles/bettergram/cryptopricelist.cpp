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

double CryptoPriceList::marketCap() const
{
	return _marketCap;
}

QString CryptoPriceList::marketCapString() const
{
	return QString::number(_marketCap);
}

void CryptoPriceList::setMarketCap(double marketCap)
{
	if (_marketCap != marketCap) {
		_marketCap = marketCap;
		emit marketCapChanged();
	}
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

	addTestData(QUrl("https://www.livecoinwatch.com/price/Bitcoin-BTC"), "Bitcoin", "BTC", 7935.96, -3.22, false);
	addTestData(QUrl("https://www.livecoinwatch.com/price/Ethereum-ETH"), "Ethereum", "ETH", 625.64, -8.43, false);
	addTestData(QUrl("https://www.livecoinwatch.com/price/Ripple-XRP"), "Ripple", "XRP", 0.6246, -6.59, true);
	addTestData(QUrl("https://www.livecoinwatch.com/price/BitcoinCash-BCH"), "Bitcoin Cash", "BCH", 1064.28, 9.45, true);
	addTestData(QUrl("https://www.livecoinwatch.com/price/EOS-EOS"), "EOS", "EOS", 11.5, -10.6358, true);
	addTestData(QUrl("https://www.livecoinwatch.com/price/Litecoin-LTC"), "Litecoin", "LTC", 125.46, -5.28, true);
	addTestData(QUrl("https://www.livecoinwatch.com/price/Cardano-ADA"), "Cardano", "ADA", 0.2151, -9.72, true);
	addTestData(QUrl("https://www.livecoinwatch.com/price/Stellar-XLM"), "Stellar", "XLM", 0.2882, -6.81, true);
	addTestData(QUrl("https://www.livecoinwatch.com/price/TRON-TRX"), "TRON", "TRX", 0.071258, -8.4789, true);
	addTestData(QUrl("https://www.livecoinwatch.com/price/IOTA-MIOTA"), "IOTA", "MIOTA", 1.55, 8.77, true);
}

void CryptoPriceList::addTestData(const QUrl &url,
								  const QString &name,
								  const QString &shortName,
								  double currentPrice,
								  double changeFor24Hours,
								  bool isCurrentPriceGrown)
{
	CryptoPrice *price = new CryptoPrice(url, name, shortName, this);

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
