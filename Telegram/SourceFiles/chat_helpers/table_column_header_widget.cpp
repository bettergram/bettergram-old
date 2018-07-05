#include "table_column_header_widget.h"

#include "ui/widgets/buttons.h"
#include "styles/style_chat_helpers.h"
#include <lang/lang_keys.h>

#include <QMouseEvent>

namespace ChatHelpers {

TableColumnHeaderWidget::TableColumnHeaderWidget(QWidget* parent)
	: TWidget(parent)
{
	_ascendingButton = new Ui::IconButton(this, st::tableColumnSortAscending);
	_ascendingButton->setClickedCallback([this] { onAscendingButtonClicked(); });

	_descendingButton = new Ui::IconButton(this, st::tableColumnSortDescending);
	_descendingButton->setClickedCallback([this] { onDescendingButtonClicked(); });

	updateControlsGeometry();

	setCursor(style::cur_pointer);
}

LangKey TableColumnHeaderWidget::text() const
{
	return _text;
}

void TableColumnHeaderWidget::setText(const LangKey &text)
{
	if (_text != text) {
		_text = text;
		update();
	}
}

int TableColumnHeaderWidget::textFlags() const
{
	return _textFlags;
}

void TableColumnHeaderWidget::setTextFlags(int textFlags)
{
	if (_textFlags != textFlags) {
		_textFlags = textFlags;
		update();
	}
}

TableColumnHeaderWidget::SortOrder TableColumnHeaderWidget::sortOrder() const
{
	return _sortOrder;
}

void TableColumnHeaderWidget::setSortOrder(const SortOrder &sortOrder)
{
	if (_sortOrder != sortOrder) {
		_sortOrder = sortOrder;

		switch (_sortOrder) {
		case SortOrder::None:
			_ascendingButton->setIconOverride(nullptr);
			_descendingButton->setIconOverride(nullptr);
			break;
		case SortOrder::Ascending:
			_ascendingButton->setIconOverride(&st::tableColumnSortAscending.iconOver);
			_descendingButton->setIconOverride(nullptr);
			break;
		case SortOrder::Descending:
			_ascendingButton->setIconOverride(nullptr);
			_descendingButton->setIconOverride(&st::tableColumnSortDescending.iconOver);
			break;
		default:
			break;
		}

		emit sortOrderChanged();
	}
}

void TableColumnHeaderWidget::resetSortOrder()
{
	setSortOrder(SortOrder::None);
}

void TableColumnHeaderWidget::mousePressEvent(QMouseEvent *e)
{
	_isPressed = true;
}

void TableColumnHeaderWidget::mouseReleaseEvent(QMouseEvent *e)
{
	if (_isPressed) {
		_isPressed = false;

		switch (_sortOrder) {
		case SortOrder::None:
			setSortOrder(SortOrder::Ascending);
			break;
		case SortOrder::Ascending:
			setSortOrder(SortOrder::Descending);
			break;
		case SortOrder::Descending:
			setSortOrder(SortOrder::None);
			break;
		default:
			break;
		}
	}
}

void TableColumnHeaderWidget::leaveEventHook(QEvent *e)
{
	_isPressed = false;
}

void TableColumnHeaderWidget::paintEvent(QPaintEvent *event)
{
	Painter painter(this);
	QRect r = event ? event->rect() : rect();

	if (r != rect()) {
		painter.setClipRect(r);
	}

	painter.fillRect(r, st::tableHeaderBg);

	painter.setFont(st::semiboldFont);

	QMargins margins = contentsMargins();

	painter.setPen(st::tableHeaderFg);
	painter.drawText(margins.left(),
					 margins.top(),
					 width() - margins.left() - 2 * margins.right() - _ascendingButton->width(),
					 height() - margins.top() - margins.bottom(),
					 _textFlags,
					 lang(_text));
}

void TableColumnHeaderWidget::resizeEvent(QResizeEvent *e)
{
	updateControlsGeometry();
}

void TableColumnHeaderWidget::onAscendingButtonClicked()
{
	setSortOrder(SortOrder::Ascending);
}

void TableColumnHeaderWidget::onDescendingButtonClicked()
{
	setSortOrder(SortOrder::Descending);
}

void TableColumnHeaderWidget::updateControlsGeometry()
{
	int rightMargin = contentsMargins().right();

	_ascendingButton->moveToLeft(width() - _ascendingButton->width() - rightMargin,
								 height() / 2 - _ascendingButton->height());

	_descendingButton->moveToLeft(width() - _descendingButton->width() - rightMargin,
								  height() / 2);
}

} // namespace ChatHelpers
