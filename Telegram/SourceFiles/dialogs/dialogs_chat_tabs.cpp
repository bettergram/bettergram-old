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

	_favoriteButton->setClickedCallback([this] { onTabSelected(EntryType::Favorite); });
	_groupButton->setClickedCallback([this] { onTabSelected(EntryType::Group); });
	_oneOnOneButton->setClickedCallback([this] { onTabSelected(EntryType::OneOnOne); });
	_announcementButton->setClickedCallback([this] { onTabSelected(EntryType::Channel | EntryType::Feed); });
}

void ChatTabs::selectTab(EntryTypes type)
{
	// Set default icons to tab buttons
	_favoriteButton->setIconOverride(nullptr);
	_groupButton->setIconOverride(nullptr);
	_oneOnOneButton->setIconOverride(nullptr);
	_announcementButton->setIconOverride(nullptr);

	// Set highlighted icon to the current tab button

	switch ((EntryType)type.value()) {
	case EntryType::Favorite:
		_favoriteButton->setIconOverride(&st::dialogsChatTabsFavoriteButton.iconOver);
		break;
	case EntryType::Group:
		_groupButton->setIconOverride(&st::dialogsChatTabsGroupButton.iconOver);
		break;
	case EntryType::OneOnOne:
		_oneOnOneButton->setIconOverride(&st::dialogsChatTabsOneOnOneButton.iconOver);
		break;
	case EntryType::Channel:
	case EntryType::Feed:
   case (EntryType)(EntryType::Channel | EntryType::Feed).value():
		_announcementButton->setIconOverride(&st::dialogsChatTabsAnnouncementButton.iconOver);
		break;
	case EntryType::None:
		break;
	default:
		DEBUG_LOG(("Can not recognize EntryType value '%1'").arg(static_cast<int>(type)));
		break;
	}
}

void ChatTabs::onTabSelected(EntryTypes type)
{
	selectTab(type);
	emit tabSelected(type);
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
