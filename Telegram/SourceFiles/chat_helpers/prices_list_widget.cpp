#include "prices_list_widget.h"

#include "bettergram/bettergramsettings.h"
#include "bettergram/cryptopricelist.h"
#include "bettergram/cryptoprice.h"

#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "lang/lang_keys.h"
#include "styles/style_window.h"
#include "core/click_handler_types.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_widgets.h"

namespace ChatHelpers {

using namespace Bettergram;

class PricesListWidget::Footer : public TabbedSelector::InnerFooter
{
public:
	Footer(not_null<PricesListWidget*> parent);

protected:
	void processPanelHideFinished() override;
	void resizeEvent(QResizeEvent* e) override;
	void customizeClick();

private:
	not_null<PricesListWidget*> _parent;
	object_ptr<Ui::LinkButton> _link;
};

PricesListWidget::Footer::Footer(not_null<PricesListWidget*> parent)
	:  InnerFooter(parent)
	, _parent(parent)
	, _link(object_ptr<Ui::LinkButton>(this, lang(lng_prices_customize_list), st::largeLinkButton))
{
	_link->setClickedCallback([this] { customizeClick(); });
}

void PricesListWidget::Footer::resizeEvent(QResizeEvent* e)
{
	_link->move(rect().center() - _link->rect().center());
}

void PricesListWidget::Footer::processPanelHideFinished()
{
}

void PricesListWidget::Footer::customizeClick()
{
	//TODO: bettergram: realize PricesListWidget::Footer::customizeClick() method
}

PricesListWidget::PricesListWidget(QWidget* parent, not_null<Window::Controller*> controller)
	: Inner(parent, controller)
{
	_siteName = new Ui::FlatLabel(this, st::pricesSiteNameLabel);
	_siteName->setRichText(textcmdLink(1, lang(lng_prices_site_name)));
	_siteName->setLink(1, std::make_shared<UrlClickHandler>(qsl("https://www.livecoinwatch.com")));

	_marketCap = new Ui::FlatLabel(this, st::pricesMarketCapLabel);
	_marketCap->setRichText(textcmdLink(1, lang(lng_prices_market_cap)
										.arg(BettergramSettings::instance()->cryptoPriceList()->marketCapString())));
	_marketCap->setLink(1, std::make_shared<UrlClickHandler>(qsl("https://www.livecoinwatch.com")));

	BettergramSettings::instance()->getCryptoPriceList();

	updateControlsGeometry();

	//TODO: bettergram: get crypto price list from servers and remove call of _cryptoPriceList->createTestData() method
	BettergramSettings::instance()->cryptoPriceList()->createTestData();
}

void PricesListWidget::refreshRecent()
{
}

void PricesListWidget::clearSelection()
{
}

object_ptr<TabbedSelector::InnerFooter> PricesListWidget::createFooter()
{
	Expects(_footer == nullptr);

	auto res = object_ptr<Footer>(this);

	_footer = res;
	return std::move(res);
}

void PricesListWidget::afterShown()
{
	if (_timerId == -1) {
		_timerId = startTimer(_timerIntervalMs);
	}

	BettergramSettings::instance()->getCryptoPriceList();
}

void PricesListWidget::beforeHiding()
{
	if (_timerId != -1) {
		killTimer(_timerId);
		_timerId = -1;
	}
}

void PricesListWidget::timerEvent(QTimerEvent *event)
{
	if (event->timerId() == _timerId) {
		BettergramSettings::instance()->getCryptoPriceList();
	}
}

TabbedSelector::InnerFooter* PricesListWidget::getFooter() const
{
	return _footer;
}

int PricesListWidget::countDesiredHeight(int newWidth)
{
	return 1;
}

void PricesListWidget::paintEvent(QPaintEvent *event) {
	Painter painter(this);
	QRect r = event ? event->rect() : rect();

	if (r != rect()) {
		painter.setClipRect(r);
	}

	painter.fillRect(r, st::pricesPanBg);

	int top = _marketCap->y() + _marketCap->height() + st::pricesPanPadding;
	int textLeftPadding = st::pricesPanPadding;

	// Draw table header

	//TODO: bettergram: move column24hWidth and columnPriceWidth to style
	int column24hWidth = 100;
	int columnPriceWidth = 100;

	//TODO: bettergram: move headerHeight to style
	int headerHeight = 50;

	QRect headerRect(0, top, width(), headerHeight);

	painter.fillRect(headerRect, st::pricesPanTableHeaderBg);

	painter.setFont(st::semiboldFont);

	painter.drawText(textLeftPadding, headerRect.top(), headerRect.width(), headerRect.height(),
					 Qt::AlignLeft | Qt::AlignVCenter, lang(lng_prices_header_coin));

	painter.drawText(0, headerRect.top(), headerRect.width() - column24hWidth - textLeftPadding, headerRect.height(),
					 Qt::AlignRight | Qt::AlignVCenter, lang(lng_prices_header_price));

	painter.drawText(0, headerRect.top(), headerRect.width() - textLeftPadding, headerRect.height(),
					 Qt::AlignRight | Qt::AlignVCenter, lang(lng_prices_header_24h));

//	QFont pricesPanSiteNameFont = QFont();
//	pricesPanSiteNameFont.setPixelSize(16);

//	painter.setFont(pricesPanSiteNameFont);

//	enumerateSections([this, &painter, r, fromColumn, toColumn](const SectionInfo &info) {
//		if (r.top() >= info.rowsBottom) {
//			return true;
//		} else if (r.top() + r.height() <= info.top) {
//			return false;
//		}
//		if (info.section > 0 && r.top() < info.rowsTop) {
//			painter.setFont(st::emojiPanHeaderFont);
//			painter.setPen(st::emojiPanHeaderFg);
//			painter.drawTextLeft(st::emojiPanHeaderLeft - st::buttonRadius, info.top + st::emojiPanHeaderTop, width(), lang(LangKey(lng_emoji_category0 + info.section)));
//		}
//		if (r.top() + r.height() > info.rowsTop) {
//			ensureLoaded(info.section);
//			auto fromRow = floorclamp(r.y() - info.rowsTop, _singleSize.height(), 0, info.rowsCount);
//			auto toRow = ceilclamp(r.y() + r.height() - info.rowsTop, _singleSize.height(), 0, info.rowsCount);
//			for (auto i = fromRow; i < toRow; ++i) {
//				for (auto j = fromColumn; j < toColumn; ++j) {
//					auto index = i * _columnCount + j;
//					if (index >= info.count) break;

//					auto selected = (!_picker->isHidden() && info.section * MatrixRowShift + index == _pickerSel) || (info.section * MatrixRowShift + index == _selected);

//					auto w = QPoint(_rowsLeft + j * _singleSize.width(), info.rowsTop + i * _singleSize.height());
//					if (selected) {
//						auto tl = w;
//						if (rtl()) tl.setX(width() - tl.x() - _singleSize.width());
//						App::roundRect(painter, QRect(tl, _singleSize), st::emojiPanHover, StickerHoverCorners);
//					}
//					auto sourceRect = QRect(_emoji[info.section][index]->x() * _esize, _emoji[info.section][index]->y() * _esize, _esize, _esize);
//					auto imageLeft = w.x() + (_singleSize.width() - (_esize / cIntRetinaFactor())) / 2;
//					auto imageTop = w.y() + (_singleSize.height() - (_esize / cIntRetinaFactor())) / 2;
//					painter.drawPixmapLeft(imageLeft, imageTop, width(), App::emojiLarge(), sourceRect);
//				}
//			}
//		}
//		return true;
//	});
}

void PricesListWidget::resizeEvent(QResizeEvent *e)
{
	updateControlsGeometry();
}

void PricesListWidget::updateControlsGeometry()
{
	_siteName->moveToLeft((width() - _siteName->width()) / 2, st::pricesPanHeader);

	_marketCap->setRichText(textcmdLink(1, lang(lng_prices_market_cap)
										.arg(BettergramSettings::instance()->cryptoPriceList()->marketCapString())));
	_marketCap->setLink(1, std::make_shared<UrlClickHandler>(qsl("https://www.livecoinwatch.com")));

	_marketCap->moveToLeft((width() - _marketCap->width()) / 2,
						   _siteName->y() + _siteName->height() + st::pricesPanPadding);
}

} // namespace ChatHelpers
