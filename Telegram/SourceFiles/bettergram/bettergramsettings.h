/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/observer.h"
#include <QObject>
#include <QtNetwork/QNetworkAccessManager>

namespace Bettergram {

class CryptoPriceList;
class AdItem;

/**
 * @brief The BettergramSettings class contains Bettergram specific settings
 */
class BettergramSettings : public QObject {
	Q_OBJECT

public:
	enum class BillingPlan {
		Unknown,
		Monthly,
		Quarterly,
		Yearly
	};

	static BettergramSettings *init();
	static BettergramSettings *instance();

	bool isPaid() const;
	BillingPlan billingPlan() const;

	CryptoPriceList *cryptoPriceList() const;
	AdItem *currentAd() const;

	base::Observable<void> &isPaidObservable();
	base::Observable<void> &billingPlanObservable();

	void getCryptoPriceList();

public slots:

signals:
	void isPaidChanged();
	void billingPlanChanged();

protected:

private:
	static BettergramSettings *_instance;

	QNetworkAccessManager _networkManager;

	bool _isPaid = false;
	BillingPlan _billingPlan = BillingPlan::Unknown;

	CryptoPriceList *_cryptoPriceList = nullptr;
	AdItem *_currentAd = nullptr;

	base::Observable<void> _isPaidObservable;
	base::Observable<void> _billingPlanObservable;

	explicit BettergramSettings(QObject *parent = nullptr);

	void setIsPaid(bool isPaid);
	void setBillingPlan(BillingPlan billingPlan);

	void getIsPaid();
	void getNextAd();

	void parseCryptoPriceList(const QByteArray &byteArray);

private slots:
	void onGetCryptoPriceListFinished();
	void onGetCryptoPriceListSslFailed(QList<QSslError> errors);
};

} // namespace Bettergram
