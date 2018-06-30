/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/

#include "dialogs_chat_tab_button.h"
#include "dialogs_layout.h"

namespace Dialogs {

ChatTabButton::ChatTabButton(QWidget *parent, const style::IconButton &st) :
	Ui::IconButton(parent, st) {
}

bool ChatTabButton::selected() const
{
	return _selected;
}

void ChatTabButton::setSelected(bool selected)
{
	if (_selected != selected) {
		_selected = selected;

		if (_selected) {
			setIconOverride(&style().iconOver);
		} else {
			setIconOverride(nullptr, &style().icon);
		}
	}
}

void ChatTabButton::select()
{
	setSelected(true);
}

void ChatTabButton::unselect()
{
	setSelected(false);
}

int ChatTabButton::unreadCount() const
{
	return _unreadCount;
}

void ChatTabButton::setUnreadCount(int unreadCount)
{
	if (_unreadCount != unreadCount) {
		_unreadCount = unreadCount;

		update();
	}
}

void ChatTabButton::enterEventHook(QEvent *e)
{
	if (!_selected) {
		setIconOverride(nullptr, nullptr);
	}
	Ui::IconButton::enterEventHook(e);
}

void ChatTabButton::leaveEventHook(QEvent *e)
{
	if (!_selected) {
		setIconOverride(nullptr, nullptr);
	}
	Ui::IconButton::enterEventHook(e);
}

void ChatTabButton::paintEvent(QPaintEvent *event)
{
	Ui::IconButton::paintEvent(event);

	if (_unreadCount <= 0) {
		return;
	}

	Painter painter(this);

	QString counter = QString::number(_unreadCount);
	if (counter.size() > 4) {
		counter = qsl("..") + counter.mid(counter.size() - 3);
	}

	::Dialogs::Layout::UnreadBadgeStyle st;
	st.active = false;
	st.muted = false;

	// place the badge at horizontal center
	int unreadRight = (width() + std::max(st.size, st.font->width(counter) + 2 * st.padding)) / 2;

	// keep the bottom padding for the badge as the top padding for the button icon
	int unreadTop = height() - st.size - style().iconPosition.y();
	int unreadWidth = 0;

	::Dialogs::Layout::paintUnreadCount(painter, counter, unreadRight, unreadTop, st, &unreadWidth);
}

} // namespace Dialogs
