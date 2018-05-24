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

	const_iterator begin() const;
	const_iterator end() const;

	CryptoPrice *at(int index) const;
	int count() const;

public slots:

signals:

protected:

private:
	QList<CryptoPrice*> _list;
};

} // namespace Bettergram
