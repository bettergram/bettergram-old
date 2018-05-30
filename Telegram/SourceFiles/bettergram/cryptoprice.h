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
 * See also https://www.livecoinwatch.com
 */
class CryptoPrice : public QObject {
	Q_OBJECT

public:
	explicit CryptoPrice(const QUrl &url, const QString &name, const QString &shortName, QObject *parent = nullptr);

	const QUrl &url() const;
	const QString &name() const;
	const QString &shortName() const;

	double currentPrice() const;
	QString currentPriceString() const;
	void setCurrentPrice(double currentPrice);

	double changeFor24Hours() const;
	QString changeFor24HoursString() const;
	void setChangeFor24Hours(double changeFor24Hours);

	bool isCurrentPriceGrown() const;
	void setIsCurrentPriceGrown(bool isCurrentPriceGrown);

	bool isChangeFor24HoursGrown() const;

public slots:

signals:
	void currentPriceChanged();
	void changeFor24HoursChanged();

	void isCurrentPriceGrownChanged();
	void isChangeFor24HoursGrownChanged();

protected:

private:
	/// Site address of the information about the cryptocurrency. For example: https://www.livecoinwatch.com/price/Bitcoin-BTC
	const QUrl _url;

	/// Name of the cryptocurrency. For example: Bitcoin
	const QString _name;

	/// Short name of the cryptocurrency. For example: BTC
	const QString _shortName;

	/// Current price of the cryptocurrency. For example: $7935.96
	double _currentPrice;

	/// Price change of the cryptocurrency for the latest 24 hours. For example: -3.22%
	double _changeFor24Hours;

	/// True if the current price of the cryptocurrency is grown
	bool _isCurrentPriceGrown;

	/// True if the price change of the cryptocurrency for the latest 24 hours is grown
	bool _isChangeFor24HoursGrown;

	void setIsChangeFor24HoursGrown(bool isChangeFor24HoursGrown);
};

} // namespace Bettergram
