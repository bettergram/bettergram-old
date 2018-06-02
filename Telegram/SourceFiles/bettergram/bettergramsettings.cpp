/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/

#include "bettergramsettings.h"
#include "cryptopricelist.h"
#include "cryptoprice.h"

#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

namespace Bettergram {

BettergramSettings *BettergramSettings::_instance = nullptr;

BettergramSettings *BettergramSettings::init()
{
	return instance();
}

BettergramSettings *BettergramSettings::instance()
{
	if (!_instance) {
		_instance = new BettergramSettings();
	}

	return _instance;
}

Bettergram::BettergramSettings::BettergramSettings(QObject *parent) :
	QObject(parent),
	_cryptoPriceList(new CryptoPriceList(this))
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
		_isPaidObservable.notify();
	}
}

BettergramSettings::BillingPlan BettergramSettings::billingPlan() const
{
	return _billingPlan;
}

CryptoPriceList *BettergramSettings::cryptoPriceList() const
{
	return _cryptoPriceList;
}

void BettergramSettings::setBillingPlan(BillingPlan billingPlan)
{
	if (_billingPlan != billingPlan) {
		_billingPlan = billingPlan;

		emit billingPlanChanged();
		_billingPlanObservable.notify();
	}
}

base::Observable<void> &BettergramSettings::isPaidObservable()
{
	return _isPaidObservable;
}

base::Observable<void> &BettergramSettings::billingPlanObservable()
{
	return _billingPlanObservable;
}

void BettergramSettings::getIsPaid()
{
	//TODO: bettergram: ask server and get know if the instance is paid or not and the current billing plan
	//TODO: bettergram: if the application is not paid then call getAd();
}

void BettergramSettings::getAd()
{
	//TODO: bettergram: get ad from servers
}

void BettergramSettings::getCryptoPriceList()
{
	QUrl url("https://www.livecoinwatch.com/api/v1/get_crypto_price_list_top_10");

	QNetworkRequest request;
	request.setUrl(url);

	QNetworkReply *reply = _networkManager.get(request);

	connect(reply, &QNetworkReply::finished,
			this, &BettergramSettings::onGetCryptoPriceListFinished);

	connect(reply, &QNetworkReply::sslErrors,
			this, &BettergramSettings::onGetCryptoPriceListSslFailed);
}

void BettergramSettings::parseCryptoPriceList(const QByteArray &byteArray)
{
	if (byteArray.isEmpty()) {
		qWarning() << "Can not get crypto price list. Response is emtpy";
		return;
	}

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(byteArray, &parseError);

	if (!doc.isObject()) {
		qWarning() << QString("Can not get crypto price list. Response is wrong. %1 (%2). Response: %3")
					  .arg(parseError.errorString())
					  .arg(parseError.error)
					  .arg(QString::fromUtf8(byteArray));
		return;
	}

	QJsonObject json = doc.object();

	if (json.isEmpty()) {
		qWarning() << "Can not get crypto price list. Response is emtpy or wrong";
		return;
	}

	QString status = json.value("status").toString();

	if (status != "success") {
		QString errorMessage = json.value("message").toString("Unknown error");
		qWarning() << "Can not get crypto price list." << errorMessage;
		return;
	}

	double marketCap = json.value("marketCap").toDouble();
	QList<CryptoPrice> priceList;

	QJsonArray priceListJson = json.value("prices").toArray();

	for (const QJsonValue &jsonValue : priceListJson) {
		QJsonObject priceJson = jsonValue.toObject();

		if (priceJson.isEmpty()) {
			qWarning() << "Price json is empty";
			continue;
		}

		QString name = priceJson.value("name").toString();
		if (name.isEmpty()) {
			qWarning() << "Price name is empty";
			continue;
		}

		QString shortName = priceJson.value("shortName").toString();
		if (shortName.isEmpty()) {
			qWarning() << "Price short name is empty";
			continue;
		}

		QString url = priceJson.value("url").toString();
		if (url.isEmpty()) {
			qWarning() << "Price url is empty";
			continue;
		}

		double price = priceJson.value("price").toDouble();
		double changeFor24Hours = priceJson.value("24h").toDouble();
		bool isCurrentPriceGrown = priceJson.value("isGrown").toBool();

		CryptoPrice cryptoPrice(url, name, shortName, price, changeFor24Hours, isCurrentPriceGrown);
		priceList.push_back(cryptoPrice);
	}

	_cryptoPriceList->updateData(marketCap, priceList);
}

void BettergramSettings::onGetCryptoPriceListFinished()
{
	QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());

	if(reply->error() == QNetworkReply::NoError) {
		parseCryptoPriceList(reply->readAll());
	} else {
		qWarning() << QString("Can not get crypto price list. %1 (%2)")
					  .arg(reply->errorString())
					  .arg(reply->error());
	}

	reply->deleteLater();
}

void BettergramSettings::onGetCryptoPriceListSslFailed(QList<QSslError> errors)
{
	for(const QSslError &error : errors) {
		qWarning() << error.errorString();
	}
}

} // namespace Bettergrams
