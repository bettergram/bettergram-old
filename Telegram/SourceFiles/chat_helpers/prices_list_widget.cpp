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

#include <QMouseEvent>
#include <QDebug>

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

	setMouseTracking(true);
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

void PricesListWidget::setSelectedRow(int selectedRow)
{
	if (_selectedRow != selectedRow) {
		_selectedRow = selectedRow;

		update();
	}
}

int PricesListWidget::getTableTop() const
{
	return _marketCap->y() + _marketCap->height() + st::pricesPanPadding;
}

int PricesListWidget::getTableBottom() const
{
	return getTableContentTop() + getTableContentHeight();
}

int PricesListWidget::getTableContentTop() const
{
	return getTableTop() + st::pricesPanTableHeaderHeight;
}

int PricesListWidget::getTableContentHeight() const
{
	return st::pricesPanTableRowHeight * BettergramSettings::instance()->cryptoPriceList()->count();
}

int PricesListWidget::getRowTop(int row) const
{
	return getTableContentTop() + row * st::pricesPanTableRowHeight;
}

QRect PricesListWidget::getTableRectangle() const
{
	return QRect(st::pricesPanPadding,
				 getTableTop(),
				 width() - 2 * st::pricesPanPadding,
				 getTableBottom());
}

QRect PricesListWidget::getTableHeaderRectangle() const
{
	return QRect(st::pricesPanPadding,
				 getTableTop(),
				 width() - 2 * st::pricesPanPadding,
				 st::pricesPanTableHeaderHeight);
}

QRect PricesListWidget::getTableContentRectangle() const
{
	return QRect(st::pricesPanPadding,
				 getTableContentTop(),
				 width() - 2 * st::pricesPanPadding,
				 getTableContentHeight());
}

QRect PricesListWidget::getRowRectangle(int row) const
{
	return QRect(st::pricesPanPadding,
				 getRowTop(row),
				 width() - 2 * st::pricesPanPadding,
				 st::pricesPanTableRowHeight);
}

void PricesListWidget::countSelectedRow(const QPoint &point)
{
	if (_selectedRow == -1) {
		if (!getTableContentRectangle().contains(point)) {
			// Nothing changed
			return;
		}
	} else if (getRowRectangle(_selectedRow).contains(point)) {
		// Nothing changed
		return;
	}

	if (!getTableContentRectangle().contains(point)) {
		setSelectedRow(-1);
		return;
	}

	int rowCount = BettergramSettings::instance()->cryptoPriceList()->count();

	for (int row = 0; row < rowCount; row++) {
		if (getRowRectangle(row).contains(point)) {
			setSelectedRow(row);
			return;
		}
	}

	setSelectedRow(-1);
}

TabbedSelector::InnerFooter* PricesListWidget::getFooter() const
{
	return _footer;
}

int PricesListWidget::countDesiredHeight(int newWidth)
{
	Q_UNUSED(newWidth);

	return getTableBottom();
}

void PricesListWidget::mousePressEvent(QMouseEvent *e)
{
	QPointF point = e->localPos();

	countSelectedRow(QPoint(static_cast<int>(qRound(point.x())),
							static_cast<int>(qRound(point.y()))));

	_pressedRow = _selectedRow;
}

void PricesListWidget::mouseReleaseEvent(QMouseEvent *e)
{
	QPointF point = e->localPos();

	countSelectedRow(QPoint(static_cast<int>(qRound(point.x())),
							static_cast<int>(qRound(point.y()))));

	if (_pressedRow != -1 && _pressedRow == _selectedRow) {
		//TODO: bettergram: handle click event to _pressedRow
	}
}

void PricesListWidget::mouseMoveEvent(QMouseEvent *e)
{
	QPointF point = e->localPos();

	countSelectedRow(QPoint(static_cast<int>(qRound(point.x())),
							static_cast<int>(qRound(point.y()))));
}

void PricesListWidget::leaveEventHook(QEvent *event)
{
	setSelectedRow(-1);
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

	int columnCoinWidth = width() - st::pricesPanColumnPriceWidth - st::pricesPanColumn24hWidth - 2 * textLeftPadding;

	int columnCoinLeft = textLeftPadding;
	int columnPriceLeft = textLeftPadding + columnCoinWidth;
	int column24hLeft = width() - st::pricesPanColumn24hWidth - textLeftPadding;

	QRect headerRect(0, top, width(), st::pricesPanTableHeaderHeight);

	painter.fillRect(headerRect, st::pricesPanTableHeaderBg);

	// The App::roundRect() method has artefacts if color is different from st::pricesPanHover
	//App::roundRect(painter, headerRect, st::pricesPanHover, StickerHoverCorners);

	painter.setFont(st::semiboldFont);

	painter.drawText(columnCoinLeft, headerRect.top(), columnCoinWidth, headerRect.height(),
					 Qt::AlignLeft | Qt::AlignVCenter, lang(lng_prices_header_coin));

	painter.drawText(columnPriceLeft, headerRect.top(), st::pricesPanColumnPriceWidth, headerRect.height(),
					 Qt::AlignRight | Qt::AlignVCenter, lang(lng_prices_header_price));

	painter.drawText(column24hLeft, headerRect.top(), st::pricesPanColumn24hWidth, headerRect.height(),
					 Qt::AlignRight | Qt::AlignVCenter, lang(lng_prices_header_24h));

	top += st::pricesPanTableHeaderHeight;

	// Draw rows

	int columnCoinTextLeft = columnCoinLeft + st::pricesPanTableImageSize + textLeftPadding;
	int rowCount = BettergramSettings::instance()->cryptoPriceList()->count();

	if (_selectedRow != -1 && _selectedRow < rowCount) {
		QRect rowRectangle(0, getRowTop(_selectedRow), width(), st::pricesPanTableRowHeight);
		App::roundRect(painter, rowRectangle, st::pricesPanHover, StickerHoverCorners);
	}

	for (const CryptoPrice *price : *BettergramSettings::instance()->cryptoPriceList()) {
		//TODO: bettergram: draw cryptocurrency icon

		painter.setPen(st::pricesPanTableCryptoNameFg);

		painter.drawText(columnCoinTextLeft, top, columnCoinWidth, st::pricesPanTableRowHeight / 2,
						 Qt::AlignLeft | Qt::AlignBottom, price->name());

		painter.setPen(st::pricesPanTableCryptoShortNameFg);

		painter.drawText(columnCoinTextLeft, top + st::pricesPanTableRowHeight / 2, columnCoinWidth, st::pricesPanTableRowHeight / 2,
						 Qt::AlignLeft | Qt::AlignTop, price->shortName());

		if (price->isCurrentPriceGrown()) {
			painter.setPen(st::pricesPanTableUpFg);
		} else {
			painter.setPen(st::pricesPanTableDownFg);
		}

		painter.drawText(columnPriceLeft, top, st::pricesPanColumnPriceWidth, st::pricesPanTableRowHeight,
						 Qt::AlignRight | Qt::AlignVCenter, price->currentPriceString());

		if (price->isChangeFor24HoursGrown()) {
			painter.setPen(st::pricesPanTableUpFg);
		} else {
			painter.setPen(st::pricesPanTableDownFg);
		}

		painter.drawText(column24hLeft, top, st::pricesPanColumn24hWidth, st::pricesPanTableRowHeight,
						 Qt::AlignRight | Qt::AlignVCenter, price->changeFor24HoursString());

		top += st::pricesPanTableRowHeight;
	}

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
