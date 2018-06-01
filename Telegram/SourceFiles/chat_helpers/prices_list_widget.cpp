#include "prices_list_widget.h"
#include "table_column_header_widget.h"

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
	void onFooterClicked();

private:
	not_null<PricesListWidget*> _parent;
	object_ptr<Ui::LinkButton> _link;
};

PricesListWidget::Footer::Footer(not_null<PricesListWidget*> parent)
	:  InnerFooter(parent)
	, _parent(parent)
	, _link(object_ptr<Ui::LinkButton>(this, lang(lng_prices_footer), st::largeLinkButton))
{
	//TODO: bettergram: if a user is paid then we must to hide the "Upgrade Now" text

	_link->setClickedCallback([this] { onFooterClicked(); });
}

void PricesListWidget::Footer::resizeEvent(QResizeEvent* e)
{
	_link->move(rect().center() - _link->rect().center());
}

void PricesListWidget::Footer::processPanelHideFinished()
{
}

void PricesListWidget::Footer::onFooterClicked()
{
	QDesktopServices::openUrl(QUrl("https://www.livecoinwatch.com"));
}

PricesListWidget::PricesListWidget(QWidget* parent, not_null<Window::Controller*> controller)
	: Inner(parent, controller)
{
	_siteName = new Ui::IconButton(this, st::pricesPanSiteNameIcon);
	_siteName->setClickedCallback([] { QDesktopServices::openUrl(QUrl("https://www.livecoinwatch.com")); });

	_marketCap = new Ui::FlatLabel(this, st::pricesPanMarketCapLabel);
	_marketCap->setRichText(textcmdLink(1, lang(lng_prices_market_cap)
										.arg(BettergramSettings::instance()->cryptoPriceList()->marketCapString())));
	_marketCap->setLink(1, std::make_shared<UrlClickHandler>(qsl("https://www.livecoinwatch.com")));

	_coinHeader = new TableColumnHeaderWidget(this);
	_coinHeader->setText(lng_prices_header_coin);
	_coinHeader->setTextFlags(Qt::AlignLeft | Qt::AlignVCenter);

	_priceHeader = new TableColumnHeaderWidget(this);
	_priceHeader->setText(lng_prices_header_price);
	_priceHeader->setTextFlags(Qt::AlignRight | Qt::AlignVCenter);

	_24hHeader = new TableColumnHeaderWidget(this);
	_24hHeader->setText(lng_prices_header_24h);
	_24hHeader->setTextFlags(Qt::AlignRight | Qt::AlignVCenter);

	connect(_coinHeader, &TableColumnHeaderWidget::sortOrderChanged,
			this, &PricesListWidget::onCoinColumnSortOrderChanged);

	connect(_priceHeader, &TableColumnHeaderWidget::sortOrderChanged,
			this, &PricesListWidget::onPriceColumnSortOrderChanged);

	connect(_24hHeader, &TableColumnHeaderWidget::sortOrderChanged,
			this, &PricesListWidget::on24hColumnSortOrderChanged);

	BettergramSettings::instance()->getCryptoPriceList();

	updateControlsGeometry();

	CryptoPriceList *priceList = BettergramSettings::instance()->cryptoPriceList();
	connect(priceList, &CryptoPriceList::sorted, this, &PricesListWidget::onPriceListSorted);

	//TODO: bettergram: get crypto price list from servers and remove call of _cryptoPriceList->createTestData() method
	priceList->createTestData();

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

		if (_selectedRow >= 0) {
			setCursor(style::cur_pointer);
		} else {
			setCursor(style::cur_default);
		}

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
	return QRect(0,
				 getTableTop(),
				 width(),
				 getTableBottom());
}

QRect PricesListWidget::getTableHeaderRectangle() const
{
	return QRect(0,
				 getTableTop(),
				 width(),
				 st::pricesPanTableHeaderHeight);
}

QRect PricesListWidget::getTableContentRectangle() const
{
	return QRect(0,
				 getTableContentTop(),
				 width(),
				 getTableContentHeight());
}

QRect PricesListWidget::getRowRectangle(int row) const
{
	return QRect(0,
				 getRowTop(row),
				 width(),
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

	CryptoPriceList *priceList = BettergramSettings::instance()->cryptoPriceList();

	if (_pressedRow >= 0 && _pressedRow < priceList->count() && _pressedRow == _selectedRow) {
		QUrl url = priceList->at(_pressedRow)->url();
		if (!url.isEmpty()) {
			QDesktopServices::openUrl(url);
		}
	}
}

void PricesListWidget::mouseMoveEvent(QMouseEvent *e)
{
	QPointF point = e->localPos();

	countSelectedRow(QPoint(static_cast<int>(qRound(point.x())),
							static_cast<int>(qRound(point.y()))));
}

void PricesListWidget::enterEventHook(QEvent *e)
{
	QPoint point = mapFromGlobal(QCursor::pos());
	countSelectedRow(point);
}

void PricesListWidget::leaveEventHook(QEvent *e)
{
	Q_UNUSED(e);

	setSelectedRow(-1);
}

void PricesListWidget::paintEvent(QPaintEvent *event) {
	Painter painter(this);
	QRect r = event ? event->rect() : rect();

	if (r != rect()) {
		painter.setClipRect(r);
	}

	painter.fillRect(r, st::pricesPanBg);

	int top = getTableContentTop();

	int columnCoinWidth = _coinHeader->width()
			- _coinHeader->contentsMargins().left()
			- _coinHeader->contentsMargins().right()
			- st::pricesPanTableImageSize
			- st::pricesPanTablePadding;

	int columnPriceWidth = _priceHeader->width()
			- _priceHeader->contentsMargins().left()
			- _priceHeader->contentsMargins().right();

	int column24hWidth = _24hHeader->width()
			- _24hHeader->contentsMargins().left()
			- _24hHeader->contentsMargins().right();

	int columnCoinLeft = _coinHeader->x() + _coinHeader->contentsMargins().left();
	int columnPriceLeft = _priceHeader->x() + _priceHeader->contentsMargins().left();
	int column24hLeft = _24hHeader->x() + _24hHeader->contentsMargins().left();

	// Draw rows

	int columnCoinTextLeft = columnCoinLeft + st::pricesPanTableImageSize + st::pricesPanTablePadding;
	int rowCount = BettergramSettings::instance()->cryptoPriceList()->count();

	if (_selectedRow != -1 && _selectedRow < rowCount) {
		QRect rowRectangle(0, getRowTop(_selectedRow), width(), st::pricesPanTableRowHeight);
		App::roundRect(painter, rowRectangle, st::pricesPanHover, StickerHoverCorners);
	}

	painter.setFont(st::semiboldFont);

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

		painter.drawText(columnPriceLeft, top, columnPriceWidth, st::pricesPanTableRowHeight,
						 Qt::AlignRight | Qt::AlignVCenter, price->currentPriceString());

		if (price->isChangeFor24HoursGrown()) {
			painter.setPen(st::pricesPanTableUpFg);
		} else {
			painter.setPen(st::pricesPanTableDownFg);
		}

		painter.drawText(column24hLeft, top, column24hWidth, st::pricesPanTableRowHeight,
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

	int columnCoinWidth = width() - st::pricesPanColumnPriceWidth - st::pricesPanColumn24hWidth;

	_coinHeader->resize(columnCoinWidth, st::pricesPanTableHeaderHeight);
	_coinHeader->setContentsMargins(st::pricesPanTablePadding, 0, st::pricesPanTablePadding, 0);

	_priceHeader->resize(st::pricesPanColumnPriceWidth, st::pricesPanTableHeaderHeight);
	_priceHeader->setContentsMargins(0, 0, st::pricesPanTablePadding, 0);

	_24hHeader->resize(st::pricesPanColumn24hWidth, st::pricesPanTableHeaderHeight);
	_24hHeader->setContentsMargins(0, 0, st::pricesPanTablePadding, 0);

	int headerTop = getTableTop();

	_coinHeader->moveToLeft(0, headerTop);
	_priceHeader->moveToLeft(_coinHeader->x() + _coinHeader->width(), headerTop);
	_24hHeader->moveToLeft(_priceHeader->x() + _priceHeader->width(), headerTop);
}

void PricesListWidget::onCoinColumnSortOrderChanged()
{
	if (_coinHeader->sortOrder() != TableColumnHeaderWidget::SortOrder::None) {
		_priceHeader->resetSortOrder();
		_24hHeader->resetSortOrder();

		CryptoPriceList::SortOrder sortOrder = CryptoPriceList::SortOrder::None;

		if (_coinHeader->sortOrder() == TableColumnHeaderWidget::SortOrder::Ascending) {
			sortOrder = CryptoPriceList::SortOrder::NameAscending;
		} else {
			sortOrder = CryptoPriceList::SortOrder::NameDescending;
		}

		BettergramSettings::instance()->cryptoPriceList()->setSortOrder(sortOrder);
	}
}

void PricesListWidget::onPriceColumnSortOrderChanged()
{
	if (_priceHeader->sortOrder() != TableColumnHeaderWidget::SortOrder::None) {
		_coinHeader->resetSortOrder();
		_24hHeader->resetSortOrder();

		CryptoPriceList::SortOrder sortOrder = CryptoPriceList::SortOrder::None;

		if (_priceHeader->sortOrder() == TableColumnHeaderWidget::SortOrder::Ascending) {
			sortOrder = CryptoPriceList::SortOrder::PriceAscending;
		} else {
			sortOrder = CryptoPriceList::SortOrder::PriceDescending;
		}

		BettergramSettings::instance()->cryptoPriceList()->setSortOrder(sortOrder);
	}
}

void PricesListWidget::on24hColumnSortOrderChanged()
{
	if (_24hHeader->sortOrder() != TableColumnHeaderWidget::SortOrder::None) {
		_coinHeader->resetSortOrder();
		_priceHeader->resetSortOrder();

		CryptoPriceList::SortOrder sortOrder = CryptoPriceList::SortOrder::None;

		if (_24hHeader->sortOrder() == TableColumnHeaderWidget::SortOrder::Ascending) {
			sortOrder = CryptoPriceList::SortOrder::ChangeFor24hAscending;
		} else {
			sortOrder = CryptoPriceList::SortOrder::ChangeFor24hDescending;
		}

		BettergramSettings::instance()->cryptoPriceList()->setSortOrder(sortOrder);
	}
}

void PricesListWidget::onPriceListSorted()
{
	update();
}

} // namespace ChatHelpers
