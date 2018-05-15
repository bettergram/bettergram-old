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
  , _type(EntryType::None)
  ,_favoriteButton(this, st::dialogsChatTabsFavoriteButton)
  ,_groupButton(this, st::dialogsChatTabsGroupButton)
  ,_oneOnOneButton(this, st::dialogsChatTabsOneOnOneButton)
  ,_announcementButton(this, st::dialogsChatTabsAnnouncementButton) {

	_listButtons.push_back(_favoriteButton);
	_listButtons.push_back(_groupButton);
	_listButtons.push_back(_oneOnOneButton);
	_listButtons.push_back(_announcementButton);

	setGeometryToLeft(0, 0, width(), _listButtons.first()->height());

	_favoriteButton->setClickedCallback([this] { onTabClicked(EntryType::Favorite); });
	_groupButton->setClickedCallback([this] { onTabClicked(EntryType::Group); });
	_oneOnOneButton->setClickedCallback([this] { onTabClicked(EntryType::OneOnOne); });
	_announcementButton->setClickedCallback([this] { onTabClicked(EntryType::Channel | EntryType::Feed); });
}

void ChatTabs::selectTab(const EntryTypes &type)
{
	_type = type;

	// Set default icons to tab buttons
	_favoriteButton->setIconOverride(nullptr);
	_groupButton->setIconOverride(nullptr);
	_oneOnOneButton->setIconOverride(nullptr);
	_announcementButton->setIconOverride(nullptr);

	// Set highlighted icon to the current tab button

	switch (_type.value()) {
	case static_cast<unsigned>(EntryType::Favorite):
		_favoriteButton->setIconOverride(&st::dialogsChatTabsFavoriteButton.iconOver);
		break;
	case static_cast<unsigned>(EntryType::Group):
		_groupButton->setIconOverride(&st::dialogsChatTabsGroupButton.iconOver);
		break;
	case static_cast<unsigned>(EntryType::OneOnOne):
		_oneOnOneButton->setIconOverride(&st::dialogsChatTabsOneOnOneButton.iconOver);
		break;
	case static_cast<unsigned>(EntryType::Channel):
	case static_cast<unsigned>(EntryType::Feed):
	case (EntryType::Channel | EntryType::Feed).value():
		_announcementButton->setIconOverride(&st::dialogsChatTabsAnnouncementButton.iconOver);
		break;
	case static_cast<unsigned>(EntryType::None):
		break;
	default:
		DEBUG_LOG(("Can not recognize EntryType value '%1'").arg(static_cast<int>(type)));
		break;
	}
}

void ChatTabs::onTabClicked(const EntryTypes &type)
{
	// If user clicks to selected Tab twice
	// this tab becomes unselected and we show all messages without filtering

	if ((_type & type) == type) {
		_type &= ~type;
	} else {
		_type = type;
	}

	selectTab(_type);
	emit tabSelected(_type);
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
