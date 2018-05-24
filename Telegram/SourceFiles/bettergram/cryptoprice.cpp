/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/

#include "cryptoprice.h"

namespace Bettergram {

CryptoPrice::CryptoPrice(const QString &name, const QString &shortName, QObject *parent) :
	QObject(parent),
	_name(name),
	_shortName(shortName)
{
}

const QString &CryptoPrice::name() const
{
	return _name;
}

const QString &CryptoPrice::shortName() const
{
	return _shortName;
}

double CryptoPrice::currentPrice() const
{
	return _currentPrice;
}

QString CryptoPrice::currentPriceString() const
{
	if (_currentPrice < 1.0) {
		return QString("$%1").arg(_currentPrice, 0, 'g', 4);
	} else {
		return QString("$%1").arg(_currentPrice, 0, 'g', 2);
	}
}

void CryptoPrice::setCurrentPrice(double currentPrice)
{
	if (_currentPrice != currentPrice) {
		_currentPrice = currentPrice;
		emit currentPriceChanged();
	}
}

double CryptoPrice::changeFor24Hours() const
{
	return _changeFor24Hours;
}

QString CryptoPrice::changeFor24HoursString() const
{
	return QString("%1%").arg(_changeFor24Hours, 0, 'g', 2);
}

void CryptoPrice::setChangeFor24Hours(double changeFor24Hours)
{
	if (_changeFor24Hours != changeFor24Hours) {
		_changeFor24Hours = changeFor24Hours;

		setIsChangeFor24HoursGrown(_changeFor24Hours >= 0.0);
		emit changeFor24HoursChanged();
	}
}

bool CryptoPrice::isCurrentPriceGrown() const
{
	return _isCurrentPriceGrown;
}

void CryptoPrice::setIsCurrentPriceGrown(bool isCurrentPriceGrown)
{
	if (_isCurrentPriceGrown != isCurrentPriceGrown) {
		_isCurrentPriceGrown = isCurrentPriceGrown;
		emit currentPriceChanged();
	}
}

bool CryptoPrice::isChangeFor24HoursGrown() const
{
	return _isChangeFor24HoursGrown;
}

void CryptoPrice::setIsChangeFor24HoursGrown(bool isChangeFor24HoursGrown)
{
	if (_isChangeFor24HoursGrown != isChangeFor24HoursGrown) {
		_isChangeFor24HoursGrown = isChangeFor24HoursGrown;
		emit isChangeFor24HoursGrownChanged();
	}
}

} // namespace Bettergrams
