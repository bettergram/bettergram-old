/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/

#include "bettergramsettings.h"

namespace Bettergram {

BettergramSettings *BettergramSettings::_instance = nullptr;

BettergramSettings *BettergramSettings::instance()
{
	if (!_instance) {
		_instance = new BettergramSettings();
	}

	return _instance;
}

Bettergram::BettergramSettings::BettergramSettings(QObject *parent) : QObject(parent)
{
	getIsPaid();
}

bool BettergramSettings::isPaid() const
{
	return _isPaid;
}

void BettergramSettings::setIsPaid(bool isPaid)
{
	if (_isPaid != isPaid) {
		_isPaid = isPaid;
		emit isPaidChanged();
	}
}

void BettergramSettings::getIsPaid()
{
	//TODO: bettergram: ask server and get know if the instance is paid or not
}

} // namespace Bettergrams
