/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "dialogs/dialogs_entry.h"
#include "dialogs/dialogs_list.h"

class History;

namespace Dialogs {

class IndexedList : public QObject {
	Q_OBJECT

public:
	IndexedList(SortMode sortMode);

	RowsByLetter addToEnd(Key key);
	Row *addByName(Key key);
	void adjustByPos(const RowsByLetter &links);
	void moveToTop(Key key);

	// row must belong to this indexed list all().
	void movePinned(Row *row, int deltaSign);

	// For sortMode != SortMode::Date
	void peerNameChanged(
		not_null<PeerData*> peer,
		const base::flat_set<QChar> &oldChars);

	//For sortMode == SortMode::Date
	void peerNameChanged(
		Mode list,
		not_null<PeerData*> peer,
		const base::flat_set<QChar> &oldChars);

	void del(Key key, Row *replacedBy = nullptr);
	void clear();

	const List &all() const {
		return current();
	}
	const List &unfilteredAll() const {
		return _list;
	}
	//const List& getFilteredList(Dialogs::EntryTypes types);// { return _list; }  // TODO: this should perform the filtering and then return the filtered list

	const List *filtered(QChar ch) const {
		if (auto it = _index.find(ch); it != _index.cend()) {
			return it->second.get();
		}
		return &_empty;
	}

	bool isFilteredByType() const;

	~IndexedList();

	// Part of List interface is duplicated here for all() list.
	int size() const { return all().size(); }
	bool isEmpty() const { return all().isEmpty(); }
	bool contains(Key key) const { return all().contains(key); }
	Row *getRow(Key key) const { return all().getRow(key); }
	Row *rowAtY(int32 y, int32 h) const { return all().rowAtY(y, h); }

	using iterator = List::iterator;
	using const_iterator = List::const_iterator;
	const_iterator cbegin() const { return all().cbegin(); }
	const_iterator cend() const { return all().cend(); }
	const_iterator begin() const { return all().cbegin(); }
	const_iterator end() const { return all().cend(); }
	iterator begin() { return all().begin(); }
	iterator end() { return all().end(); }
	const_iterator cfind(Row *value) const { return all().cfind(value); }
	const_iterator find(Row *value) const { return all().cfind(value); }
	iterator find(Row *value) { return all().find(value); }
	const_iterator cfind(int y, int h) const { return all().cfind(y, h); }
	const_iterator find(int y, int h) const { return all().cfind(y, h); }
	iterator find(int y, int h) { return all().find(y, h); }
	void setFilterTypes(EntryTypes types);
	const EntryTypes& getFilterTypes() const { return _filterTypes; }

	void performFilter();

	void countUnreadMessages(int *countInFavorite, int *countInGroup, int *countInOneOnOne, int *countInAnnouncement) const;
	void markAsRead(Dialogs::EntryTypes type);

signals:
	void performFilterStarted();
	void performFilterFinished();

private:
	void adjustByName(
		Key key,
		const base::flat_set<QChar> &oldChars);
	void adjustNames(
		Mode list,
		not_null<History*> history,
		const base::flat_set<QChar> &oldChars);

	List& current();
	const List& current() const;

	void markAsRead(Row *row);

	SortMode _sortMode;
	List _list, _empty;
	std::unique_ptr<List> _pFiltered;
	base::flat_map<QChar, std::unique_ptr<List>> _index;
	Dialogs::EntryTypes	_filterTypes = Dialogs::EntryType::All;

};

} // namespace Dialogs
