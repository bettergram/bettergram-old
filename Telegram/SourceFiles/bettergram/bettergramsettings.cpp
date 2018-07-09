#include "bettergramsettings.h"
#include "cryptopricelist.h"
#include "cryptoprice.h"
#include "aditem.h"

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
	_cryptoPriceList(new CryptoPriceList(this)),
	_currentAd(new AdItem(this))
{
	getIsPaid();
	getNextAd(true);
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

void BettergramSettings::setBillingPlan(BillingPlan billingPlan)
{
	if (_billingPlan != billingPlan) {
		_billingPlan = billingPlan;

		emit billingPlanChanged();
		_billingPlanObservable.notify();
	}
}

CryptoPriceList *BettergramSettings::cryptoPriceList() const
{
	return _cryptoPriceList;
}

AdItem *BettergramSettings::currentAd() const
{
	return _currentAd;
}

bool BettergramSettings::isWindowActive() const
{
	return _isWindowActive;
}

void BettergramSettings::setIsWindowActive(bool isWindowActive)
{
	if (_isWindowActive != isWindowActive) {
		_isWindowActive = isWindowActive;

		if (_isWindowActiveHandler) {
			_isWindowActiveHandler();
		}
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
	//TODO: bettergram: if the application is not paid then call getNextAd();
}

void BettergramSettings::getCryptoPriceList()
{
	QUrl url("https://http-api.livecoinwatch.com/bettergram/top10");

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
		LOG(("Can not get crypto price list. Response is emtpy"));
		return;
	}

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(byteArray, &parseError);

	if (!doc.isObject()) {
		LOG(("Can not get crypto price list. Response is wrong. %1 (%2). Response: %3")
					  .arg(parseError.errorString())
					  .arg(parseError.error)
					  .arg(QString::fromUtf8(byteArray)));
		return;
	}

	QJsonObject json = doc.object();

	if (json.isEmpty()) {
		LOG(("Can not get crypto price list. Response is emtpy or wrong"));
		return;
	}

	bool success = json.value("success").toBool();

	if (!success) {
		QString errorMessage = json.value("message").toString("Unknown error");
		LOG(("Can not get crypto price list. %1").arg(errorMessage));
		return;
	}

	double marketCap = json.value("marketCap").toDouble();

	// It is optionally parameter.
	// This parameter may contain number of seconds for the next update
	// (5, 60, 90 seconds and etc.).
	int freq = qAbs(json.value("freq").toInt());

	QList<CryptoPrice> priceList;

	QJsonArray priceListJson = json.value("prices").toArray();
	int i = 0;

	for (const QJsonValue &jsonValue : priceListJson) {
		QJsonObject priceJson = jsonValue.toObject();

		if (priceJson.isEmpty()) {
			LOG(("Price json is empty"));
			continue;
		}

		QString name = priceJson.value("name").toString();
		if (name.isEmpty()) {
			LOG(("Price name is empty"));
			continue;
		}

		QString shortName = priceJson.value("code").toString();
		if (shortName.isEmpty()) {
			LOG(("Price code is empty"));
			continue;
		}

		QString url = priceJson.value("url").toString();
		if (url.isEmpty()) {
			LOG(("Price url is empty"));
			continue;
		}

		QString iconUrl = priceJson.value("iconUrl").toString();
		if (iconUrl.isEmpty()) {
			LOG(("Price icon url is empty"));
			continue;
		}

		double price = priceJson.value("price").toDouble();
		double changeFor24Hours = priceJson.value("day").toDouble();
		bool isCurrentPriceGrown = priceJson.value("isGrown").toBool();

		CryptoPrice cryptoPrice(url, iconUrl, name, shortName, price, changeFor24Hours, isCurrentPriceGrown, i);
		priceList.push_back(cryptoPrice);

		i++;
	}

	_cryptoPriceList->updateData(marketCap, freq, priceList);
}

void BettergramSettings::onGetCryptoPriceListFinished()
{
	QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());

	if(reply->error() == QNetworkReply::NoError) {
		parseCryptoPriceList(reply->readAll());
	} else {
		LOG(("Can not get crypto price list. %1 (%2)")
					  .arg(reply->errorString())
					  .arg(reply->error()));
	}

	reply->deleteLater();
}

void BettergramSettings::onGetCryptoPriceListSslFailed(QList<QSslError> errors)
{
	for(const QSslError &error : errors) {
		LOG(("%1").arg(error.errorString()));
	}
}

void BettergramSettings::getNextAd(bool reset)
{
	if(_isPaid) {
		_currentAd->clear();
		return;
	}

	QString url = "https://api.bettergram.io/v1/ads/next";

	if (!reset && !_currentAd->isEmpty()) {
		url += "?last=";
		url += _currentAd->id();
	}

	QNetworkRequest request;
	request.setUrl(url);

	QNetworkReply *reply = _networkManager.get(request);

	connect(reply, &QNetworkReply::finished,
			this, &BettergramSettings::onGetNextAdFinished);

	connect(reply, &QNetworkReply::sslErrors,
			this, &BettergramSettings::onGetNextAdSslFailed);
}

void BettergramSettings::getNextAdLater(bool reset)
{
	int delay = _currentAd->duration();

	if (delay <= 0) {
		delay = AdItem::defaultDuration();
	}

	QTimer::singleShot(delay * 1000, this, [this, reset]() {
		if (_isWindowActive) {
			_isWindowActiveHandler = nullptr;
			getNextAd(reset);
		} else {
			_isWindowActiveHandler = [this, reset]() {
				if (_isWindowActive) {
					_isWindowActiveHandler = nullptr;
					getNextAd(reset);
				}
			};
		}
	});
}

bool BettergramSettings::parseNextAd(const QByteArray &byteArray)
{
	if (byteArray.isEmpty()) {
		LOG(("Can not get next ad. Response is emtpy"));
		return false;
	}

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(byteArray, &parseError);

	if (!doc.isObject()) {
		LOG(("Can not get next ad. Response is wrong. %1 (%2). Response: %3")
					  .arg(parseError.errorString())
					  .arg(parseError.error)
					  .arg(QString::fromUtf8(byteArray)));
		return false;
	}

	QJsonObject json = doc.object();

	if (json.isEmpty()) {
		LOG(("Can not get next ad. Response is emtpy or wrong"));
		return false;
	}

	bool isSuccess = json.value("success").toBool();

	if (!isSuccess) {
		QString errorMessage = json.value("message").toString("Unknown error");
		LOG(("Can not get next ad. %1").arg(errorMessage));
		return false;
	}

	QJsonObject adJson = json.value("ad").toObject();

	if (adJson.isEmpty()) {
		LOG(("Can not get next ad. Ad json is empty"));
		return false;
	}

	QString id = adJson.value("_id").toString();
	if (id.isEmpty()) {
		LOG(("Can not get next ad. Id is empty"));
		return false;
	}

	QString text = adJson.value("text").toString();
	if (text.isEmpty()) {
		LOG(("Can not get next ad. Text is empty"));
		return false;
	}

	QString url = adJson.value("url").toString();
	if (url.isEmpty()) {
		LOG(("Can not get next ad. Url is empty"));
		return false;
	}

	int duration = adJson.value("duration").toInt(AdItem::defaultDuration());

	AdItem adItem(id, text, url, duration);

	_currentAd->update(adItem);

	 return true;
}

void BettergramSettings::onGetNextAdFinished()
{
	QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());

	if(reply->error() == QNetworkReply::NoError) {
		if (parseNextAd(reply->readAll())) {
			getNextAdLater();
		} else {
			// Try to get new ad without previous ad id
			getNextAdLater(true);
		}
	} else {
        //	LOG(("Can not get next ad item. %1 (%2)")
        //				  .arg(reply->errorString())
        //				  .arg(reply->error()));

		getNextAdLater();
	}

	reply->deleteLater();
}

void BettergramSettings::onGetNextAdSslFailed(QList<QSslError> errors)
{
	for(const QSslError &error : errors) {
		LOG(("%1").arg(error.errorString()));
	}
}

} // namespace Bettergrams
