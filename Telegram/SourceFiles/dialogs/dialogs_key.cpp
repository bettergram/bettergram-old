/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "dialogs/dialogs_key.h"

#include "data/data_feed.h"
#include "history/history.h"

namespace Dialogs {

not_null<Entry*> Key::entry() const {
	if (const auto p = base::get_if<not_null<History*>>(&_value)) {
		return *p;
	} else if (const auto p = base::get_if<not_null<Data::Feed*>>(&_value)) {
		return *p;
	}
	Unexpected("Empty Dialogs::Key in Key::entry().");
}

History *Key::history() const {
	if (const auto p = base::get_if<not_null<History*>>(&_value)) {
		return *p;
	}
	return nullptr;
}

Data::Feed *Key::feed() const {
	if (const auto p = base::get_if<not_null<Data::Feed*>>(&_value)) {
		return *p;
	}
	return nullptr;
}

PeerData *Key::peer() const {
	if (const auto p = base::get_if<not_null<History*>>(&_value)) {
		return (*p)->peer;
	}
	return nullptr;
}

uint64 Key::id() const {
	if (const auto p = base::get_if<not_null<History*>>(&_value)) {
		return (*p)->peerId();
	} else if (const auto p = base::get_if<not_null<Data::Feed*>>(&_value)) {
		return (*p)->id();
	} else {
		return 0;
	}
}

} // namespace Dialogs
