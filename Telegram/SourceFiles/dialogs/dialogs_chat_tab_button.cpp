/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/

#include "dialogs_chat_tab_button.h"

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

} // namespace Dialogs
