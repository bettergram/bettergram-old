/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QObject>

namespace Bettergram {

/**
 * @brief The CryptoPrice class contains current price of one cryptocurrency.
 */
class CryptoPrice : public QObject {
	Q_OBJECT

public:
	explicit CryptoPrice(QObject *parent = nullptr);

public slots:

signals:

protected:

private:

};

} // namespace Bettergram
