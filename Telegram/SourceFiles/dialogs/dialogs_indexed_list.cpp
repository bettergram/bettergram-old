/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "dialogs/dialogs_indexed_list.h"

#include "auth_session.h"
#include "data/data_session.h"
#include "history/history.h"

namespace Dialogs {

IndexedList::IndexedList(SortMode sortMode)
: _sortMode(sortMode)
, _list(sortMode)
, _empty(sortMode) {
}

RowsByLetter IndexedList::addToEnd(Key key) {
	RowsByLetter result;
	if (!_list.contains(key)) {
		result.emplace(0, _list.addToEnd(key));
		for (auto ch : key.entry()->chatsListFirstLetters()) {
			auto j = _index.find(ch);
			if (j == _index.cend()) {
				j = _index.emplace(
					ch,
					std::make_unique<List>(_sortMode)).first;
			}
			result.emplace(ch, j->second->addToEnd(key));
		}
		performFilter();
	}
	return result;
}

Row *IndexedList::addByName(Key key) {
	if (const auto row = _list.getRow(key)) {
		return row;
	}

	Row *result = _list.addByName(key);
	for (auto ch : key.entry()->chatsListFirstLetters()) {
		auto j = _index.find(ch);
		if (j == _index.cend()) {
			j = _index.emplace(
				ch,
				std::make_unique<List>(_sortMode)).first;
		}
		j->second->addByName(key);
	}
	performFilter();
	return result;
}

void IndexedList::adjustByPos(const RowsByLetter &links) {
	for (auto [ch, row] : links) {
		if (ch == QChar(0)) {
			_list.adjustByPos(row);
			performFilter();
		} else {
			if (auto it = _index.find(ch); it != _index.cend()) {
				it->second->adjustByPos(row);
				performFilter();
			}
		}
	}
}

void IndexedList::moveToTop(Key key) {
	if (_list.moveToTop(key)) {
		for (auto ch : key.entry()->chatsListFirstLetters()) {
			if (auto it = _index.find(ch); it != _index.cend()) {
				it->second->moveToTop(key);
			}
		}
		performFilter();
	}
}

void IndexedList::movePinned(Row *row, int deltaSign) {
	auto swapPinnedIndexWith = find(row);
	Assert(swapPinnedIndexWith != cend());
	if (deltaSign > 0) {
		++swapPinnedIndexWith;
	} else {
		Assert(swapPinnedIndexWith != cbegin());
		--swapPinnedIndexWith;
	}
	Auth().data().reorderTwoPinnedDialogs(
		row->key(),
		(*swapPinnedIndexWith)->key());
	performFilter();
}

void IndexedList::peerNameChanged(
		not_null<PeerData*> peer,
		const base::flat_set<QChar> &oldLetters) {
	Expects(_sortMode != SortMode::Date);

	if (const auto history = App::historyLoaded(peer)) {
		if (_sortMode == SortMode::Name) {
			adjustByName(history, oldLetters);
		} else {
			adjustNames(Dialogs::Mode::All, history, oldLetters);
		}
		performFilter();
	}
}

void IndexedList::peerNameChanged(
		Mode list,
		not_null<PeerData*> peer,
		const base::flat_set<QChar> &oldLetters) {
	Expects(_sortMode == SortMode::Date);

	if (const auto history = App::historyLoaded(peer)) {
		adjustNames(list, history, oldLetters);
		performFilter();
	}
}

void IndexedList::adjustByName(
		Key key,
		const base::flat_set<QChar> &oldLetters) {
	const auto mainRow = _list.adjustByName(key);
	if (!mainRow) return;

	auto toRemove = oldLetters;
	auto toAdd = base::flat_set<QChar>();
	for (auto ch : key.entry()->chatsListFirstLetters()) {
		auto j = toRemove.find(ch);
		if (j == toRemove.cend()) {
			toAdd.insert(ch);
		} else {
			toRemove.erase(j);
			if (auto it = _index.find(ch); it != _index.cend()) {
				it->second->adjustByName(key);
			}
		}
	}
	for (auto ch : toRemove) {
		if (auto it = _index.find(ch); it != _index.cend()) {
			it->second->del(key, mainRow);
		}
	}
	if (!toAdd.empty()) {
		for (auto ch : toAdd) {
			auto j = _index.find(ch);
			if (j == _index.cend()) {
				j = _index.emplace(
					ch,
					std::make_unique<List>(_sortMode)).first;
			}
			j->second->addByName(key);
		}
	}
}

void IndexedList::adjustNames(
		Mode list,
		not_null<History*> history,
		const base::flat_set<QChar> &oldLetters) {
	const auto key = Dialogs::Key(history);
	auto mainRow = _list.getRow(key);
	if (!mainRow) return;

	auto toRemove = oldLetters;
	auto toAdd = base::flat_set<QChar>();
	for (auto ch : key.entry()->chatsListFirstLetters()) {
		auto j = toRemove.find(ch);
		if (j == toRemove.cend()) {
			toAdd.insert(ch);
		} else {
			toRemove.erase(j);
		}
	}
	for (auto ch : toRemove) {
		if (_sortMode == SortMode::Date) {
			history->removeChatListEntryByLetter(list, ch);
		}
		if (auto it = _index.find(ch); it != _index.cend()) {
			it->second->del(key, mainRow);
		}
	}
	for (auto ch : toAdd) {
		auto j = _index.find(ch);
		if (j == _index.cend()) {
			j = _index.emplace(
				ch,
				std::make_unique<List>(_sortMode)).first;
		}
		auto row = j->second->addToEnd(key);
		if (_sortMode == SortMode::Date) {
			history->addChatListEntryByLetter(list, ch, row);
		}
	}
}

void IndexedList::del(Key key, Row *replacedBy) {
	if (_list.del(key, replacedBy)) {
		for (auto ch : key.entry()->chatsListFirstLetters()) {
			if (auto it = _index.find(ch); it != _index.cend()) {
				it->second->del(key, replacedBy);
			}
		}
		performFilter();
	}
}

//const List& IndexedList::getFilteredList(Dialogs::EntryTypes types)
//{
//	using EntryTypes = Dialogs::EntryTypes;
//
//	if(types != _filterType || types == EntryType::None)
//	{
//		if((_filterType = types) == EntryType::None)
//			return _empty;
//
//		performFilter();
//	}
//
//	return *_pFiltered;
//}

void IndexedList::setFilterTypes(EntryTypes types)
{
	if(types == EntryType::None)
		types = EntryType::All;

	if(types != _filterTypes)
	{
		_filterTypes = types;
		performFilter();
	}
}

void IndexedList::performFilter()
{
	emit performFilterStarted();

	if(_filterTypes == EntryType::All)
	{
		_pFiltered.release();
		emit performFilterFinished();
		return;
	}

	_pFiltered.reset(new List(_list.getSortMode()));

	for(auto it = _list.cbegin(); it != _list.cend(); ++it)
	{
		if(((*it)->entry()->getEntryType() & _filterTypes) != EntryType::None)
			_pFiltered->addToEnd((*it)->key());
	}

	emit performFilterFinished();
}

void IndexedList::countUnreadMessages(int *countInFavorite, int *countInGroup, int *countInOneOnOne, int *countInAnnouncement) const
{
	int inFavorite = 0;
	int inGroup = 0;
	int inOneOnOne = 0;
	int inAnnouncement = 0;

	for(auto it = _list.cbegin(); it != _list.cend(); ++it) {
		const Entry *entry = (*it)->entry();
		const EntryTypes type = entry->getEntryType();

		if(entry->isFavoriteDialog()) {
			inFavorite += entry->chatListUnreadNoMutedCount();
		}

		if(type & EntryType::Group) {
			inGroup += entry->chatListUnreadNoMutedCount();
		}

		if(type & EntryType::OneOnOne) {
			inOneOnOne += entry->chatListUnreadNoMutedCount();
		}

		if(type & (EntryType::Channel | EntryType::Feed)) {
			inAnnouncement += entry->chatListUnreadNoMutedCount();
		}
	}

	*countInFavorite = inFavorite;
	*countInGroup = inGroup;
	*countInOneOnOne = inOneOnOne;
	*countInAnnouncement = inAnnouncement;
}

List& IndexedList::current()
{
	if(_filterTypes != EntryType::All && _pFiltered)
		return *_pFiltered;
	else
		return _list;
}

const List& IndexedList::current() const
{
	if(_filterTypes != EntryType::All && _pFiltered)
		return *_pFiltered;
	else
		return _list;
}

void IndexedList::clear() {
	_index.clear();
}

bool IndexedList::isFilteredByType() const
{
	return _pFiltered.get();
}

IndexedList::~IndexedList() {
	clear();
}

} // namespace Dialogs
