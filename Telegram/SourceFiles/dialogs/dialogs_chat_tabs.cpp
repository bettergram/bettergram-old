/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/

#include "dialogs_chat_tabs.h"
#include "ui/widgets/buttons.h"
#include "styles/style_dialogs.h"

namespace Dialogs {

ChatTabs::ChatTabs(QWidget *parent) : TWidget(parent)
  ,_favoriteButton(this, st::dialogsChatTabsFavoriteButton)
  ,_groupButton(this, st::dialogsChatTabsGroupButton)
  ,_oneOnOneButton(this, st::dialogsChatTabsOneOnOneButton)
  ,_announcementButton(this, st::dialogsChatTabsAnnouncementButton) {

	_listButtons.push_back(_favoriteButton);
	_listButtons.push_back(_groupButton);
	_listButtons.push_back(_oneOnOneButton);
	_listButtons.push_back(_announcementButton);

	setGeometryToLeft(0, 0, width(), _listButtons.first()->height());
}

void ChatTabs::resizeEvent(QResizeEvent *e) {
	Q_UNUSED(e);
	updateControlsGeometry();
}

void ChatTabs::updateControlsGeometry() {
	//TODO: it would be much better to use QRowLayout for this

	// Align all buttons at the center

	// We assume that all buttons have the same width
	int horizontalSpacing = (width() - _listButtons.size() * _listButtons.first()->width()) / 5;
	int x = horizontalSpacing;
	int y = 0;

	for (Ui::IconButton *button : _listButtons) {
		button->moveToLeft(x, y);
		x += button->width();
		x += horizontalSpacing;
	}
}

} // namespace Dialogs
