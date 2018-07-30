/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mainwidget.h"

#include <rpl/combine.h>
#include <rpl/merge.h>
#include <rpl/flatten_latest.h>
#include "data/data_photo.h"
#include "data/data_document.h"
#include "data/data_web_page.h"
#include "data/data_game.h"
#include "data/data_peer_values.h"
#include "data/data_drafts.h"
#include "data/data_session.h"
#include "data/data_media_types.h"
#include "data/data_feed.h"
#include "ui/special_buttons.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/shadow.h"
#include "window/section_memento.h"
#include "window/section_widget.h"
#include "window/window_connecting_widget.h"
#include "ui/widgets/dropdown_menu.h"
#include "ui/focus_persister.h"
#include "ui/resize_area.h"
#include "ui/text_options.h"
#include "ui/toast/toast.h"
#include "chat_helpers/message_field.h"
#include "chat_helpers/stickers.h"
#include "info/info_memento.h"
#include "info/info_controller.h"
#include "observer_peer.h"
#include "apiwrap.h"
#include "dialogs/dialogs_widget.h"
#include "dialogs/dialogs_key.h"
#include "history/history.h"
#include "history/history_widget.h"
#include "history/history_message.h"
#include "history/history_media.h"
#include "history/view/history_view_service_message.h"
#include "history/view/history_view_element.h"
#include "lang/lang_keys.h"
#include "lang/lang_cloud_manager.h"
#include "boxes/add_contact_box.h"
#include "storage/file_upload.h"
#include "messenger.h"
#include "application.h"
#include "mainwindow.h"
#include "inline_bots/inline_bot_layout_item.h"
#include "boxes/confirm_box.h"
#include "boxes/sticker_set_box.h"
#include "boxes/mute_settings_box.h"
#include "boxes/peer_list_controllers.h"
#include "boxes/download_path_box.h"
#include "boxes/connection_box.h"
#include "storage/localstorage.h"
#include "shortcuts.h"
#include "media/media_audio.h"
#include "media/player/media_player_panel.h"
#include "media/player/media_player_widget.h"
#include "media/player/media_player_volume_controller.h"
#include "media/player/media_player_instance.h"
#include "media/player/media_player_float.h"
#include "base/qthelp_regex.h"
#include "base/qthelp_url.h"
#include "base/flat_set.h"
#include "window/window_top_bar_wrap.h"
#include "window/notifications_manager.h"
#include "window/window_slide_animation.h"
#include "window/window_controller.h"
#include "window/themes/window_theme.h"
#include "mtproto/dc_options.h"
#include "core/file_utilities.h"
#include "core/update_checker.h"
#include "calls/calls_instance.h"
#include "calls/calls_top_bar.h"
#include "export/export_settings.h"
#include "export/view/export_view_top_bar.h"
#include "export/view/export_view_panel_controller.h"
#include "auth_session.h"
#include "storage/storage_facade.h"
#include "storage/storage_shared_media.h"
#include "storage/storage_user_photos.h"
#include "styles/style_dialogs.h"
#include "styles/style_history.h"
#include "styles/style_boxes.h"
#include "bettergram/bettergramsettings.h"

namespace {

bool IsForceLogoutNotification(const MTPDupdateServiceNotification &data) {
	return qs(data.vtype).startsWith(qstr("AUTH_KEY_DROP_"));
}

bool HasForceLogoutNotification(const MTPUpdates &updates) {
	const auto checkUpdate = [](const MTPUpdate &update) {
		if (update.type() != mtpc_updateServiceNotification) {
			return false;
		}
		return IsForceLogoutNotification(
			update.c_updateServiceNotification());
	};
	const auto checkVector = [&](const MTPVector<MTPUpdate> &list) {
		for (const auto &update : list.v) {
			if (checkUpdate(update)) {
				return true;
			}
		}
		return false;
	};
	switch (updates.type()) {
	case mtpc_updates:
		return checkVector(updates.c_updates().vupdates);
	case mtpc_updatesCombined:
		return checkVector(updates.c_updatesCombined().vupdates);
	case mtpc_updateShort:
		return checkUpdate(updates.c_updateShort().vupdate);
	}
	return false;
}

} // namespace

enum StackItemType {
	HistoryStackItem,
	SectionStackItem,
};

class StackItem {
public:
	StackItem(PeerData *peer) : _peer(peer) {
	}

	PeerData *peer() const {
		return _peer;
	}

	void setThirdSectionMemento(
		std::unique_ptr<Window::SectionMemento> &&memento);
	std::unique_ptr<Window::SectionMemento> takeThirdSectionMemento() {
		return std::move(_thirdSectionMemento);
	}

	void setThirdSectionWeak(QPointer<Window::SectionWidget> section) {
		_thirdSectionWeak = section;
	}
	QPointer<Window::SectionWidget> thirdSectionWeak() const {
		return _thirdSectionWeak;
	}

	virtual StackItemType type() const = 0;
	virtual ~StackItem() = default;

private:
	PeerData *_peer = nullptr;
	QPointer<Window::SectionWidget> _thirdSectionWeak;
	std::unique_ptr<Window::SectionMemento> _thirdSectionMemento;

};

class StackItemHistory : public StackItem {
public:
	StackItemHistory(
		not_null<History*> history,
		MsgId msgId,
		QList<MsgId> replyReturns)
	: StackItem(history->peer)
	, history(history)
	, msgId(msgId)
	, replyReturns(replyReturns) {
	}

	StackItemType type() const override {
		return HistoryStackItem;
	}

	not_null<History*> history;
	MsgId msgId;
	QList<MsgId> replyReturns;

};

class StackItemSection : public StackItem {
public:
	StackItemSection(
		std::unique_ptr<Window::SectionMemento> &&memento);

	StackItemType type() const override {
		return SectionStackItem;
	}
	Window::SectionMemento *memento() const {
		return _memento.get();
	}

private:
	std::unique_ptr<Window::SectionMemento> _memento;

};

void StackItem::setThirdSectionMemento(
		std::unique_ptr<Window::SectionMemento> &&memento) {
	_thirdSectionMemento = std::move(memento);
}

StackItemSection::StackItemSection(
	std::unique_ptr<Window::SectionMemento> &&memento)
: StackItem(nullptr)
, _memento(std::move(memento)) {
}

template <typename ToggleCallback, typename DraggedCallback>
MainWidget::Float::Float(
	QWidget *parent,
	not_null<Window::Controller*> controller,
	not_null<HistoryItem*> item,
	ToggleCallback toggle,
	DraggedCallback dragged)
: animationSide(RectPart::Right)
, column(Window::Column::Second)
, corner(RectPart::TopRight)
, widget(
	parent,
	controller,
	item,
	[this, toggle = std::move(toggle)](bool visible) {
		toggle(this, visible);
	},
	[this, dragged = std::move(dragged)](bool closed) {
		dragged(this, closed);
	}) {
}

MainWidget::MainWidget(
	QWidget *parent,
	not_null<Window::Controller*> controller)
: RpWidget(parent)
, _controller(controller)
, _dialogsWidth(st::columnMinimalWidthLeft)
, _thirdColumnWidth(st::columnMinimalWidthThird)
, _sideShadow(this)
, _dialogs(this, _controller)
, _history(this, _controller)
, _playerPlaylist(
	this,
	_controller,
	Media::Player::Panel::Layout::OnlyPlaylist)
, _playerPanel(this, _controller, Media::Player::Panel::Layout::Full) {
	Messenger::Instance().mtp()->setUpdatesHandler(rpcDone(&MainWidget::updateReceived));
	Messenger::Instance().mtp()->setGlobalFailHandler(rpcFail(&MainWidget::updateFail));

	_ptsWaiter.setRequesting(true);
	updateScrollColors();
	setupConnectingWidget();

	connect(_dialogs, SIGNAL(cancelled()), this, SLOT(dialogsCancelled()));
	connect(this, SIGNAL(dialogsUpdated()), _dialogs, SLOT(onListScroll()));
	connect(_history, SIGNAL(cancelled()), _dialogs, SLOT(activate()));
	connect(&noUpdatesTimer, SIGNAL(timeout()), this, SLOT(mtpPing()));
	connect(&_onlineTimer, SIGNAL(timeout()), this, SLOT(updateOnline()));
	connect(&_idleFinishTimer, SIGNAL(timeout()), this, SLOT(checkIdleFinish()));
	connect(&_bySeqTimer, SIGNAL(timeout()), this, SLOT(getDifference()));
	connect(&_byPtsTimer, SIGNAL(timeout()), this, SLOT(onGetDifferenceTimeByPts()));
	connect(&_byMinChannelTimer, SIGNAL(timeout()), this, SLOT(getDifference()));
	connect(&_failDifferenceTimer, SIGNAL(timeout()), this, SLOT(onGetDifferenceTimeAfterFail()));
	subscribe(Media::Player::Updated(), [this](const AudioMsgId &audioId) {
		if (audioId.type() != AudioMsgId::Type::Video) {
			handleAudioUpdate(audioId);
		}
	});
	subscribe(Auth().calls().currentCallChanged(), [this](Calls::Call *call) { setCurrentCall(call); });

	Auth().data().currentExportView(
	) | rpl::start_with_next([=](Export::View::PanelController *view) {
		setCurrentExportView(view);
	}, lifetime());

	subscribe(_controller->dialogsListFocused(), [this](bool) {
		updateDialogsWidthAnimated();
	});
	subscribe(_controller->dialogsListDisplayForced(), [this](bool) {
		updateDialogsWidthAnimated();
	});
	rpl::merge(
		Auth().settings().dialogsWidthRatioChanges()
			| rpl::map([] { return rpl::empty_value(); }),
		Auth().settings().thirdColumnWidthChanges()
			| rpl::map([] { return rpl::empty_value(); })
	) | rpl::start_with_next(
		[this] { updateControlsGeometry(); },
		lifetime());
	subscribe(_controller->floatPlayerAreaUpdated(), [this] {
		checkFloatPlayerVisibility();
	});

	using namespace rpl::mappers;
	_controller->activeChatValue(
	) | rpl::map([](Dialogs::Key key) {
		const auto peer = key.peer();
		auto canWrite = peer
			? Data::CanWriteValue(peer)
			: rpl::single(false);
		return std::move(canWrite) | rpl::map(tuple(key, _1));
	}) | rpl::flatten_latest(
	) | rpl::start_with_next([this](Dialogs::Key key, bool canWrite) {
		updateThirdColumnToCurrentChat(key, canWrite);
	}, lifetime());

	QCoreApplication::instance()->installEventFilter(this);

	connect(&_viewsIncrementTimer, SIGNAL(timeout()), this, SLOT(onViewsIncrement()));

	using Update = Window::Theme::BackgroundUpdate;
	subscribe(Window::Theme::Background(), [this](const Update &update) {
		if (update.type == Update::Type::New || update.type == Update::Type::Changed) {
			clearCachedBackground();
		}
	});
	connect(&_cacheBackgroundTimer, SIGNAL(timeout()), this, SLOT(onCacheBackground()));

	_playerPanel->setPinCallback([this] { switchToFixedPlayer(); });
	_playerPanel->setCloseCallback([this] { closeBothPlayers(); });
	subscribe(Media::Player::instance()->titleButtonOver(), [this](bool over) {
		if (over) {
			_playerPanel->showFromOther();
		} else {
			_playerPanel->hideFromOther();
		}
	});
	subscribe(Media::Player::instance()->playerWidgetOver(), [this](bool over) {
		if (over) {
			if (_playerPlaylist->isHidden()) {
				auto position = mapFromGlobal(QCursor::pos()).x();
				auto bestPosition = _playerPlaylist->bestPositionFor(position);
				if (rtl()) bestPosition = position + 2 * (position - bestPosition) - _playerPlaylist->width();
				updateMediaPlaylistPosition(bestPosition);
			}
			_playerPlaylist->showFromOther();
		} else {
			_playerPlaylist->hideFromOther();
		}
	});
	subscribe(Media::Player::instance()->tracksFinishedNotifier(), [this](AudioMsgId::Type type) {
		if (type == AudioMsgId::Type::Voice) {
			auto songState = Media::Player::mixer()->currentState(AudioMsgId::Type::Song);
			if (!songState.id || IsStoppedOrStopping(songState.state)) {
				closeBothPlayers();
			}
		}
	});
	subscribe(Media::Player::instance()->trackChangedNotifier(), [this](AudioMsgId::Type type) {
		if (type == AudioMsgId::Type::Voice) {
			checkCurrentFloatPlayer();
		}
	});

	subscribe(Adaptive::Changed(), [this]() { handleAdaptiveLayoutUpdate(); });

	_dialogs->show();
	if (Adaptive::OneColumn()) {
		_history->hide();
	} else {
		_history->show();
	}

	orderWidgets();

#ifndef TDESKTOP_DISABLE_AUTOUPDATE
	Core::UpdateChecker checker;
	checker.start();
#endif // !TDESKTOP_DISABLE_AUTOUPDATE
}

void MainWidget::setupConnectingWidget() {
	using namespace rpl::mappers;
	_connecting = Window::ConnectingWidget::CreateDefaultWidget(
		this,
		Window::AdaptiveIsOneColumn() | rpl::map(!_1));
}

void MainWidget::checkCurrentFloatPlayer() {
	const auto state = Media::Player::instance()->current(AudioMsgId::Type::Voice);
	const auto fullId = state.contextId();
	const auto last = currentFloatPlayer();
	if (last
		&& !last->widget->detached()
		&& last->widget->item()->fullId() == fullId) {
		return;
	}
	if (last) {
		last->widget->detach();
	}
	if (const auto item = App::histItemById(fullId)) {
		if (const auto media = item->media()) {
			if (const auto document = media->document()) {
				if (document->isVideoMessage()) {
					createFloatPlayer(item);
				}
			}
		}
	}
}

void MainWidget::createFloatPlayer(not_null<HistoryItem*> item) {
	_playerFloats.push_back(std::make_unique<Float>(
		this,
		_controller,
		item,
		[this](not_null<Float*> instance, bool visible) {
			instance->hiddenByWidget = !visible;
			toggleFloatPlayer(instance);
		},
		[this](not_null<Float*> instance, bool closed) {
			finishFloatPlayerDrag(instance, closed);
		}));
	currentFloatPlayer()->column = Auth().settings().floatPlayerColumn();
	currentFloatPlayer()->corner = Auth().settings().floatPlayerCorner();
	checkFloatPlayerVisibility();
}

void MainWidget::toggleFloatPlayer(not_null<Float*> instance) {
	auto visible = !instance->hiddenByHistory && !instance->hiddenByWidget && instance->widget->isReady();
	if (instance->visible != visible) {
		instance->widget->resetMouseState();
		instance->visible = visible;
		if (!instance->visibleAnimation.animating() && !instance->hiddenByDrag) {
			auto finalRect = QRect(getFloatPlayerPosition(instance), instance->widget->size());
			instance->animationSide = getFloatPlayerSide(finalRect.center());
		}
		instance->visibleAnimation.start([this, instance] {
			updateFloatPlayerPosition(instance);
		}, visible ? 0. : 1., visible ? 1. : 0., st::slideDuration, visible ? anim::easeOutCirc : anim::linear);
		updateFloatPlayerPosition(instance);
	}
}

void MainWidget::checkFloatPlayerVisibility() {
	auto instance = currentFloatPlayer();
	if (!instance) {
		return;
	}

	auto amVisible = false;
	if (auto item = instance->widget->item()) {
		Auth().data().queryItemVisibility().notify({ item, &amVisible }, true);
	}
	instance->hiddenByHistory = amVisible;
	toggleFloatPlayer(instance);
	updateFloatPlayerPosition(instance);
}

void MainWidget::updateFloatPlayerPosition(not_null<Float*> instance) {
	auto visible = instance->visibleAnimation.current(instance->visible ? 1. : 0.);
	if (visible == 0. && !instance->visible) {
		instance->widget->hide();
		if (instance->widget->detached()) {
			InvokeQueued(instance->widget, [this, instance] {
				removeFloatPlayer(instance);
			});
		}
		return;
	}

	if (!instance->widget->dragged()) {
		if (instance->widget->isHidden()) {
			instance->widget->show();
		}

		auto dragged = instance->draggedAnimation.current(1.);
		auto position = QPoint();
		if (instance->hiddenByDrag) {
			instance->widget->setOpacity(instance->widget->countOpacityByParent());
			position = getFloatPlayerHiddenPosition(instance->dragFrom, instance->widget->size(), instance->animationSide);
		} else {
			instance->widget->setOpacity(visible * visible);
			position = getFloatPlayerPosition(instance);
			if (visible < 1.) {
				auto hiddenPosition = getFloatPlayerHiddenPosition(position, instance->widget->size(), instance->animationSide);
				position.setX(anim::interpolate(hiddenPosition.x(), position.x(), visible));
				position.setY(anim::interpolate(hiddenPosition.y(), position.y(), visible));
			}
		}
		if (dragged < 1.) {
			position.setX(anim::interpolate(instance->dragFrom.x(), position.x(), dragged));
			position.setY(anim::interpolate(instance->dragFrom.y(), position.y(), dragged));
		}
		instance->widget->move(position);
	}
}

QPoint MainWidget::getFloatPlayerHiddenPosition(QPoint position, QSize size, RectPart side) const {
	switch (side) {
	case RectPart::Left: return QPoint(-size.width(), position.y());
	case RectPart::Top: return QPoint(position.x(), -size.height());
	case RectPart::Right: return QPoint(width(), position.y());
	case RectPart::Bottom: return QPoint(position.x(), height());
	}
	Unexpected("Bad side in MainWidget::getFloatPlayerHiddenPosition().");
}

QPoint MainWidget::getFloatPlayerPosition(not_null<Float*> instance) const {
	auto section = getFloatPlayerSection(instance->column);
	auto rect = section->rectForFloatPlayer();
	auto position = rect.topLeft();
	if (IsBottomCorner(instance->corner)) {
		position.setY(position.y() + rect.height() - instance->widget->height());
	}
	if (IsRightCorner(instance->corner)) {
		position.setX(position.x() + rect.width() - instance->widget->width());
	}
	return mapFromGlobal(position);
}

RectPart MainWidget::getFloatPlayerSide(QPoint center) const {
	auto left = qAbs(center.x());
	auto right = qAbs(width() - center.x());
	auto top = qAbs(center.y());
	auto bottom = qAbs(height() - center.y());
	if (left < right && left < top && left < bottom) {
		return RectPart::Left;
	} else if (right < top && right < bottom) {
		return RectPart::Right;
	} else if (top < bottom) {
		return RectPart::Top;
	}
	return RectPart::Bottom;
}

void MainWidget::removeFloatPlayer(not_null<Float*> instance) {
	auto widget = std::move(instance->widget);
	auto i = std::find_if(_playerFloats.begin(), _playerFloats.end(), [instance](auto &item) {
		return (item.get() == instance);
	});
	Assert(i != _playerFloats.end());
	_playerFloats.erase(i);

	// ~QWidget() can call HistoryInner::enterEvent() which can
	// lead to repaintHistoryItem() and we'll have an instance
	// in _playerFloats with destroyed widget. So we destroy the
	// instance first and only after that destroy the widget.
	widget.destroy();
}

Window::AbstractSectionWidget *MainWidget::getFloatPlayerSection(Window::Column column) const {
	if (Adaptive::ThreeColumn()) {
		if (column == Window::Column::First) {
			return _dialogs;
		} else if (column == Window::Column::Second
			|| !_thirdSection) {
			if (_mainSection) {
				return _mainSection;
			}
			return _history;
		}
		return _thirdSection;
	} else if (Adaptive::Normal()) {
		if (column == Window::Column::First) {
			return _dialogs;
		} else if (_mainSection) {
			return _mainSection;
		}
		return _history;
	}
	if (Adaptive::OneColumn() && selectingPeer()) {
		return _dialogs;
	} else if (_mainSection) {
		return _mainSection;
	} else if (!Adaptive::OneColumn() || _history->peer()) {
		return _history;
	}
	return _dialogs;
}

void MainWidget::updateFloatPlayerColumnCorner(QPoint center) {
	Expects(!_playerFloats.empty());
	auto size = _playerFloats.back()->widget->size();
	auto min = INT_MAX;
	auto column = Auth().settings().floatPlayerColumn();
	auto corner = Auth().settings().floatPlayerCorner();
	auto checkSection = [this, center, size, &min, &column, &corner](
			Window::AbstractSectionWidget *widget,
			Window::Column widgetColumn) {
		auto rect = mapFromGlobal(widget->rectForFloatPlayer());
		auto left = rect.x() + (size.width() / 2);
		auto right = rect.x() + rect.width() - (size.width() / 2);
		auto top = rect.y() + (size.height() / 2);
		auto bottom = rect.y() + rect.height() - (size.height() / 2);
		auto checkCorner = [&](QPoint point, RectPart checked) {
			auto distance = (point - center).manhattanLength();
			if (min > distance) {
				min = distance;
				column = widgetColumn;
				corner = checked;
			}
		};
		checkCorner({ left, top }, RectPart::TopLeft);
		checkCorner({ right, top }, RectPart::TopRight);
		checkCorner({ left, bottom }, RectPart::BottomLeft);
		checkCorner({ right, bottom }, RectPart::BottomRight);
	};

	if (Adaptive::ThreeColumn()) {
		checkSection(_dialogs, Window::Column::First);
		if (_mainSection) {
			checkSection(_mainSection, Window::Column::Second);
		} else {
			checkSection(_history, Window::Column::Second);
		}
		if (_thirdSection) {
			checkSection(_thirdSection, Window::Column::Third);
		}
	} else if (Adaptive::Normal()) {
		checkSection(_dialogs, Window::Column::First);
		if (_mainSection) {
			checkSection(_mainSection, Window::Column::Second);
		} else {
			checkSection(_history, Window::Column::Second);
		}
	} else {
		if (Adaptive::OneColumn() && selectingPeer()) {
			checkSection(_dialogs, Window::Column::First);
		} else if (_mainSection) {
			checkSection(_mainSection, Window::Column::Second);
		} else if (!Adaptive::OneColumn() || _history->peer()) {
			checkSection(_history, Window::Column::Second);
		} else {
			checkSection(_dialogs, Window::Column::First);
		}
	}
	if (Auth().settings().floatPlayerColumn() != column) {
		Auth().settings().setFloatPlayerColumn(column);
		Auth().saveSettingsDelayed();
	}
	if (Auth().settings().floatPlayerCorner() != corner) {
		Auth().settings().setFloatPlayerCorner(corner);
		Auth().saveSettingsDelayed();
	}
}

void MainWidget::finishFloatPlayerDrag(not_null<Float*> instance, bool closed) {
	instance->dragFrom = instance->widget->pos();
	auto center = instance->widget->geometry().center();
	if (closed) {
		instance->hiddenByDrag = true;
		instance->animationSide = getFloatPlayerSide(center);
	}
	updateFloatPlayerColumnCorner(center);
	instance->column = Auth().settings().floatPlayerColumn();
	instance->corner = Auth().settings().floatPlayerCorner();

	instance->draggedAnimation.finish();
	instance->draggedAnimation.start([this, instance] { updateFloatPlayerPosition(instance); }, 0., 1., st::slideDuration, anim::sineInOut);
	updateFloatPlayerPosition(instance);

	if (closed) {
		if (auto item = instance->widget->item()) {
			auto voiceData = Media::Player::instance()->current(AudioMsgId::Type::Voice);
			if (_player && voiceData.contextId() == item->fullId()) {
				_player->entity()->stopAndClose();
			}
		}
		instance->widget->detach();
	}
}

bool MainWidget::setForwardDraft(PeerId peerId, MessageIdsList &&items) {
	Expects(peerId != 0);
	const auto peer = App::peer(peerId);
	const auto error = GetErrorTextForForward(
		peer,
		Auth().data().idsToItems(items));
	if (!error.isEmpty()) {
		Ui::show(Box<InformBox>(error), LayerOption::KeepOther);
		return false;
	}

	App::history(peer)->setForwardDraft(std::move(items));
	Ui::showPeerHistory(peer, ShowAtUnreadMsgId);
	_history->cancelReply();
	return true;
}

bool MainWidget::shareUrl(
		not_null<PeerData*> peer,
		const QString &url,
		const QString &text) {
	if (!peer->canWrite()) {
		Ui::show(Box<InformBox>(lang(lng_share_cant)));
		return false;
	}
	TextWithTags textWithTags = {
		url + '\n' + text,
		TextWithTags::Tags()
	};
	MessageCursor cursor = {
		url.size() + 1,
		url.size() + 1 + text.size(),
		QFIXED_MAX
	};
	auto history = App::history(peer->id);
	history->setLocalDraft(
		std::make_unique<Data::Draft>(textWithTags, 0, cursor, false));
	history->clearEditDraft();
	if (_history->peer() == peer) {
		_history->applyDraft();
	} else {
		Ui::showPeerHistory(peer, ShowAtUnreadMsgId);
	}
	return true;
}

void MainWidget::replyToItem(not_null<HistoryItem*> item) {
	if (_history->peer() == item->history()->peer
		|| _history->peer() == item->history()->peer->migrateTo()) {
		_history->replyToMessage(item);
	}
}

bool MainWidget::onInlineSwitchChosen(const PeerId &peer, const QString &botAndQuery) {
	PeerData *p = App::peer(peer);
	if (!peer || !p->canWrite()) {
		Ui::show(Box<InformBox>(lang(lng_inline_switch_cant)));
		return false;
	}
	History *h = App::history(peer);
	TextWithTags textWithTags = { botAndQuery, TextWithTags::Tags() };
	MessageCursor cursor = { botAndQuery.size(), botAndQuery.size(), QFIXED_MAX };
	h->setLocalDraft(std::make_unique<Data::Draft>(textWithTags, 0, cursor, false));
	h->clearEditDraft();
	bool opened = _history->peer() && (_history->peer()->id == peer);
	if (opened) {
		_history->applyDraft();
	} else {
		Ui::showPeerHistory(peer, ShowAtUnreadMsgId);
	}
	return true;
}

void MainWidget::cancelForwarding(not_null<History*> history) {
	history->setForwardDraft({});
	_history->updateForwarding();
}

void MainWidget::finishForwarding(not_null<History*> history) {
	auto toForward = history->validateForwardDraft();
	if (!toForward.empty()) {
		auto options = ApiWrap::SendOptions(history);
		Auth().api().forwardMessages(std::move(toForward), options);
		cancelForwarding(history);
	}

	Auth().data().sendHistoryChangeNotifications();
	historyToDown(history);
	dialogsToUp();
}

bool MainWidget::onSendPaths(const PeerId &peerId) {
	Expects(peerId != 0);

	auto peer = App::peer(peerId);
	if (!peer->canWrite()) {
		Ui::show(Box<InformBox>(lang(lng_forward_send_files_cant)));
		return false;
	} else if (auto megagroup = peer->asMegagroup()) {
		if (megagroup->restricted(ChannelRestriction::f_send_media)) {
			Ui::show(Box<InformBox>(lang(lng_restricted_send_media)));
			return false;
		}
	}
	Ui::showPeerHistory(peer, ShowAtTheEndMsgId);
	return _history->confirmSendingFiles(cSendPaths());
}

void MainWidget::onFilesOrForwardDrop(
		const PeerId &peerId,
		const QMimeData *data) {
	Expects(peerId != 0);

	if (data->hasFormat(qsl("application/x-td-forward"))) {
		if (!setForwardDraft(peerId, Auth().data().takeMimeForwardIds())) {
			// We've already released the mouse button, so the forwarding is cancelled.
			if (_hider) {
				_hider->startHide();
				noHider(_hider);
			}
		}
	} else {
		auto peer = App::peer(peerId);
		if (!peer->canWrite()) {
			Ui::show(Box<InformBox>(lang(lng_forward_send_files_cant)));
			return;
		}
		Ui::showPeerHistory(peer, ShowAtTheEndMsgId);
		_history->confirmSendingFiles(data);
	}
}

void MainWidget::notify_botCommandsChanged(UserData *bot) {
	_history->notify_botCommandsChanged(bot);
}

void MainWidget::notify_inlineBotRequesting(bool requesting) {
	_history->notify_inlineBotRequesting(requesting);
}

void MainWidget::notify_replyMarkupUpdated(const HistoryItem *item) {
	_history->notify_replyMarkupUpdated(item);
}

void MainWidget::notify_inlineKeyboardMoved(const HistoryItem *item, int oldKeyboardTop, int newKeyboardTop) {
	_history->notify_inlineKeyboardMoved(item, oldKeyboardTop, newKeyboardTop);
}

bool MainWidget::notify_switchInlineBotButtonReceived(const QString &query, UserData *samePeerBot, MsgId samePeerReplyTo) {
	return _history->notify_switchInlineBotButtonReceived(query, samePeerBot, samePeerReplyTo);
}

void MainWidget::notify_userIsBotChanged(UserData *bot) {
	_history->notify_userIsBotChanged(bot);
}

void MainWidget::notify_migrateUpdated(PeerData *peer) {
	_history->notify_migrateUpdated(peer);
}

void MainWidget::notify_historyMuteUpdated(History *history) {
	_dialogs->notify_historyMuteUpdated(history);
}

bool MainWidget::cmd_search() {
	if (Ui::isLayerShown() || !isActiveWindow()) return false;
	if (_mainSection) {
		return _mainSection->cmd_search();
	}
	return _history->cmd_search();
}

bool MainWidget::cmd_next_chat() {
	if (Ui::isLayerShown() || !isActiveWindow()) return false;
	return _history->cmd_next_chat();
}

bool MainWidget::cmd_previous_chat() {
	if (Ui::isLayerShown() || !isActiveWindow()) return false;
	return _history->cmd_previous_chat();
}

void MainWidget::markAsRead(Dialogs::EntryTypes type)
{
	_dialogs->markAsRead(type);
}

void MainWidget::noHider(HistoryHider *destroyed) {
	if (_hider == destroyed) {
		_hider = nullptr;
		if (Adaptive::OneColumn()) {
			if (_forwardConfirm) {
				_forwardConfirm->closeBox();
				_forwardConfirm = nullptr;
			}
			if (_mainSection || (_history->peer() && _history->peer()->id)) {
				auto animationParams = ([this] {
					if (_mainSection) {
						return prepareMainSectionAnimation(_mainSection);
					}
					return prepareHistoryAnimation(_history->peer() ? _history->peer()->id : 0);
				})();
				_dialogs->hide();
				if (_mainSection) {
					_mainSection->showAnimated(Window::SlideDirection::FromRight, animationParams);
				} else {
					_history->showAnimated(Window::SlideDirection::FromRight, animationParams);
				}
				checkFloatPlayerVisibility();
			}
		} else {
			if (_forwardConfirm) {
				_forwardConfirm->deleteLater();
				_forwardConfirm = nullptr;
			}
		}
	}
}

void MainWidget::hiderLayer(object_ptr<HistoryHider> h) {
	if (Messenger::Instance().locked()) {
		return;
	}

	_hider = std::move(h);
	connect(_hider, SIGNAL(forwarded()), _dialogs, SLOT(onCancelSearch()));
	if (Adaptive::OneColumn()) {
		dialogsToUp();

		_hider->hide();
		auto animationParams = prepareDialogsAnimation();

		if (_mainSection) {
			_mainSection->hide();
		} else {
			_history->hide();
		}
		if (_dialogs->isHidden()) {
			_dialogs->show();
			updateControlsGeometry();
			_dialogs->showAnimated(Window::SlideDirection::FromLeft, animationParams);
		}
	} else {
		_hider->show();
		updateControlsGeometry();
		_dialogs->activate();
	}
	checkFloatPlayerVisibility();
}

void MainWidget::showForwardLayer(MessageIdsList &&items) {
	hiderLayer(object_ptr<HistoryHider>(this, std::move(items)));
}

void MainWidget::showSendPathsLayer() {
	hiderLayer(object_ptr<HistoryHider>(this));
}

void MainWidget::deleteLayer(FullMsgId itemId) {
	if (const auto item = App::histItemById(itemId)) {
		const auto suggestModerateActions = true;
		Ui::show(Box<DeleteMessagesBox>(item, suggestModerateActions));
	}
}

void MainWidget::cancelUploadLayer(not_null<HistoryItem*> item) {
	const auto itemId = item->fullId();
	Auth().uploader().pause(itemId);
	const auto stopUpload = [=] {
		Ui::hideLayer();
		if (const auto item = App::histItemById(itemId)) {
			const auto history = item->history();
			item->destroy();
			if (!history->lastMessageKnown()) {
				Auth().api().requestDialogEntry(history);
			}
			Auth().data().sendHistoryChangeNotifications();
		}
		Auth().uploader().unpause();
	};
	const auto continueUpload = [=] {
		Auth().uploader().unpause();
	};
	Ui::show(Box<ConfirmBox>(
		lang(lng_selected_cancel_sure_this),
		lang(lng_selected_upload_stop),
		lang(lng_continue),
		stopUpload,
		continueUpload));
}

void MainWidget::deletePhotoLayer(PhotoData *photo) {
	if (!photo) return;
	Ui::show(Box<ConfirmBox>(lang(lng_delete_photo_sure), lang(lng_box_delete), crl::guard(this, [=] {
		Ui::hideLayer();

		auto me = App::self();
		if (!me) return;

		if (me->userpicPhotoId() == photo->id) {
			Messenger::Instance().peerClearPhoto(me->id);
		} else if (photo->peer && !photo->peer->isUser() && photo->peer->userpicPhotoId() == photo->id) {
			Messenger::Instance().peerClearPhoto(photo->peer->id);
		} else {
			MTP::send(MTPphotos_DeletePhotos(MTP_vector<MTPInputPhoto>(1, MTP_inputPhoto(MTP_long(photo->id), MTP_long(photo->access)))));
			Auth().storage().remove(Storage::UserPhotosRemoveOne(me->bareId(), photo->id));
		}
	})));
}

void MainWidget::shareUrlLayer(const QString &url, const QString &text) {
	// Don't allow to insert an inline bot query by share url link.
	if (url.trimmed().startsWith('@')) {
		return;
	}
	hiderLayer(object_ptr<HistoryHider>(this, url, text));
}

void MainWidget::inlineSwitchLayer(const QString &botAndQuery) {
	hiderLayer(object_ptr<HistoryHider>(this, botAndQuery));
}

bool MainWidget::selectingPeer(bool withConfirm) const {
	return _hider ? (withConfirm ? _hider->withConfirm() : true) : false;
}

bool MainWidget::selectingPeerForInlineSwitch() {
	return selectingPeer() ? !_hider->botAndQuery().isEmpty() : false;
}

void MainWidget::offerPeer(PeerId peer) {
	Ui::hideLayer();
	if (_hider->offerPeer(peer) && Adaptive::OneColumn()) {
		_forwardConfirm = Ui::show(Box<ConfirmBox>(_hider->offeredText(), lang(lng_forward_send), crl::guard(this, [this] {
			_hider->forward();
			if (_forwardConfirm) _forwardConfirm->closeBox();
			if (_hider) _hider->offerPeer(0);
		}), crl::guard(this, [this] {
			if (_hider && _forwardConfirm) _hider->offerPeer(0);
		})));
	}
}

void MainWidget::dialogsActivate() {
	_dialogs->activate();
}

bool MainWidget::leaveChatFailed(PeerData *peer, const RPCError &error) {
	if (MTP::isDefaultHandledError(error)) return false;

	if (error.type() == qstr("USER_NOT_PARTICIPANT") || error.type() == qstr("CHAT_ID_INVALID") || error.type() == qstr("PEER_ID_INVALID")) { // left this chat already
		deleteConversation(peer);
		return true;
	}
	return false;
}

void MainWidget::deleteHistoryAfterLeave(PeerData *peer, const MTPUpdates &updates) {
	sentUpdatesReceived(updates);
	deleteConversation(peer);
}

void MainWidget::deleteHistoryPart(DeleteHistoryRequest request, const MTPmessages_AffectedHistory &result) {
	auto peer = request.peer;

	auto &d = result.c_messages_affectedHistory();
	if (peer && peer->isChannel()) {
		peer->asChannel()->ptsUpdateAndApply(d.vpts.v, d.vpts_count.v);
	} else {
		ptsUpdateAndApply(d.vpts.v, d.vpts_count.v);
	}

	auto offset = d.voffset.v;
	if (offset <= 0) {
		cRefReportSpamStatuses().remove(peer->id);
		Local::writeReportSpamStatuses();
		return;
	}

	auto flags = MTPmessages_DeleteHistory::Flags(0);
	if (request.justClearHistory) {
		flags |= MTPmessages_DeleteHistory::Flag::f_just_clear;
	}
	MTP::send(MTPmessages_DeleteHistory(MTP_flags(flags), peer->input, MTP_int(0)), rpcDone(&MainWidget::deleteHistoryPart, request));
}

void MainWidget::deleteMessages(
		not_null<PeerData*> peer,
		const QVector<MTPint> &ids,
		bool forEveryone) {
	if (const auto channel = peer->asChannel()) {
		MTP::send(
			MTPchannels_DeleteMessages(
				channel->inputChannel,
				MTP_vector<MTPint>(ids)),
			rpcDone(&MainWidget::messagesAffected, peer));
	} else {
		auto flags = MTPmessages_DeleteMessages::Flags(0);
		if (forEveryone) {
			flags |= MTPmessages_DeleteMessages::Flag::f_revoke;
		}
		MTP::send(
			MTPmessages_DeleteMessages(
				MTP_flags(flags),
				MTP_vector<MTPint>(ids)),
			rpcDone(&MainWidget::messagesAffected, peer));
	}
}

void MainWidget::deletedContact(UserData *user, const MTPcontacts_Link &result) {
	auto &d(result.c_contacts_link());
	App::feedUsers(MTP_vector<MTPUser>(1, d.vuser));
	App::feedUserLink(MTP_int(peerToUser(user->id)), d.vmy_link, d.vforeign_link);
}

void MainWidget::removeDialog(Dialogs::Key key) {
	_dialogs->removeDialog(key);
}

void MainWidget::deleteConversation(
		not_null<PeerData*> peer,
		bool deleteHistory) {
	if (_controller->activeChatCurrent().peer() == peer) {
		Ui::showChatsList();
	}
	if (const auto history = App::historyLoaded(peer->id)) {
		Auth().data().setPinnedDialog(history, false);
		removeDialog(history);
		if (const auto channel = peer->asMegagroup()) {
			channel->addFlags(MTPDchannel::Flag::f_left);
			if (const auto from = channel->mgInfo->migrateFromPtr) {
				if (const auto migrated = App::historyLoaded(from)) {
					migrated->updateChatListExistence();
				}
			}
		}
		history->clear();
		if (deleteHistory) {
			history->markFullyLoaded();
		}
	}
	if (const auto channel = peer->asChannel()) {
		channel->ptsWaitingForShortPoll(-1);
	}
	if (deleteHistory) {
		DeleteHistoryRequest request = { peer, false };
		MTP::send(
			MTPmessages_DeleteHistory(
				MTP_flags(0),
				peer->input,
				MTP_int(0)),
			rpcDone(&MainWidget::deleteHistoryPart, request));
	}
}

void MainWidget::deleteAndExit(ChatData *chat) {
	PeerData *peer = chat;
	MTP::send(MTPmessages_DeleteChatUser(chat->inputChat, App::self()->inputUser), rpcDone(&MainWidget::deleteHistoryAfterLeave, peer), rpcFail(&MainWidget::leaveChatFailed, peer));
}

void MainWidget::addParticipants(
		not_null<PeerData*> chatOrChannel,
		const std::vector<not_null<UserData*>> &users) {
	if (auto chat = chatOrChannel->asChat()) {
		for_const (auto user, users) {
			MTP::send(
				MTPmessages_AddChatUser(
					chat->inputChat,
					user->inputUser,
					MTP_int(ForwardOnAdd)),
				rpcDone(&MainWidget::sentUpdatesReceived),
				rpcFail(&MainWidget::addParticipantFail, { user, chat }),
				0,
				5);
		}
	} else if (auto channel = chatOrChannel->asChannel()) {
		QVector<MTPInputUser> inputUsers;
		inputUsers.reserve(qMin(int(users.size()), int(MaxUsersPerInvite)));
		for (auto i = users.cbegin(), e = users.cend(); i != e; ++i) {
			inputUsers.push_back((*i)->inputUser);
			if (inputUsers.size() == MaxUsersPerInvite) {
				MTP::send(
					MTPchannels_InviteToChannel(
						channel->inputChannel,
						MTP_vector<MTPInputUser>(inputUsers)),
					rpcDone(&MainWidget::inviteToChannelDone, { channel }),
					rpcFail(&MainWidget::addParticipantsFail, { channel }),
					0,
					5);
				inputUsers.clear();
			}
		}
		if (!inputUsers.isEmpty()) {
			MTP::send(
				MTPchannels_InviteToChannel(
					channel->inputChannel,
					MTP_vector<MTPInputUser>(inputUsers)),
				rpcDone(&MainWidget::inviteToChannelDone, { channel }),
				rpcFail(&MainWidget::addParticipantsFail, { channel }),
				0,
				5);
		}
	}
}

bool MainWidget::addParticipantFail(UserAndPeer data, const RPCError &error) {
	if (MTP::isDefaultHandledError(error)) return false;

	QString text = lang(lng_failed_add_participant);
	if (error.type() == qstr("USER_LEFT_CHAT")) { // trying to return a user who has left
	} else if (error.type() == qstr("USER_KICKED")) { // trying to return a user who was kicked by admin
		text = lang(lng_cant_invite_banned);
	} else if (error.type() == qstr("USER_PRIVACY_RESTRICTED")) {
		text = lang(lng_cant_invite_privacy);
	} else if (error.type() == qstr("USER_NOT_MUTUAL_CONTACT")) { // trying to return user who does not have me in contacts
		text = lang(lng_failed_add_not_mutual);
	} else if (error.type() == qstr("USER_ALREADY_PARTICIPANT") && data.user->botInfo) {
		text = lang(lng_bot_already_in_group);
	} else if (error.type() == qstr("PEER_FLOOD")) {
		text = PeerFloodErrorText((data.peer->isChat() || data.peer->isMegagroup()) ? PeerFloodType::InviteGroup : PeerFloodType::InviteChannel);
	}
	Ui::show(Box<InformBox>(text));
	return false;
}

bool MainWidget::addParticipantsFail(
		not_null<ChannelData*> channel,
		const RPCError &error) {
	if (MTP::isDefaultHandledError(error)) return false;

	QString text = lang(lng_failed_add_participant);
	if (error.type() == qstr("USER_LEFT_CHAT")) { // trying to return banned user to his group
	} else if (error.type() == qstr("USER_KICKED")) { // trying to return a user who was kicked by admin
		text = lang(lng_cant_invite_banned);
	} else if (error.type() == qstr("USER_PRIVACY_RESTRICTED")) {
		text = lang(channel->isMegagroup() ? lng_cant_invite_privacy : lng_cant_invite_privacy_channel);
	} else if (error.type() == qstr("USER_NOT_MUTUAL_CONTACT")) { // trying to return user who does not have me in contacts
		text = lang(channel->isMegagroup() ? lng_failed_add_not_mutual : lng_failed_add_not_mutual_channel);
	} else if (error.type() == qstr("PEER_FLOOD")) {
		text = PeerFloodErrorText(PeerFloodType::InviteGroup);
	}
	Ui::show(Box<InformBox>(text));
	return false;
}

bool MainWidget::sendMessageFail(const RPCError &error) {
	if (MTP::isDefaultHandledError(error)) return false;

	if (error.type() == qstr("PEER_FLOOD")) {
		Ui::show(Box<InformBox>(PeerFloodErrorText(PeerFloodType::Send)));
		return true;
	} else if (error.type() == qstr("USER_BANNED_IN_CHANNEL")) {
		const auto link = textcmdLink(
			Messenger::Instance().createInternalLinkFull(qsl("spambot")),
			lang(lng_cant_more_info));
		const auto text = lng_error_public_groups_denied(lt_more_info, link);
		Ui::show(Box<InformBox>(text));
		return true;
	}
	return false;
}

void MainWidget::onCacheBackground() {
	if (Window::Theme::Background()->tile()) {
		auto &bg = Window::Theme::Background()->pixmapForTiled();

		auto result = QImage(_willCacheFor.width() * cIntRetinaFactor(), _willCacheFor.height() * cIntRetinaFactor(), QImage::Format_RGB32);
        result.setDevicePixelRatio(cRetinaFactor());
		{
			QPainter p(&result);
			auto left = 0;
			auto top = 0;
			auto right = _willCacheFor.width();
			auto bottom = _willCacheFor.height();
			auto w = bg.width() / cRetinaFactor();
			auto h = bg.height() / cRetinaFactor();
			auto sx = 0;
			auto sy = 0;
			auto cx = qCeil(_willCacheFor.width() / w);
			auto cy = qCeil(_willCacheFor.height() / h);
			for (int i = sx; i < cx; ++i) {
				for (int j = sy; j < cy; ++j) {
					p.drawPixmap(QPointF(i * w, j * h), bg);
				}
			}
		}
		_cachedX = 0;
		_cachedY = 0;
		_cachedBackground = App::pixmapFromImageInPlace(std::move(result));
	} else {
		auto &bg = Window::Theme::Background()->pixmap();

		QRect to, from;
		Window::Theme::ComputeBackgroundRects(_willCacheFor, bg.size(), to, from);
		_cachedX = to.x();
		_cachedY = to.y();
		_cachedBackground = App::pixmapFromImageInPlace(bg.toImage().copy(from).scaled(to.width() * cIntRetinaFactor(), to.height() * cIntRetinaFactor(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
		_cachedBackground.setDevicePixelRatio(cRetinaFactor());
	}
	_cachedFor = _willCacheFor;
}

Dialogs::IndexedList *MainWidget::contactsList() {
	return _dialogs->contactsList();
}

Dialogs::IndexedList *MainWidget::dialogsList() {
	return _dialogs->dialogsList();
}

Dialogs::IndexedList *MainWidget::contactsNoDialogsList() {
	return _dialogs->contactsNoDialogsList();
}

void MainWidget::unreadCountChanged()
{
	_dialogs->unreadCountChanged();
}

TimeMs MainWidget::highlightStartTime(not_null<const HistoryItem*> item) const {
	return _history->highlightStartTime(item);
}

bool MainWidget::historyInSelectionMode() const {
	return _history->inSelectionMode();
}

void MainWidget::sendBotCommand(PeerData *peer, UserData *bot, const QString &cmd, MsgId replyTo) {
	_history->sendBotCommand(peer, bot, cmd, replyTo);
}

void MainWidget::hideSingleUseKeyboard(PeerData *peer, MsgId replyTo) {
	_history->hideSingleUseKeyboard(peer, replyTo);
}

void MainWidget::app_sendBotCallback(
		not_null<const HistoryMessageMarkupButton*> button,
		not_null<const HistoryItem*> msg,
		int row,
		int column) {
	_history->app_sendBotCallback(button, msg, row, column);
}

bool MainWidget::insertBotCommand(const QString &cmd) {
	return _history->insertBotCommand(cmd);
}

void MainWidget::searchMessages(const QString &query, Dialogs::Key inChat) {
	_dialogs->searchMessages(query, inChat);
	if (Adaptive::OneColumn()) {
		Ui::showChatsList();
	} else {
		_dialogs->activate();
	}
}

void MainWidget::itemEdited(not_null<HistoryItem*> item) {
	if (_history->peer() == item->history()->peer
		|| (_history->peer() && _history->peer() == item->history()->peer->migrateTo())) {
		_history->itemEdited(item);
	}
}

void MainWidget::checkLastUpdate(bool afterSleep) {
	auto n = getms(true);
	if (_lastUpdateTime && n > _lastUpdateTime + (afterSleep ? NoUpdatesAfterSleepTimeout : NoUpdatesTimeout)) {
		_lastUpdateTime = n;
		MTP::ping();
	}
}

void MainWidget::messagesAffected(
		not_null<PeerData*> peer,
		const MTPmessages_AffectedMessages &result) {
	const auto &data = result.c_messages_affectedMessages();
	if (const auto channel = peer->asChannel()) {
		channel->ptsUpdateAndApply(data.vpts.v, data.vpts_count.v);
	} else {
		ptsUpdateAndApply(data.vpts.v, data.vpts_count.v);
	}

	if (const auto history = App::historyLoaded(peer)) {
		if (!history->lastMessageKnown()) {
			Auth().api().requestDialogEntry(history);
		}
	}
}

void MainWidget::handleAudioUpdate(const AudioMsgId &audioId) {
	using State = Media::Player::State;
	const auto document = audioId.audio();
	auto state = Media::Player::mixer()->currentState(audioId.type());
	if (state.id == audioId && state.state == State::StoppedAtStart) {
		state.state = State::Stopped;
		Media::Player::mixer()->clearStoppedAtStart(audioId);

		auto filepath = document->filepath(DocumentData::FilePathResolveSaveFromData);
		if (!filepath.isEmpty()) {
			if (documentIsValidMediaFile(filepath)) {
				File::Launch(filepath);
			}
		}
	}

	if (state.id == audioId) {
		if (!Media::Player::IsStoppedOrStopping(state.state)) {
			createPlayer();
		}
	}

	if (const auto item = App::histItemById(audioId.contextId())) {
		Auth().data().requestItemRepaint(item);
	}
	if (const auto items = InlineBots::Layout::documentItems()) {
		if (const auto i = items->find(document); i != items->end()) {
			for (const auto item : i->second) {
				item->update();
			}
		}
	}
}

void MainWidget::switchToPanelPlayer() {
	if (_playerUsingPanel) return;
	_playerUsingPanel = true;

	_player->hide(anim::type::normal);
	_playerVolume.destroyDelayed();
	_playerPlaylist->hideIgnoringEnterEvents();

	Media::Player::instance()->usePanelPlayer().notify(true, true);
}

void MainWidget::switchToFixedPlayer() {
	if (!_playerUsingPanel) return;
	_playerUsingPanel = false;

	if (!_player) {
		createPlayer();
	} else {
		_player->show(anim::type::normal);
		if (!_playerVolume) {
			_playerVolume.create(this);
			_player->entity()->volumeWidgetCreated(_playerVolume);
			updateMediaPlayerPosition();
		}
	}

	Media::Player::instance()->usePanelPlayer().notify(false, true);
	_playerPanel->hideIgnoringEnterEvents();
}

void MainWidget::closeBothPlayers() {
	if (_playerUsingPanel) {
		_playerUsingPanel = false;
		_player.destroyDelayed();
	} else if (_player) {
		_player->hide(anim::type::normal);
	}
	_playerVolume.destroyDelayed();

	Media::Player::instance()->usePanelPlayer().notify(false, true);
	_playerPanel->hideIgnoringEnterEvents();
	_playerPlaylist->hideIgnoringEnterEvents();
	Media::Player::instance()->stop(AudioMsgId::Type::Voice);
	Media::Player::instance()->stop(AudioMsgId::Type::Song);

	Shortcuts::disableMediaShortcuts();
}

void MainWidget::createPlayer() {
	if (_playerUsingPanel) {
		return;
	}
	if (!_player) {
		_player.create(this, object_ptr<Media::Player::Widget>(this));
		rpl::merge(
			_player->heightValue() | rpl::map([] { return true; }),
			_player->shownValue()
		) | rpl::start_with_next(
			[this] { playerHeightUpdated(); },
			_player->lifetime());
		_player->entity()->setCloseCallback([this] { closeBothPlayers(); });
		_playerVolume.create(this);
		_player->entity()->volumeWidgetCreated(_playerVolume);
		orderWidgets();
		if (_a_show.animating()) {
			_player->show(anim::type::instant);
			_player->setVisible(false);
			Shortcuts::enableMediaShortcuts();
		} else {
			_player->hide(anim::type::instant);
		}
	}
	if (_player && !_player->toggled()) {
		if (!_a_show.animating()) {
			_player->show(anim::type::normal);
			_playerHeight = _contentScrollAddToY = _player->contentHeight();
			updateControlsGeometry();
			Shortcuts::enableMediaShortcuts();
		}
	}
}

void MainWidget::playerHeightUpdated() {
	if (!_player) {
		// Player could be already "destroyDelayed", but still handle events.
		return;
	}
	auto playerHeight = _player->contentHeight();
	if (playerHeight != _playerHeight) {
		_contentScrollAddToY += playerHeight - _playerHeight;
		_playerHeight = playerHeight;
		updateControlsGeometry();
	}
	if (!_playerHeight && _player->isHidden()) {
		auto state = Media::Player::mixer()->currentState(Media::Player::instance()->getActiveType());
		if (!state.id || Media::Player::IsStoppedOrStopping(state.state)) {
			_playerVolume.destroyDelayed();
			_player.destroyDelayed();
		}
	}
}

void MainWidget::setCurrentCall(Calls::Call *call) {
	_currentCall = call;
	if (_currentCall) {
		subscribe(_currentCall->stateChanged(), [this](Calls::Call::State state) {
			using State = Calls::Call::State;
			if (state == State::Established) {
				createCallTopBar();
			} else {
				destroyCallTopBar();
			}
		});
	} else {
		destroyCallTopBar();
	}
}

void MainWidget::createCallTopBar() {
	Expects(_currentCall != nullptr);

	_callTopBar.create(this, object_ptr<Calls::TopBar>(this, _currentCall));
	_callTopBar->heightValue(
	) | rpl::start_with_next([this](int value) {
		callTopBarHeightUpdated(value);
	}, lifetime());
	orderWidgets();
	if (_a_show.animating()) {
		_callTopBar->show(anim::type::instant);
		_callTopBar->setVisible(false);
	} else {
		_callTopBar->hide(anim::type::instant);
		_callTopBar->show(anim::type::normal);
		_callTopBarHeight = _contentScrollAddToY = _callTopBar->height();
		updateControlsGeometry();
	}
}

void MainWidget::destroyCallTopBar() {
	if (_callTopBar) {
		_callTopBar->hide(anim::type::normal);
	}
}

void MainWidget::callTopBarHeightUpdated(int callTopBarHeight) {
	if (!callTopBarHeight && !_currentCall) {
		_callTopBar.destroyDelayed();
	}
	if (callTopBarHeight != _callTopBarHeight) {
		_contentScrollAddToY += callTopBarHeight - _callTopBarHeight;
		_callTopBarHeight = callTopBarHeight;
		updateControlsGeometry();
	}
}

void MainWidget::setCurrentExportView(Export::View::PanelController *view) {
	_currentExportView = view;
	if (_currentExportView) {
		_currentExportView->progressState(
		) | rpl::start_with_next([=](Export::View::Content &&data) {
			if (!data.rows.empty()
				&& data.rows[0].id == Export::View::Content::kDoneId) {
				LOG(("Export Info: Destroy top bar by Done."));
				destroyExportTopBar();
			} else if (!_exportTopBar) {
				LOG(("Export Info: Create top bar by State."));
				createExportTopBar(std::move(data));
			} else {
				_exportTopBar->entity()->updateData(std::move(data));
			}
		}, _currentExportView->lifetime());
	} else {
		LOG(("Export Info: Destroy top bar by controller removal."));
		destroyExportTopBar();
	}
}

void MainWidget::createExportTopBar(Export::View::Content &&data) {
	_exportTopBar.create(
		this,
		object_ptr<Export::View::TopBar>(this, std::move(data)));
	rpl::merge(
		_exportTopBar->heightValue() | rpl::map([] { return true; }),
		_exportTopBar->shownValue()
	) | rpl::start_with_next([=] {
		exportTopBarHeightUpdated();
	}, _exportTopBar->lifetime());
	_exportTopBar->entity()->clicks(
	) | rpl::start_with_next([=] {
		if (_currentExportView) {
			_currentExportView->activatePanel();
		}
	}, _exportTopBar->lifetime());
	orderWidgets();
	if (_a_show.animating()) {
		_exportTopBar->show(anim::type::instant);
		_exportTopBar->setVisible(false);
	} else {
		_exportTopBar->hide(anim::type::instant);
		_exportTopBar->show(anim::type::normal);
		_exportTopBarHeight = _contentScrollAddToY = _exportTopBar->contentHeight();
		updateControlsGeometry();
	}
}

void MainWidget::destroyExportTopBar() {
	if (_exportTopBar) {
		_exportTopBar->hide(anim::type::normal);
	}
}

void MainWidget::exportTopBarHeightUpdated() {
	if (!_exportTopBar) {
		// Player could be already "destroyDelayed", but still handle events.
		return;
	}
	const auto exportTopBarHeight = _exportTopBar->contentHeight();
	if (exportTopBarHeight != _exportTopBarHeight) {
		_contentScrollAddToY += exportTopBarHeight - _exportTopBarHeight;
		_exportTopBarHeight = exportTopBarHeight;
		updateControlsGeometry();
	}
	if (!_exportTopBarHeight && _exportTopBar->isHidden()) {
		_exportTopBar.destroyDelayed();
	}
}

void MainWidget::documentLoadProgress(FileLoader *loader) {
	if (auto documentId = loader ? loader->objId() : 0) {
		documentLoadProgress(Auth().data().document(documentId));
	}
}

void MainWidget::documentLoadProgress(DocumentData *document) {
	if (document->loaded()) {
		document->performActionOnLoad();
	}

	Auth().data().requestDocumentViewRepaint(document);
	Auth().documentUpdated.notify(document, true);

	if (!document->loaded() && document->isAudioFile()) {
		Media::Player::instance()->documentLoadProgress(document);
	}
}

void MainWidget::documentLoadFailed(FileLoader *loader, bool started) {
	auto documentId = loader ? loader->objId() : 0;
	if (!documentId) return;

	auto document = Auth().data().document(documentId);
	if (started) {
		auto failedFileName = loader->fileName();
		Ui::show(Box<ConfirmBox>(lang(lng_download_finish_failed), crl::guard(this, [=] {
			Ui::hideLayer();
			if (document) document->save(failedFileName);
		})));
	} else {
		Ui::show(Box<ConfirmBox>(lang(lng_download_path_failed), lang(lng_download_path_settings), crl::guard(this, [=] {
			Global::SetDownloadPath(QString());
			Global::SetDownloadPathBookmark(QByteArray());
			Ui::show(Box<DownloadPathBox>());
			Global::RefDownloadPathChanged().notify();
		})));
	}

	if (document) {
		if (document->loading()) document->cancel();
		document->status = FileDownloadFailed;
	}
}

void MainWidget::inlineResultLoadProgress(FileLoader *loader) {
	//InlineBots::Result *result = InlineBots::resultFromLoader(loader);
	//if (!result) return;

	//result->loaded();

	//Ui::repaintInlineItem();
}

void MainWidget::inlineResultLoadFailed(FileLoader *loader, bool started) {
	//InlineBots::Result *result = InlineBots::resultFromLoader(loader);
	//if (!result) return;

	//result->loaded();

	//Ui::repaintInlineItem();
}

void MainWidget::onSendFileConfirm(
		const std::shared_ptr<FileLoadResult> &file) {
	_history->sendFileConfirmed(file);
}

bool MainWidget::onSendSticker(DocumentData *document) {
	return _history->onStickerSend(document);
}

void MainWidget::dialogsCancelled() {
	if (_hider) {
		_hider->startHide();
		noHider(_hider);
	}
	_history->activate();
}

void MainWidget::insertCheckedServiceNotification(const TextWithEntities &message, const MTPMessageMedia &media, int32 date) {
	auto flags = MTPDmessage::Flag::f_entities | MTPDmessage::Flag::f_from_id | MTPDmessage_ClientFlag::f_clientside_unread;
	auto sending = TextWithEntities(), left = message;
	HistoryItem *item = nullptr;
	while (TextUtilities::CutPart(sending, left, MaxMessageSize)) {
		auto localEntities = TextUtilities::EntitiesToMTP(sending.entities);
		item = App::histories().addNewMessage(
			MTP_message(
				MTP_flags(flags),
				MTP_int(clientMsgId()),
				MTP_int(ServiceUserId),
				MTP_peerUser(MTP_int(Auth().userId())),
				MTPnullFwdHeader,
				MTPint(),
				MTPint(),
				MTP_int(date),
				MTP_string(sending.text),
				media,
				MTPnullMarkup,
				localEntities,
				MTPint(),
				MTPint(),
				MTPstring(),
				MTPlong()),
			NewMessageUnread);
	}
	Auth().data().sendHistoryChangeNotifications();
}

void MainWidget::serviceHistoryDone(const MTPmessages_Messages &msgs) {
	auto handleResult = [&](auto &&result) {
		App::feedUsers(result.vusers);
		App::feedChats(result.vchats);
		App::feedMsgs(result.vmessages, NewMessageLast);
	};

	switch (msgs.type()) {
	case mtpc_messages_messages:
		handleResult(msgs.c_messages_messages());
		break;

	case mtpc_messages_messagesSlice:
		handleResult(msgs.c_messages_messagesSlice());
		break;

	case mtpc_messages_channelMessages:
		LOG(("API Error: received messages.channelMessages! (MainWidget::serviceHistoryDone)"));
		handleResult(msgs.c_messages_channelMessages());
		break;

	case mtpc_messages_messagesNotModified:
		LOG(("API Error: received messages.messagesNotModified! (MainWidget::serviceHistoryDone)"));
		break;
	}

	App::wnd()->showDelayedServiceMsgs();
}

bool MainWidget::serviceHistoryFail(const RPCError &error) {
	if (MTP::isDefaultHandledError(error)) return false;

	App::wnd()->showDelayedServiceMsgs();
	return false;
}

bool MainWidget::isIdle() const {
	return _isIdle;
}

void MainWidget::clearCachedBackground() {
	_cachedBackground = QPixmap();
	_cacheBackgroundTimer.stop();
	update();
}

QPixmap MainWidget::cachedBackground(const QRect &forRect, int &x, int &y) {
	if (!_cachedBackground.isNull() && forRect == _cachedFor) {
		x = _cachedX;
		y = _cachedY;
		return _cachedBackground;
	}
	if (_willCacheFor != forRect || !_cacheBackgroundTimer.isActive()) {
		_willCacheFor = forRect;
		_cacheBackgroundTimer.start(CacheBackgroundTimeout);
	}
	return QPixmap();
}

void MainWidget::updateScrollColors() {
	_history->updateScrollColors();
}

void MainWidget::setChatBackground(const App::WallPaper &wp) {
	_background = std::make_unique<App::WallPaper>(wp);
	_background->full->loadEvenCancelled();
	checkChatBackground();
}

bool MainWidget::chatBackgroundLoading() {
	return (_background != nullptr);
}

float64 MainWidget::chatBackgroundProgress() const {
	if (_background) {
		return _background->full->progress();
	}
	return 1.;
}

void MainWidget::checkChatBackground() {
	if (_background) {
		if (_background->full->loaded()) {
			if (_background->full->isNull()) {
				Window::Theme::Background()->setImage(Window::Theme::kDefaultBackground);
			} else if (false
				|| _background->id == Window::Theme::kInitialBackground
				|| _background->id == Window::Theme::kDefaultBackground) {
				Window::Theme::Background()->setImage(_background->id);
			} else {
				Window::Theme::Background()->setImage(_background->id, _background->full->pix().toImage());
			}
			_background = nullptr;
			QTimer::singleShot(0, this, SLOT(update()));
		}
	}
}

ImagePtr MainWidget::newBackgroundThumb() {
	return _background ? _background->thumb : ImagePtr();
}

void MainWidget::messageDataReceived(ChannelData *channel, MsgId msgId) {
	_history->messageDataReceived(channel, msgId);
}

void MainWidget::updateBotKeyboard(History *h) {
	_history->updateBotKeyboard(h);
}

void MainWidget::pushReplyReturn(not_null<HistoryItem*> item) {
	_history->pushReplyReturn(item);
}

void MainWidget::setInnerFocus() {
	if (_hider || !_history->peer()) {
		if (_hider && _hider->wasOffered()) {
			_hider->setFocus();
		} else if (!_hider && _mainSection) {
			_mainSection->setInnerFocus();
		} else if (!_hider && _thirdSection) {
			_thirdSection->setInnerFocus();
		} else {
			dialogsActivate();
		}
	} else if (_mainSection) {
		_mainSection->setInnerFocus();
	} else if (_history->peer() || !_thirdSection) {
		_history->setInnerFocus();
	} else {
		_thirdSection->setInnerFocus();
	}
}

void MainWidget::scheduleViewIncrement(HistoryItem *item) {
	PeerData *peer = item->history()->peer;
	ViewsIncrement::iterator i = _viewsIncremented.find(peer);
	if (i != _viewsIncremented.cend()) {
		if (i.value().contains(item->id)) return;
	} else {
		i = _viewsIncremented.insert(peer, ViewsIncrementMap());
	}
	i.value().insert(item->id, true);
	ViewsIncrement::iterator j = _viewsToIncrement.find(peer);
	if (j == _viewsToIncrement.cend()) {
		j = _viewsToIncrement.insert(peer, ViewsIncrementMap());
		_viewsIncrementTimer.start(SendViewsTimeout);
	}
	j.value().insert(item->id, true);
}

void MainWidget::onViewsIncrement() {
	for (ViewsIncrement::iterator i = _viewsToIncrement.begin(); i != _viewsToIncrement.cend();) {
		if (_viewsIncrementRequests.contains(i.key())) {
			++i;
			continue;
		}

		QVector<MTPint> ids;
		ids.reserve(i.value().size());
		for (ViewsIncrementMap::const_iterator j = i.value().cbegin(), end = i.value().cend(); j != end; ++j) {
			ids.push_back(MTP_int(j.key()));
		}
		auto req = MTP::send(MTPmessages_GetMessagesViews(i.key()->input, MTP_vector<MTPint>(ids), MTP_bool(true)), rpcDone(&MainWidget::viewsIncrementDone, ids), rpcFail(&MainWidget::viewsIncrementFail), 0, 5);
		_viewsIncrementRequests.insert(i.key(), req);
		i = _viewsToIncrement.erase(i);
	}
}

void MainWidget::viewsIncrementDone(QVector<MTPint> ids, const MTPVector<MTPint> &result, mtpRequestId req) {
	auto &v = result.v;
	if (ids.size() == v.size()) {
		for (ViewsIncrementRequests::iterator i = _viewsIncrementRequests.begin(); i != _viewsIncrementRequests.cend(); ++i) {
			if (i.value() == req) {
				PeerData *peer = i.key();
				ChannelId channel = peerToChannel(peer->id);
				for (int32 j = 0, l = ids.size(); j < l; ++j) {
					if (HistoryItem *item = App::histItemById(channel, ids.at(j).v)) {
						item->setViewsCount(v.at(j).v);
					}
				}
				_viewsIncrementRequests.erase(i);
				break;
			}
		}
	}
	if (!_viewsToIncrement.isEmpty() && !_viewsIncrementTimer.isActive()) {
		_viewsIncrementTimer.start(SendViewsTimeout);
	}
}

bool MainWidget::viewsIncrementFail(const RPCError &error, mtpRequestId req) {
	if (MTP::isDefaultHandledError(error)) return false;

	for (ViewsIncrementRequests::iterator i = _viewsIncrementRequests.begin(); i != _viewsIncrementRequests.cend(); ++i) {
		if (i.value() == req) {
			_viewsIncrementRequests.erase(i);
			break;
		}
	}
	if (!_viewsToIncrement.isEmpty() && !_viewsIncrementTimer.isActive()) {
		_viewsIncrementTimer.start(SendViewsTimeout);
	}
	return false;
}

void MainWidget::createDialog(Dialogs::Key key) {
	_dialogs->createDialog(key);
}

void MainWidget::choosePeer(PeerId peerId, MsgId showAtMsgId) {
	if (selectingPeer()) {
		offerPeer(peerId);
	} else {
		Ui::showPeerHistory(peerId, showAtMsgId);
	}
}

void MainWidget::clearBotStartToken(PeerData *peer) {
	if (peer && peer->isUser() && peer->asUser()->botInfo) {
		peer->asUser()->botInfo->startToken = QString();
	}
}

void MainWidget::ctrlEnterSubmitUpdated() {
	_history->updateFieldSubmitSettings();
}

void MainWidget::ui_showPeerHistory(
		PeerId peerId,
		const SectionShow &params,
		MsgId showAtMsgId) {
	if (auto peer = App::peerLoaded(peerId)) {
		if (peer->migrateTo()) {
			peer = peer->migrateTo();
			peerId = peer->id;
			if (showAtMsgId > 0) showAtMsgId = -showAtMsgId;
		}
		auto restriction = peer->restrictionReason();
		if (!restriction.isEmpty()) {
			if (params.activation != anim::activation::background) {
				Ui::show(Box<InformBox>(restriction));
			}
			return;
		}
	}

	_controller->dialogsListFocused().set(false, true);
	_a_dialogsWidth.finish();

	using Way = SectionShow::Way;
	auto way = params.way;
	bool back = (way == Way::Backward || !peerId);
	bool foundInStack = !peerId;
	if (foundInStack || (way == Way::ClearStack)) {
		for_const (auto &item, _stack) {
			clearBotStartToken(item->peer());
		}
		_stack.clear();
	} else {
		for (auto i = 0, s = int(_stack.size()); i < s; ++i) {
			if (_stack.at(i)->type() == HistoryStackItem && _stack.at(i)->peer()->id == peerId) {
				foundInStack = true;
				while (int(_stack.size()) > i + 1) {
					clearBotStartToken(_stack.back()->peer());
					_stack.pop_back();
				}
				_stack.pop_back();
				if (!back) {
					back = true;
				}
				break;
			}
		}
		if (const auto activeChat = _controller->activeChatCurrent()) {
			if (const auto peer = activeChat.peer()) {
				if (way == Way::Forward && peer->id == peerId) {
					way = Way::ClearStack;
				}
			}
		}
	}

	const auto wasActivePeer = _controller->activeChatCurrent().peer();
	if (params.activation != anim::activation::background) {
		Ui::hideSettingsAndLayer();
	}
	if (_hider) {
		_hider->startHide();
		_hider = nullptr;
	}

	auto animatedShow = [&] {
		if (_a_show.animating()
			|| Messenger::Instance().locked()
			|| (params.animated == anim::type::instant)) {
			return false;
		}
		if (!peerId) {
			if (Adaptive::OneColumn()) {
				return _dialogs->isHidden();
			} else {
				return false;
			}
		}
		if (_history->isHidden()) {
			if (!Adaptive::OneColumn() && way == Way::ClearStack) {
				return false;
			}
			return (_mainSection != nullptr)
				|| (Adaptive::OneColumn() && !_dialogs->isHidden());
		}
		if (back || way == Way::Forward) {
			return true;
		}
		return false;
	};

	auto animationParams = animatedShow() ? prepareHistoryAnimation(peerId) : Window::SectionSlideParams();

	if (!back && (way != Way::ClearStack)) {
		// This may modify the current section, for example remove its contents.
		saveSectionInStack();
	}

	if (_history->peer() && _history->peer()->id != peerId && way != Way::Forward) {
		clearBotStartToken(_history->peer());
	}
	_history->showHistory(peerId, showAtMsgId);

	auto noPeer = !_history->peer();
	auto onlyDialogs = noPeer && Adaptive::OneColumn();
	if (_mainSection) {
		_mainSection->hide();
		_mainSection->deleteLater();
		_mainSection = nullptr;
	}

	updateControlsGeometry();

	if (noPeer) {
		_controller->setActiveChatEntry(Dialogs::Key());
	}

	if (onlyDialogs) {
		_history->hide();
		if (!_a_show.animating()) {
			if (animationParams) {
				auto direction = back ? Window::SlideDirection::FromLeft : Window::SlideDirection::FromRight;
				_dialogs->showAnimated(direction, animationParams);
			} else {
				_dialogs->showFast();
			}
		}
	} else {
		const auto nowActivePeer = _controller->activeChatCurrent().peer();
		if (nowActivePeer && nowActivePeer != wasActivePeer) {
			if (const auto channel = nowActivePeer->asChannel()) {
				channel->ptsWaitingForShortPoll(
					WaitForChannelGetDifference);
			}
			_viewsIncremented.remove(nowActivePeer);
		}
		if (Adaptive::OneColumn() && !_dialogs->isHidden()) {
			_dialogs->hide();
		}
		if (!_a_show.animating()) {
			if (!animationParams.oldContentCache.isNull()) {
				_history->showAnimated(
					back
						? Window::SlideDirection::FromLeft
						: Window::SlideDirection::FromRight,
					animationParams);
			} else {
				_history->show();
				if (App::wnd()) {
					QTimer::singleShot(0, App::wnd(), SLOT(setInnerFocus()));
				}
			}
		}
	}

	if (!_dialogs->isHidden()) {
		if (!back) {
			if (const auto history = _history->history()) {
				_dialogs->scrollToPeer(history, showAtMsgId);
			}
		}
		_dialogs->update();
	}

	checkFloatPlayerVisibility();
}

PeerData *MainWidget::ui_getPeerForMouseAction() {
	return _history->ui_getPeerForMouseAction();
}

Dialogs::RowDescriptor MainWidget::chatListEntryBefore(
		const Dialogs::RowDescriptor &which) const {
	if (selectingPeer()) {
		return Dialogs::RowDescriptor();
	}
	return _dialogs->chatListEntryBefore(which);
}

Dialogs::RowDescriptor MainWidget::chatListEntryAfter(
		const Dialogs::RowDescriptor &which) const {
	if (selectingPeer()) {
		return Dialogs::RowDescriptor();
	}
	return _dialogs->chatListEntryAfter(which);
}

PeerData *MainWidget::peer() {
	return _history->peer();
}

void MainWidget::saveSectionInStack() {
	if (_mainSection) {
		if (auto memento = _mainSection->createMemento()) {
			_stack.push_back(std::make_unique<StackItemSection>(
				std::move(memento)));
			_stack.back()->setThirdSectionWeak(_thirdSection.data());
		}
	} else if (const auto history = _history->history()) {
		_stack.push_back(std::make_unique<StackItemHistory>(
			history,
			_history->msgId(),
			_history->replyReturns()));
		_stack.back()->setThirdSectionWeak(_thirdSection.data());
	}
}

void MainWidget::showSection(
		Window::SectionMemento &&memento,
		const SectionShow &params) {
	if (_mainSection && _mainSection->showInternal(
			&memento,
			params)) {
		if (const auto entry = _mainSection->activeChat(); entry.key) {
			_controller->setActiveChatEntry(entry);
		}
		return;
	//
	// Now third section handles only its own showSection() requests.
	// General showSection() should show layer or main_section instead.
	//
	//} else if (_thirdSection && _thirdSection->showInternal(
	//		&memento,
	//		params)) {
	//	return;
	}

	// If the window was not resized, but we've enabled
	// tabbedSelectorSectionEnabled or thirdSectionInfoEnabled
	// we need to update adaptive layout to Adaptive::ThirdColumn().
	updateColumnLayout();

	showNewSection(
		std::move(memento),
		params);
}

void MainWidget::updateColumnLayout() {
	updateWindowAdaptiveLayout();
}

Window::SectionSlideParams MainWidget::prepareThirdSectionAnimation(Window::SectionWidget *section) {
	Expects(_thirdSection != nullptr);

	Window::SectionSlideParams result;
	result.withTopBarShadow = section->hasTopBarShadow();
	if (!_thirdSection->hasTopBarShadow()) {
		result.withTopBarShadow = false;
	}
	for (auto &instance : _playerFloats) {
		instance->widget->hide();
	}
	auto sectionTop = getThirdSectionTop();
	result.oldContentCache = _thirdSection->grabForShowAnimation(result);
	for (auto &instance : _playerFloats) {
		if (instance->visible) {
			instance->widget->show();
		}
	}
	return result;
}

Window::SectionSlideParams MainWidget::prepareShowAnimation(
		bool willHaveTopBarShadow) {
	Window::SectionSlideParams result;
	result.withTopBarShadow = willHaveTopBarShadow;
	if (selectingPeer() && Adaptive::OneColumn()) {
		result.withTopBarShadow = false;
	} else if (_mainSection) {
		if (!_mainSection->hasTopBarShadow()) {
			result.withTopBarShadow = false;
		}
	} else if (!_history->peer()) {
		result.withTopBarShadow = false;
	}

	for (auto &instance : _playerFloats) {
		instance->widget->hide();
	}
	if (_player) {
		_player->hideShadow();
	}
	auto playerVolumeVisible = _playerVolume && !_playerVolume->isHidden();
	if (playerVolumeVisible) {
		_playerVolume->hide();
	}
	auto playerPanelVisible = !_playerPanel->isHidden();
	if (playerPanelVisible) {
		_playerPanel->hide();
	}
	auto playerPlaylistVisible = !_playerPlaylist->isHidden();
	if (playerPlaylistVisible) {
		_playerPlaylist->hide();
	}

	auto sectionTop = getMainSectionTop();
	if (selectingPeer() && Adaptive::OneColumn()) {
		result.oldContentCache = Ui::GrabWidget(this, QRect(
			0,
			sectionTop,
			_dialogsWidth,
			height() - sectionTop));
	} else if (_mainSection) {
		result.oldContentCache = _mainSection->grabForShowAnimation(result);
	} else {
		result.oldContentCache = _history->grabForShowAnimation(result);
	}

	if (playerVolumeVisible) {
		_playerVolume->show();
	}
	if (playerPanelVisible) {
		_playerPanel->show();
	}
	if (playerPlaylistVisible) {
		_playerPlaylist->show();
	}
	if (_player) {
		_player->showShadow();
	}
	for (auto &instance : _playerFloats) {
		if (instance->visible) {
			instance->widget->show();
		}
	}

	return result;
}

Window::SectionSlideParams MainWidget::prepareMainSectionAnimation(Window::SectionWidget *section) {
	return prepareShowAnimation(section->hasTopBarShadow());
}

Window::SectionSlideParams MainWidget::prepareHistoryAnimation(PeerId historyPeerId) {
	return prepareShowAnimation(historyPeerId != 0);
}

Window::SectionSlideParams MainWidget::prepareDialogsAnimation() {
	return prepareShowAnimation(false);
}

void MainWidget::showNewSection(
		Window::SectionMemento &&memento,
		const SectionShow &params) {
	using Column = Window::Column;

	auto saveInStack = (params.way == SectionShow::Way::Forward);
	auto thirdSectionTop = getThirdSectionTop();
	auto newThirdGeometry = QRect(
		width() - st::columnMinimalWidthThird,
		thirdSectionTop,
		st::columnMinimalWidthThird,
		height() - thirdSectionTop);
	auto newThirdSection = (Adaptive::ThreeColumn() && params.thirdColumn)
		? memento.createWidget(
			this,
			_controller,
			Column::Third,
			newThirdGeometry)
		: nullptr;
	if (newThirdSection) {
		saveInStack = false;
	} else if (auto layer = memento.createLayer(_controller, rect())) {
		if (params.activation != anim::activation::background) {
			Ui::hideLayer(anim::type::instant);
		}
		_controller->showSpecialLayer(std::move(layer));
		return;
	}

	if (params.activation != anim::activation::background) {
		Ui::hideSettingsAndLayer();
	}

	QPixmap animCache;

	_controller->dialogsListFocused().set(false, true);
	_a_dialogsWidth.finish();

	auto mainSectionTop = getMainSectionTop();
	auto newMainGeometry = QRect(
		_history->x(),
		mainSectionTop,
		_history->width(),
		height() - mainSectionTop);
	auto newMainSection = newThirdSection
		? nullptr
		: memento.createWidget(
			this,
			_controller,
			Adaptive::OneColumn() ? Column::First : Column::Second,
			newMainGeometry);
	Assert(newMainSection || newThirdSection);

	auto animatedShow = [&] {
		if (_a_show.animating()
			|| Messenger::Instance().locked()
			|| (params.animated == anim::type::instant)
			|| memento.instant()) {
			return false;
		}
		if (!Adaptive::OneColumn() && params.way == SectionShow::Way::ClearStack) {
			return false;
		} else if (Adaptive::OneColumn()
			|| (newThirdSection && _thirdSection)
			|| (newMainSection && isMainSectionShown())) {
			return true;
		}
		return false;
	}();
	auto animationParams = animatedShow
		? (newThirdSection
			? prepareThirdSectionAnimation(newThirdSection)
			: prepareMainSectionAnimation(newMainSection))
		: Window::SectionSlideParams();

	setFocus(); // otherwise dialogs widget could be focused.

	if (saveInStack) {
		// This may modify the current section, for example remove its contents.
		saveSectionInStack();
	}
	auto &settingSection = newThirdSection
		? _thirdSection
		: _mainSection;
	if (newThirdSection) {
		_thirdSection = std::move(newThirdSection);
		if (!_thirdShadow) {
			_thirdShadow.create(this);
			_thirdShadow->show();
			orderWidgets();
		}
		updateControlsGeometry();
	} else {
		if (_mainSection) {
			_mainSection->hide();
			_mainSection->deleteLater();
			_mainSection = nullptr;
		}
		_mainSection = std::move(newMainSection);
		updateControlsGeometry();
		_history->finishAnimating();
		_history->showHistory(0, 0);
		_history->hide();
		if (Adaptive::OneColumn()) _dialogs->hide();
	}

	if (animationParams) {
		auto back = (params.way == SectionShow::Way::Backward);
		auto direction = (back || settingSection->forceAnimateBack())
			? Window::SlideDirection::FromLeft
			: Window::SlideDirection::FromRight;
		settingSection->showAnimated(direction, animationParams);
	} else {
		settingSection->showFast();
	}

	if (settingSection.data() == _mainSection.data()) {
		if (const auto entry = _mainSection->activeChat(); entry.key) {
			_controller->setActiveChatEntry(entry);
		}
	}

	checkFloatPlayerVisibility();
	orderWidgets();
}

void MainWidget::checkMainSectionToLayer() {
	if (!_mainSection) {
		return;
	}
	Ui::FocusPersister persister(this);
	if (auto layer = _mainSection->moveContentToLayer(rect())) {
		dropMainSection(_mainSection);
		_controller->showSpecialLayer(
			std::move(layer),
			anim::type::instant);
	}
}

void MainWidget::dropMainSection(Window::SectionWidget *widget) {
	if (_mainSection != widget) {
		return;
	}
	_mainSection.destroy();
	_controller->showBackFromStack(
		SectionShow(
			anim::type::instant,
			anim::activation::background));
}

bool MainWidget::isMainSectionShown() const {
	return _mainSection || _history->peer();
}

bool MainWidget::isThirdSectionShown() const {
	return _thirdSection != nullptr;
}

bool MainWidget::stackIsEmpty() const {
	return _stack.empty();
}

void MainWidget::showBackFromStack(
		const SectionShow &params) {
	if (selectingPeer()) return;
	if (_stack.empty()) {
		_controller->clearSectionStack(params);
		if (App::wnd()) {
			QTimer::singleShot(0, App::wnd(), SLOT(setInnerFocus()));
		}
		return;
	}
	auto item = std::move(_stack.back());
	_stack.pop_back();
	if (auto currentHistoryPeer = _history->peer()) {
		clearBotStartToken(currentHistoryPeer);
	}
	_thirdSectionFromStack = item->takeThirdSectionMemento();
	if (item->type() == HistoryStackItem) {
		auto historyItem = static_cast<StackItemHistory*>(item.get());
		_controller->showPeerHistory(
			historyItem->peer()->id,
			params.withWay(SectionShow::Way::Backward),
			ShowAtUnreadMsgId);
		_history->setReplyReturns(historyItem->peer()->id, historyItem->replyReturns);
	} else if (item->type() == SectionStackItem) {
		auto sectionItem = static_cast<StackItemSection*>(item.get());
		showNewSection(
			std::move(*sectionItem->memento()),
			params.withWay(SectionShow::Way::Backward));
	}
	if (_thirdSectionFromStack && _thirdSection) {
		_controller->showSection(
			std::move(*base::take(_thirdSectionFromStack)),
			SectionShow(
				SectionShow::Way::ClearStack,
				anim::type::instant,
				anim::activation::background));

	}
}

void MainWidget::orderWidgets() {
	_dialogs->raise();
	if (_player) {
		_player->raise();
	}
	if (_exportTopBar) {
		_exportTopBar->raise();
	}
	if (_callTopBar) {
		_callTopBar->raise();
	}
	if (_playerVolume) {
		_playerVolume->raise();
	}
	_sideShadow->raise();
	if (_thirdShadow) {
		_thirdShadow->raise();
	}
	if (_firstColumnResizeArea) {
		_firstColumnResizeArea->raise();
	}
	if (_thirdColumnResizeArea) {
		_thirdColumnResizeArea->raise();
	}
	_connecting->raise();
	_playerPlaylist->raise();
	_playerPanel->raise();
	for (auto &instance : _playerFloats) {
		instance->widget->raise();
	}
	if (_hider) _hider->raise();
}

QRect MainWidget::historyRect() const {
	QRect r(_history->historyRect());
	r.moveLeft(r.left() + _history->x());
	r.moveTop(r.top() + _history->y());
	return r;
}

QPixmap MainWidget::grabForShowAnimation(const Window::SectionSlideParams &params) {
	QPixmap result;
	for (auto &instance : _playerFloats) {
		instance->widget->hide();
	}
	if (_player) {
		_player->hideShadow();
	}
	auto playerVolumeVisible = _playerVolume && !_playerVolume->isHidden();
	if (playerVolumeVisible) {
		_playerVolume->hide();
	}
	auto playerPanelVisible = !_playerPanel->isHidden();
	if (playerPanelVisible) {
		_playerPanel->hide();
	}
	auto playerPlaylistVisible = !_playerPlaylist->isHidden();
	if (playerPlaylistVisible) {
		_playerPlaylist->hide();
	}

	auto sectionTop = getMainSectionTop();
	if (Adaptive::OneColumn()) {
		result = Ui::GrabWidget(this, QRect(
			0,
			sectionTop,
			_dialogsWidth,
			height() - sectionTop));
	} else {
		_sideShadow->hide();
		if (_thirdShadow) {
			_thirdShadow->hide();
		}
		result = Ui::GrabWidget(this, QRect(
			_dialogsWidth,
			sectionTop,
			width() - _dialogsWidth,
			height() - sectionTop));
		_sideShadow->show();
		if (_thirdShadow) {
			_thirdShadow->show();
		}
	}
	if (playerVolumeVisible) {
		_playerVolume->show();
	}
	if (playerPanelVisible) {
		_playerPanel->show();
	}
	if (playerPlaylistVisible) {
		_playerPlaylist->show();
	}
	if (_player) {
		_player->showShadow();
	}
	for (auto &instance : _playerFloats) {
		if (instance->visible) {
			instance->widget->show();
		}
	}
	return result;
}

void MainWidget::repaintDialogRow(
		Dialogs::Mode list,
		not_null<Dialogs::Row*> row) {
	_dialogs->repaintDialogRow(list, row);
}

void MainWidget::repaintDialogRow(
		not_null<History*> history,
		MsgId messageId) {
	_dialogs->repaintDialogRow(history, messageId);
}

void MainWidget::repaintDialogsWidget()
{
	_dialogs->update();
}

void MainWidget::performFilterDialogsWidget()
{
	_dialogs->performFilter();
}

void MainWidget::windowShown() {
	_history->windowShown();
}

void MainWidget::sentUpdatesReceived(uint64 randomId, const MTPUpdates &result) {
	feedUpdates(result, randomId);
}

bool MainWidget::deleteChannelFailed(const RPCError &error) {
	if (MTP::isDefaultHandledError(error)) return false;

	//if (error.type() == qstr("CHANNEL_TOO_LARGE")) {
	//	Ui::show(Box<InformBox>(lang(lng_cant_delete_channel)));
	//}

	return true;
}

void MainWidget::inviteToChannelDone(
		not_null<ChannelData*> channel,
		const MTPUpdates &updates) {
	sentUpdatesReceived(updates);
	Auth().api().requestParticipantsCountDelayed(channel);
}

void MainWidget::historyToDown(History *history) {
	_history->historyToDown(history);
}

void MainWidget::dialogsToUp() {
	_dialogs->dialogsToUp();
}

void MainWidget::newUnreadMsg(
		not_null<History*> history,
		not_null<HistoryItem*> item) {
	_history->newUnreadMsg(history, item);
}

void MainWidget::markActiveHistoryAsRead() {
	if (const auto activeHistory = _history->history()) {
		Auth().api().readServerHistory(activeHistory);
	}
}

void MainWidget::showAnimated(const QPixmap &bgAnimCache, bool back) {
	_showBack = back;
	(_showBack ? _cacheOver : _cacheUnder) = bgAnimCache;

	_a_show.finish();

	showAll();
	(_showBack ? _cacheUnder : _cacheOver) = Ui::GrabWidget(this);
	hideAll();

	_a_show.start(
		[this] { animationCallback(); },
		0.,
		1.,
		st::slideDuration,
		Window::SlideAnimation::transition());

	show();
}

void MainWidget::animationCallback() {
	update();
	if (!_a_show.animating()) {
		_cacheUnder = _cacheOver = QPixmap();

		showAll();
		activate();
	}
}

void MainWidget::paintEvent(QPaintEvent *e) {
	if (_background) checkChatBackground();

	Painter p(this);
	auto progress = _a_show.current(getms(), 1.);
	if (_a_show.animating()) {
		auto coordUnder = _showBack ? anim::interpolate(-st::slideShift, 0, progress) : anim::interpolate(0, -st::slideShift, progress);
		auto coordOver = _showBack ? anim::interpolate(0, width(), progress) : anim::interpolate(width(), 0, progress);
		auto shadow = _showBack ? (1. - progress) : progress;
		if (coordOver > 0) {
			p.drawPixmap(QRect(0, 0, coordOver, height()), _cacheUnder, QRect(-coordUnder * cRetinaFactor(), 0, coordOver * cRetinaFactor(), height() * cRetinaFactor()));
			p.setOpacity(shadow);
			p.fillRect(0, 0, coordOver, height(), st::slideFadeOutBg);
			p.setOpacity(1);
		}
		p.drawPixmap(coordOver, 0, _cacheOver);
		p.setOpacity(shadow);
		st::slideShadow.fill(p, QRect(coordOver - st::slideShadow.width(), 0, st::slideShadow.width(), height()));
	}
}

int MainWidget::getMainSectionTop() const {
	return _callTopBarHeight + _exportTopBarHeight + _playerHeight;
}

int MainWidget::getThirdSectionTop() const {
	return 0;
}

void MainWidget::hideAll() {
	_dialogs->hide();
	_history->hide();
	if (_mainSection) {
		_mainSection->hide();
	}
	if (_thirdSection) {
		_thirdSection->hide();
	}
	_sideShadow->hide();
	if (_thirdShadow) {
		_thirdShadow->hide();
	}
	if (_player) {
		_player->setVisible(false);
		_playerHeight = 0;
	}
	for (auto &instance : _playerFloats) {
		instance->widget->hide();
	}
}

void MainWidget::showAll() {
	if (cPasswordRecovered()) {
		cSetPasswordRecovered(false);
		Ui::show(Box<InformBox>(lang(lng_signin_password_removed)));
	}
	if (Adaptive::OneColumn()) {
		_sideShadow->hide();
		if (_hider) {
			_hider->hide();
			if (!_forwardConfirm && _hider->wasOffered()) {
				_forwardConfirm = Ui::show(Box<ConfirmBox>(_hider->offeredText(), lang(lng_forward_send), crl::guard(this, [this] {
					_hider->forward();
					if (_forwardConfirm) _forwardConfirm->closeBox();
					if (_hider) _hider->offerPeer(0);
				}), crl::guard(this, [this] {
					if (_hider && _forwardConfirm) _hider->offerPeer(0);
				})), LayerOption::CloseOther, anim::type::instant);
			}
		}
		if (selectingPeer()) {
			_dialogs->showFast();
			_history->hide();
			if (_mainSection) _mainSection->hide();
		} else if (_mainSection) {
			_mainSection->show();
		} else if (_history->peer()) {
			_history->show();
			_history->updateControlsGeometry();
		} else {
			_dialogs->showFast();
			_history->hide();
		}
		if (!selectingPeer()) {
			if (_mainSection) {
				_dialogs->hide();
			} else if (isMainSectionShown()) {
				_dialogs->hide();
			}
		}
	} else {
		_sideShadow->show();
		if (_hider) {
			_hider->show();
			if (_forwardConfirm) {
				_forwardConfirm = nullptr;
				Ui::hideLayer(anim::type::instant);
				if (_hider->wasOffered()) {
					_hider->setFocus();
				}
			}
		}
		_dialogs->showFast();
		if (_mainSection) {
			_mainSection->show();
		} else {
			_history->show();
			_history->updateControlsGeometry();
		}
		if (_thirdSection) {
			_thirdSection->show();
		}
		if (_thirdShadow) {
			_thirdShadow->show();
		}
	}
	if (_player) {
		_player->setVisible(true);
		_playerHeight = _player->contentHeight();
	}
	updateControlsGeometry();
	if (auto instance = currentFloatPlayer()) {
		checkFloatPlayerVisibility();
		if (instance->visible) {
			instance->widget->show();
		}
	}

	App::wnd()->checkHistoryActivation();
}

void MainWidget::resizeEvent(QResizeEvent *e) {
	updateControlsGeometry();
}

void MainWidget::updateControlsGeometry() {
	updateWindowAdaptiveLayout();
	if (Auth().settings().dialogsWidthRatio() > 0) {
		_a_dialogsWidth.finish();
	}
	if (!_a_dialogsWidth.animating()) {
		_dialogs->stopWidthAnimation();
	}
	if (Adaptive::ThreeColumn()) {
		if (!_thirdSection
			&& !_controller->takeThirdSectionFromLayer()) {
			auto params = Window::SectionShow(
				Window::SectionShow::Way::ClearStack,
				anim::type::instant,
				anim::activation::background);
			if (Auth().settings().bettergramTabsSectionEnabled()) {
				_history->pushBettergramTabsToThirdSection(params);
			} else if (Auth().settings().tabbedSelectorSectionEnabled()) {
				_history->pushTabbedSelectorToThirdSection(params);
			} else if (Auth().settings().thirdSectionInfoEnabled()) {
				if (const auto key = _controller->activeChatCurrent()) {
					_controller->showSection(
						Info::Memento::Default(key),
						params.withThirdColumn());
				}
			}
		}
	} else {
		_thirdSection.destroy();
		_thirdShadow.destroy();
	}
	auto mainSectionTop = getMainSectionTop();
	auto dialogsWidth = qRound(_a_dialogsWidth.current(_dialogsWidth));
	if (Adaptive::OneColumn()) {
		if (_callTopBar) {
			_callTopBar->resizeToWidth(dialogsWidth);
			_callTopBar->moveToLeft(0, 0);
		}
		if (_exportTopBar) {
			_exportTopBar->resizeToWidth(dialogsWidth);
			_exportTopBar->moveToLeft(0, _callTopBarHeight);
		}
		if (_player) {
			_player->resizeToWidth(dialogsWidth);
			_player->moveToLeft(0, _callTopBarHeight + _exportTopBarHeight);
		}
		auto mainSectionGeometry = QRect(
			0,
			mainSectionTop,
			dialogsWidth,
			height() - mainSectionTop);
		_dialogs->setGeometry(mainSectionGeometry);
		_history->setGeometry(mainSectionGeometry);
		if (_hider) _hider->setGeometry(0, 0, dialogsWidth, height());
	} else {
		auto thirdSectionWidth = _thirdSection ? _thirdColumnWidth : 0;
		if (_thirdSection) {
			auto thirdSectionTop = getThirdSectionTop();
			_thirdSection->setGeometry(
				width() - thirdSectionWidth,
				thirdSectionTop,
				thirdSectionWidth,
				height() - thirdSectionTop);
		}
		accumulate_min(dialogsWidth, width() - st::columnMinimalWidthMain);
		auto mainSectionWidth = width() - dialogsWidth - thirdSectionWidth;

		_dialogs->setGeometryToLeft(0, 0, dialogsWidth, height());
		_sideShadow->setGeometryToLeft(dialogsWidth, 0, st::lineWidth, height());
		if (_thirdShadow) {
			_thirdShadow->setGeometryToLeft(
				width() - thirdSectionWidth - st::lineWidth,
				0,
				st::lineWidth,
				height());
		}
		if (_callTopBar) {
			_callTopBar->resizeToWidth(mainSectionWidth);
			_callTopBar->moveToLeft(dialogsWidth, 0);
		}
		if (_exportTopBar) {
			_exportTopBar->resizeToWidth(mainSectionWidth);
			_exportTopBar->moveToLeft(dialogsWidth, _callTopBarHeight);
		}
		if (_player) {
			_player->resizeToWidth(mainSectionWidth);
			_player->moveToLeft(
				dialogsWidth,
				_callTopBarHeight + _exportTopBarHeight);
		}
		_history->setGeometryToLeft(dialogsWidth, mainSectionTop, mainSectionWidth, height() - mainSectionTop);
		if (_hider) {
			_hider->setGeometryToLeft(dialogsWidth, 0, mainSectionWidth, height());
		}
	}
	if (_mainSection) {
		auto mainSectionGeometry = QRect(_history->x(), mainSectionTop, _history->width(), height() - mainSectionTop);
		_mainSection->setGeometryWithTopMoved(mainSectionGeometry, _contentScrollAddToY);
	}
	refreshResizeAreas();
	updateMediaPlayerPosition();
	updateMediaPlaylistPosition(_playerPlaylist->x());
	_contentScrollAddToY = 0;
	for (auto &instance : _playerFloats) {
		updateFloatPlayerPosition(instance.get());
	}
}

void MainWidget::refreshResizeAreas() {
	if (!Adaptive::OneColumn()) {
		ensureFirstColumnResizeAreaCreated();
		_firstColumnResizeArea->setGeometryToLeft(
			_history->x(),
			0,
			st::historyResizeWidth,
			height());
	} else if (_firstColumnResizeArea) {
		_firstColumnResizeArea.destroy();
	}

	if (Adaptive::ThreeColumn() && _thirdSection) {
		ensureThirdColumnResizeAreaCreated();
		_thirdColumnResizeArea->setGeometryToLeft(
			_thirdSection->x(),
			0,
			st::historyResizeWidth,
			height());
	} else if (_thirdColumnResizeArea) {
		_thirdColumnResizeArea.destroy();
	}
}

template <typename MoveCallback, typename FinishCallback>
void MainWidget::createResizeArea(
		object_ptr<Ui::ResizeArea> &area,
		MoveCallback &&moveCallback,
		FinishCallback &&finishCallback) {
	area.create(this);
	area->show();
	area->addMoveLeftCallback(
		std::forward<MoveCallback>(moveCallback));
	area->addMoveFinishedCallback(
		std::forward<FinishCallback>(finishCallback));
	orderWidgets();
}

void MainWidget::ensureFirstColumnResizeAreaCreated() {
	if (_firstColumnResizeArea) {
		return;
	}
	auto moveLeftCallback = [=](int globalLeft) {
		auto newWidth = globalLeft - mapToGlobal(QPoint(0, 0)).x();
		auto newRatio = (newWidth < st::columnMinimalWidthLeft / 2)
			? 0.
			: float64(newWidth) / width();
		Auth().settings().setDialogsWidthRatio(newRatio);
	};
	auto moveFinishedCallback = [=] {
		if (Adaptive::OneColumn()) {
			return;
		}
		if (Auth().settings().dialogsWidthRatio() > 0) {
			Auth().settings().setDialogsWidthRatio(
				float64(_dialogsWidth) / width());
		}
		Local::writeUserSettings();
	};
	createResizeArea(
		_firstColumnResizeArea,
		std::move(moveLeftCallback),
		std::move(moveFinishedCallback));
}

void MainWidget::ensureThirdColumnResizeAreaCreated() {
	if (_thirdColumnResizeArea) {
		return;
	}
	auto moveLeftCallback = [=](int globalLeft) {
		auto newWidth = mapToGlobal(QPoint(width(), 0)).x() - globalLeft;
		Auth().settings().setThirdColumnWidth(newWidth);
	};
	auto moveFinishedCallback = [=] {
		if (!Adaptive::ThreeColumn() || !_thirdSection) {
			return;
		}
		Auth().settings().setThirdColumnWidth(snap(
			Auth().settings().thirdColumnWidth(),
			st::columnMinimalWidthThird,
			st::columnMaximalWidthThird));
		Local::writeUserSettings();
	};
	createResizeArea(
		_thirdColumnResizeArea,
		std::move(moveLeftCallback),
		std::move(moveFinishedCallback));
}

void MainWidget::updateDialogsWidthAnimated() {
	if (Auth().settings().dialogsWidthRatio() > 0) {
		return;
	}
	auto dialogsWidth = _dialogsWidth;
	updateWindowAdaptiveLayout();
	if (!Auth().settings().dialogsWidthRatio()
		&& (_dialogsWidth != dialogsWidth
			|| _a_dialogsWidth.animating())) {
		_dialogs->startWidthAnimation();
		_a_dialogsWidth.start(
			[this] { updateControlsGeometry(); },
			dialogsWidth,
			_dialogsWidth,
			st::dialogsWidthDuration,
			anim::easeOutCirc);
		updateControlsGeometry();
	}
}

bool MainWidget::saveThirdSectionToStackBack() const {
	return !_stack.empty()
		&& _thirdSection != nullptr
		&& _stack.back()->thirdSectionWeak() == _thirdSection.data();
}

auto MainWidget::thirdSectionForCurrentMainSection(
	Dialogs::Key key)
-> std::unique_ptr<Window::SectionMemento> {
	if (_thirdSectionFromStack) {
		return std::move(_thirdSectionFromStack);
	} else if (const auto peer = key.peer()) {
		return std::make_unique<Info::Memento>(
			peer->id,
			Info::Memento::DefaultSection(key));
	} else if (const auto feed = key.feed()) {
		return std::make_unique<Info::Memento>(
			feed,
			Info::Memento::DefaultSection(key));
	}
	Unexpected("Key in MainWidget::thirdSectionForCurrentMainSection().");
}

void MainWidget::updateThirdColumnToCurrentChat(
		Dialogs::Key key,
		bool canWrite) {
	auto saveOldThirdSection = [&] {
		if (saveThirdSectionToStackBack()) {
			_stack.back()->setThirdSectionMemento(
				_thirdSection->createMemento());
			_thirdSection.destroy();
		}
	};
	auto params = Window::SectionShow(
		Window::SectionShow::Way::ClearStack,
		anim::type::instant,
		anim::activation::background);
	auto switchInfoFast = [&] {
		saveOldThirdSection();

		//
		// Like in _controller->showPeerInfo()
		//
		if (Adaptive::ThreeColumn()
			&& !Auth().settings().thirdSectionInfoEnabled()) {
			Auth().settings().setThirdSectionInfoEnabled(true);
			Auth().saveSettingsDelayed();
		}

		_controller->showSection(
			std::move(*thirdSectionForCurrentMainSection(key)),
			params.withThirdColumn());
	};
	auto switchBettergramTabsFast = [&] {
		saveOldThirdSection();
		_history->pushBettergramTabsToThirdSection(params);
	};
	auto switchTabbedFast = [&] {
		saveOldThirdSection();
		_history->pushTabbedSelectorToThirdSection(params);
	};
	if (Adaptive::ThreeColumn()
		&& Auth().settings().bettergramTabsSectionEnabled()
		&& key) {
		if (!canWrite) {
			switchInfoFast();
			Auth().settings().setBettergramTabsSectionEnabled(true);
			Auth().settings().setTabbedReplacedWithInfo(true);
		} else if (Auth().settings().tabbedReplacedWithInfo()) {
			Auth().settings().setTabbedReplacedWithInfo(false);
			switchBettergramTabsFast();
		}
	} else if (Adaptive::ThreeColumn()
		&& Auth().settings().tabbedSelectorSectionEnabled()
		&& key) {
		if (!canWrite) {
			switchInfoFast();
			Auth().settings().setTabbedSelectorSectionEnabled(true);
			Auth().settings().setTabbedReplacedWithInfo(true);
		} else if (Auth().settings().tabbedReplacedWithInfo()) {
			Auth().settings().setTabbedReplacedWithInfo(false);
			switchTabbedFast();
		}
	} else {
		Auth().settings().setTabbedReplacedWithInfo(false);
		if (!key) {
			if (_thirdSection) {
				_thirdSection.destroy();
				_thirdShadow.destroy();
				updateControlsGeometry();
			}
		} else if (Adaptive::ThreeColumn()
			&& Auth().settings().thirdSectionInfoEnabled()) {
			switchInfoFast();
		}
	}
}

void MainWidget::updateMediaPlayerPosition() {
	_playerPanel->moveToRight(0, 0);
	if (_player && _playerVolume) {
		auto relativePosition = _player->entity()->getPositionForVolumeWidget();
		auto playerMargins = _playerVolume->getMargin();
		_playerVolume->moveToLeft(_player->x() + relativePosition.x() - playerMargins.left(), _player->y() + relativePosition.y() - playerMargins.top());
	}
}

void MainWidget::updateMediaPlaylistPosition(int x) {
	if (_player) {
		auto playlistLeft = x;
		auto playlistWidth = _playerPlaylist->width();
		auto playlistTop = _player->y() + _player->height();
		auto rightEdge = width();
		if (playlistLeft + playlistWidth > rightEdge) {
			playlistLeft = rightEdge - playlistWidth;
		} else if (playlistLeft < 0) {
			playlistLeft = 0;
		}
		_playerPlaylist->move(playlistLeft, playlistTop);
	}
}

int MainWidget::contentScrollAddToY() const {
	return _contentScrollAddToY;
}

void MainWidget::keyPressEvent(QKeyEvent *e) {
}

bool MainWidget::eventFilter(QObject *o, QEvent *e) {
	if (e->type() == QEvent::FocusIn) {
		if (auto widget = qobject_cast<QWidget*>(o)) {
			if (_history == widget || _history->isAncestorOf(widget)
				|| (_mainSection && (_mainSection == widget || _mainSection->isAncestorOf(widget)))
				|| (_thirdSection && (_thirdSection == widget || _thirdSection->isAncestorOf(widget)))) {
				_controller->dialogsListFocused().set(false);
			} else if (_dialogs == widget || _dialogs->isAncestorOf(widget)) {
				_controller->dialogsListFocused().set(true);
			}
		}
	} else if (e->type() == QEvent::MouseButtonPress) {
		if (static_cast<QMouseEvent*>(e)->button() == Qt::BackButton) {
			_controller->showBackFromStack();
			return true;
		}
	} else if (e->type() == QEvent::Wheel && !_playerFloats.empty()) {
		for (auto &instance : _playerFloats) {
			if (instance->widget == o) {
				auto section = getFloatPlayerSection(
					instance->column);
				return section->wheelEventFromFloatPlayer(e);
			}
		}
	}
	return TWidget::eventFilter(o, e);
}

void MainWidget::handleAdaptiveLayoutUpdate() {
	showAll();
	_sideShadow->setVisible(!Adaptive::OneColumn());
	if (_player) {
		_player->updateAdaptiveLayout();
	}
}

void MainWidget::updateWindowAdaptiveLayout() {
	auto layout = _controller->computeColumnLayout();
	auto dialogsWidthRatio = Auth().settings().dialogsWidthRatio();

	// Check if we are in a single-column layout in a wide enough window
	// for the normal layout. If so, switch to the normal layout.
	if (layout.windowLayout == Adaptive::WindowLayout::OneColumn) {
		auto chatWidth = layout.chatWidth;
		//if (Auth().settings().tabbedSelectorSectionEnabled()
		//	&& chatWidth >= _history->minimalWidthForTabbedSelectorSection()) {
		//	chatWidth -= _history->tabbedSelectorSectionWidth();
		//}
		auto minimalNormalWidth = st::columnMinimalWidthLeft
			+ st::columnMinimalWidthMain;
		if (chatWidth >= minimalNormalWidth) {
			// Switch layout back to normal in a wide enough window.
			layout.windowLayout = Adaptive::WindowLayout::Normal;
			layout.dialogsWidth = st::columnMinimalWidthLeft;
			layout.chatWidth = layout.bodyWidth - layout.dialogsWidth;
			dialogsWidthRatio = float64(layout.dialogsWidth) / layout.bodyWidth;
		}
	}

	// Check if we are going to create the third column and shrink the
	// dialogs widget to provide a wide enough chat history column.
	// Don't shrink the column on the first call, when window is inited.
	if (layout.windowLayout == Adaptive::WindowLayout::ThreeColumn
		&& _started && _controller->window()->positionInited()) {
		//auto chatWidth = layout.chatWidth;
		//if (_history->willSwitchToTabbedSelectorWithWidth(chatWidth)) {
		//	auto thirdColumnWidth = _history->tabbedSelectorSectionWidth();
		//	auto twoColumnsWidth = (layout.bodyWidth - thirdColumnWidth);
		//	auto sameRatioChatWidth = twoColumnsWidth - qRound(dialogsWidthRatio * twoColumnsWidth);
		//	auto desiredChatWidth = qMax(sameRatioChatWidth, HistoryView::WideChatWidth());
		//	chatWidth -= thirdColumnWidth;
		//	auto extendChatBy = desiredChatWidth - chatWidth;
		//	accumulate_min(extendChatBy, layout.dialogsWidth - st::columnMinimalWidthLeft);
		//	if (extendChatBy > 0) {
		//		layout.dialogsWidth -= extendChatBy;
		//		layout.chatWidth += extendChatBy;
		//		dialogsWidthRatio = float64(layout.dialogsWidth) / layout.bodyWidth;
		//	}
		//}
	}

	Auth().settings().setDialogsWidthRatio(dialogsWidthRatio);

	//	auto useSmallColumnWidth = !Adaptive::OneColumn()
	//		&& !dialogsWidthRatio
	//		&& !_controller->forceWideDialogs();

	// In bettergram we do not use small column width because we have chat tabs
	bool useSmallColumnWidth = false;

	_dialogsWidth = useSmallColumnWidth
		? _controller->dialogsSmallColumnWidth()
		: layout.dialogsWidth;
	_thirdColumnWidth = layout.thirdWidth;
	if (layout.windowLayout != Global::AdaptiveWindowLayout()) {
		Global::SetAdaptiveWindowLayout(layout.windowLayout);
		Adaptive::Changed().notify(true);
	}
}

int MainWidget::backgroundFromY() const {
	return -getMainSectionTop();
}

void MainWidget::searchInChat(Dialogs::Key chat) {
	_dialogs->searchInChat(chat);
	if (Adaptive::OneColumn()) {
		dialogsToUp();
		Ui::showChatsList();
	} else {
		_dialogs->activate();
	}
}

void MainWidget::feedUpdateVector(
		const MTPVector<MTPUpdate> &updates,
		bool skipMessageIds) {
	for_const (auto &update, updates.v) {
		if (skipMessageIds && update.type() == mtpc_updateMessageID) continue;
		feedUpdate(update);
	}
	Auth().data().sendHistoryChangeNotifications();
}

void MainWidget::feedMessageIds(const MTPVector<MTPUpdate> &updates) {
	for_const (auto &update, updates.v) {
		if (update.type() == mtpc_updateMessageID) {
			feedUpdate(update);
		}
	}
}

bool MainWidget::updateFail(const RPCError &e) {
	crl::on_main(this, [] { Messenger::Instance().logOut(); });
	return true;
}

void MainWidget::updSetState(int32 pts, int32 date, int32 qts, int32 seq) {
	if (pts) {
		_ptsWaiter.init(pts);
	}
	if (updDate < date && !_byMinChannelTimer.isActive()) {
		updDate = date;
	}
	if (qts && updQts < qts) {
		updQts = qts;
	}
	if (seq && seq != updSeq) {
		updSeq = seq;
		if (_bySeqTimer.isActive()) _bySeqTimer.stop();
		for (QMap<int32, MTPUpdates>::iterator i = _bySeqUpdates.begin(); i != _bySeqUpdates.end();) {
			int32 s = i.key();
			if (s <= seq + 1) {
				MTPUpdates v = i.value();
				i = _bySeqUpdates.erase(i);
				if (s == seq + 1) {
					return feedUpdates(v);
				}
			} else {
				if (!_bySeqTimer.isActive()) _bySeqTimer.start(WaitForSkippedTimeout);
				break;
			}
		}
	}
}

void MainWidget::gotChannelDifference(
		ChannelData *channel,
		const MTPupdates_ChannelDifference &diff) {
	_channelFailDifferenceTimeout.remove(channel);

	int32 timeout = 0;
	bool isFinal = true;
	switch (diff.type()) {
	case mtpc_updates_channelDifferenceEmpty: {
		auto &d = diff.c_updates_channelDifferenceEmpty();
		if (d.has_timeout()) timeout = d.vtimeout.v;
		isFinal = d.is_final();
		channel->ptsInit(d.vpts.v);
	} break;

	case mtpc_updates_channelDifferenceTooLong: {
		auto &d = diff.c_updates_channelDifferenceTooLong();

		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		auto history = App::historyLoaded(channel->id);
		if (history) {
			history->setNotLoadedAtBottom();
		}
		App::feedMsgs(d.vmessages, NewMessageLast);
		if (history) {
			history->applyDialogFields(
				d.vunread_count.v,
				d.vread_inbox_max_id.v,
				d.vread_outbox_max_id.v);
			history->applyDialogTopMessage(d.vtop_message.v);
			history->setUnreadMentionsCount(d.vunread_mentions_count.v);
			if (_history->peer() == channel) {
				_history->updateHistoryDownVisibility();
				_history->preloadHistoryIfNeeded();
			}
			Auth().api().requestChannelRangeDifference(history);
		}

		if (d.has_timeout()) {
			timeout = d.vtimeout.v;
		}
		isFinal = d.is_final();
		channel->ptsInit(d.vpts.v);
	} break;

	case mtpc_updates_channelDifference: {
		auto &d = diff.c_updates_channelDifference();

		feedChannelDifference(d);

		if (d.has_timeout()) timeout = d.vtimeout.v;
		isFinal = d.is_final();
		channel->ptsInit(d.vpts.v);
	} break;
	}

	channel->ptsSetRequesting(false);

	if (!isFinal) {
		MTP_LOG(0, ("getChannelDifference { good - after not final channelDifference was received }%1").arg(cTestMode() ? " TESTMODE" : ""));
		getChannelDifference(channel);
	} else if (_controller->activeChatCurrent().peer() == channel) {
		channel->ptsWaitingForShortPoll(timeout ? (timeout * 1000) : WaitForChannelGetDifference);
	}
}

void MainWidget::feedChannelDifference(
		const MTPDupdates_channelDifference &data) {
	App::feedUsers(data.vusers);
	App::feedChats(data.vchats);

	_handlingChannelDifference = true;
	feedMessageIds(data.vother_updates);
	App::feedMsgs(data.vnew_messages, NewMessageUnread);
	feedUpdateVector(data.vother_updates, true);
	_handlingChannelDifference = false;
}

bool MainWidget::failChannelDifference(ChannelData *channel, const RPCError &error) {
	if (MTP::isDefaultHandledError(error)) return false;

	LOG(("RPC Error in getChannelDifference: %1 %2: %3").arg(error.code()).arg(error.type()).arg(error.description()));
	failDifferenceStartTimerFor(channel);
	return true;
}

void MainWidget::gotState(const MTPupdates_State &state) {
	auto &d = state.c_updates_state();
	updSetState(d.vpts.v, d.vdate.v, d.vqts.v, d.vseq.v);

	_lastUpdateTime = getms(true);
	noUpdatesTimer.start(NoUpdatesTimeout);
	_ptsWaiter.setRequesting(false);

	_dialogs->loadDialogs();
	updateOnline();
}

void MainWidget::gotDifference(const MTPupdates_Difference &difference) {
	_failDifferenceTimeout = 1;

	switch (difference.type()) {
	case mtpc_updates_differenceEmpty: {
		auto &d = difference.c_updates_differenceEmpty();
		updSetState(_ptsWaiter.current(), d.vdate.v, updQts, d.vseq.v);

		_lastUpdateTime = getms(true);
		noUpdatesTimer.start(NoUpdatesTimeout);

		_ptsWaiter.setRequesting(false);
	} break;
	case mtpc_updates_differenceSlice: {
		auto &d = difference.c_updates_differenceSlice();
		feedDifference(d.vusers, d.vchats, d.vnew_messages, d.vother_updates);

		auto &s = d.vintermediate_state.c_updates_state();
		updSetState(s.vpts.v, s.vdate.v, s.vqts.v, s.vseq.v);

		_ptsWaiter.setRequesting(false);

		MTP_LOG(0, ("getDifference { good - after a slice of difference was received }%1").arg(cTestMode() ? " TESTMODE" : ""));
		getDifference();
	} break;
	case mtpc_updates_difference: {
		auto &d = difference.c_updates_difference();
		feedDifference(d.vusers, d.vchats, d.vnew_messages, d.vother_updates);

		gotState(d.vstate);
	} break;
	case mtpc_updates_differenceTooLong: {
		auto &d = difference.c_updates_differenceTooLong();
		LOG(("API Error: updates.differenceTooLong is not supported by Telegram Desktop!"));
	} break;
	};
}

bool MainWidget::getDifferenceTimeChanged(ChannelData *channel, int32 ms, ChannelGetDifferenceTime &channelCurTime, TimeMs &curTime) {
	if (channel) {
		if (ms <= 0) {
			ChannelGetDifferenceTime::iterator i = channelCurTime.find(channel);
			if (i != channelCurTime.cend()) {
				channelCurTime.erase(i);
			} else {
				return false;
			}
		} else {
			auto when = getms(true) + ms;
			ChannelGetDifferenceTime::iterator i = channelCurTime.find(channel);
			if (i != channelCurTime.cend()) {
				if (i.value() > when) {
					i.value() = when;
				} else {
					return false;
				}
			} else {
				channelCurTime.insert(channel, when);
			}
		}
	} else {
		if (ms <= 0) {
			if (curTime) {
				curTime = 0;
			} else {
				return false;
			}
		} else {
			auto when = getms(true) + ms;
			if (!curTime || curTime > when) {
				curTime = when;
			} else {
				return false;
			}
		}
	}
	return true;
}

void MainWidget::ptsWaiterStartTimerFor(ChannelData *channel, int32 ms) {
	if (getDifferenceTimeChanged(channel, ms, _channelGetDifferenceTimeByPts, _getDifferenceTimeByPts)) {
		onGetDifferenceTimeByPts();
	}
}

void MainWidget::failDifferenceStartTimerFor(ChannelData *channel) {
	int32 ms = 0;
	ChannelFailDifferenceTimeout::iterator i;
	if (channel) {
		i = _channelFailDifferenceTimeout.find(channel);
		if (i == _channelFailDifferenceTimeout.cend()) {
			i = _channelFailDifferenceTimeout.insert(channel, 1);
		}
		ms = i.value() * 1000;
	} else {
		ms = _failDifferenceTimeout * 1000;
	}
	if (getDifferenceTimeChanged(channel, ms, _channelGetDifferenceTimeAfterFail, _getDifferenceTimeAfterFail)) {
		onGetDifferenceTimeAfterFail();
	}
	if (channel) {
		if (i.value() < 64) i.value() *= 2;
	} else {
		if (_failDifferenceTimeout < 64) _failDifferenceTimeout *= 2;
	}
}

bool MainWidget::ptsUpdateAndApply(int32 pts, int32 ptsCount, const MTPUpdates &updates) {
	return _ptsWaiter.updateAndApply(nullptr, pts, ptsCount, updates);
}

bool MainWidget::ptsUpdateAndApply(int32 pts, int32 ptsCount, const MTPUpdate &update) {
	return _ptsWaiter.updateAndApply(nullptr, pts, ptsCount, update);
}

bool MainWidget::ptsUpdateAndApply(int32 pts, int32 ptsCount) {
	return _ptsWaiter.updateAndApply(nullptr, pts, ptsCount);
}

void MainWidget::feedDifference(
		const MTPVector<MTPUser> &users,
		const MTPVector<MTPChat> &chats,
		const MTPVector<MTPMessage> &msgs,
		const MTPVector<MTPUpdate> &other) {
	Auth().checkAutoLock();
	App::feedUsers(users);
	App::feedChats(chats);
	feedMessageIds(other);
	App::feedMsgs(msgs, NewMessageUnread);
	feedUpdateVector(other, true);
}

bool MainWidget::failDifference(const RPCError &error) {
	if (MTP::isDefaultHandledError(error)) return false;

	LOG(("RPC Error in getDifference: %1 %2: %3").arg(error.code()).arg(error.type()).arg(error.description()));
	failDifferenceStartTimerFor(0);
	return true;
}

void MainWidget::onGetDifferenceTimeByPts() {
	auto now = getms(true), wait = 0LL;
	if (_getDifferenceTimeByPts) {
		if (_getDifferenceTimeByPts > now) {
			wait = _getDifferenceTimeByPts - now;
		} else {
			getDifference();
		}
	}
	for (ChannelGetDifferenceTime::iterator i = _channelGetDifferenceTimeByPts.begin(); i != _channelGetDifferenceTimeByPts.cend();) {
		if (i.value() > now) {
			wait = wait ? qMin(wait, i.value() - now) : (i.value() - now);
			++i;
		} else {
			getChannelDifference(i.key(), ChannelDifferenceRequest::PtsGapOrShortPoll);
			i = _channelGetDifferenceTimeByPts.erase(i);
		}
	}
	if (wait) {
		_byPtsTimer.start(wait);
	} else {
		_byPtsTimer.stop();
	}
}

void MainWidget::onGetDifferenceTimeAfterFail() {
	auto now = getms(true), wait = 0LL;
	if (_getDifferenceTimeAfterFail) {
		if (_getDifferenceTimeAfterFail > now) {
			wait = _getDifferenceTimeAfterFail - now;
		} else {
			_ptsWaiter.setRequesting(false);
			MTP_LOG(0, ("getDifference { force - after get difference failed }%1").arg(cTestMode() ? " TESTMODE" : ""));
			getDifference();
		}
	}
	for (auto i = _channelGetDifferenceTimeAfterFail.begin(); i != _channelGetDifferenceTimeAfterFail.cend();) {
		if (i.value() > now) {
			wait = wait ? qMin(wait, i.value() - now) : (i.value() - now);
			++i;
		} else {
			getChannelDifference(i.key(), ChannelDifferenceRequest::AfterFail);
			i = _channelGetDifferenceTimeAfterFail.erase(i);
		}
	}
	if (wait) {
		_failDifferenceTimer.start(wait);
	} else {
		_failDifferenceTimer.stop();
	}
}

void MainWidget::getDifference() {
	if (this != App::main()) return;

	_getDifferenceTimeByPts = 0;

	if (requestingDifference()) return;

	_bySeqUpdates.clear();
	_bySeqTimer.stop();

	noUpdatesTimer.stop();
	_getDifferenceTimeAfterFail = 0;

	_ptsWaiter.setRequesting(true);

	MTP::send(MTPupdates_GetDifference(MTP_flags(0), MTP_int(_ptsWaiter.current()), MTPint(), MTP_int(updDate), MTP_int(updQts)), rpcDone(&MainWidget::gotDifference), rpcFail(&MainWidget::failDifference));
}

void MainWidget::getChannelDifference(ChannelData *channel, ChannelDifferenceRequest from) {
	if (this != App::main() || !channel) return;

	if (from != ChannelDifferenceRequest::PtsGapOrShortPoll) {
		_channelGetDifferenceTimeByPts.remove(channel);
	}

	if (!channel->ptsInited() || channel->ptsRequesting()) return;

	if (from != ChannelDifferenceRequest::AfterFail) {
		_channelGetDifferenceTimeAfterFail.remove(channel);
	}

	channel->ptsSetRequesting(true);

	auto filter = MTP_channelMessagesFilterEmpty();
	auto flags = MTPupdates_GetChannelDifference::Flag::f_force | 0;
	if (from != ChannelDifferenceRequest::PtsGapOrShortPoll) {
		if (!channel->ptsWaitingForSkipped()) {
			flags = 0; // No force flag when requesting for short poll.
		}
	}
	MTP::send(MTPupdates_GetChannelDifference(MTP_flags(flags), channel->inputChannel, filter, MTP_int(channel->pts()), MTP_int(MTPChannelGetDifferenceLimit)), rpcDone(&MainWidget::gotChannelDifference, channel), rpcFail(&MainWidget::failChannelDifference, channel));
}

void MainWidget::mtpPing() {
	MTP::ping();
}

void MainWidget::start(const MTPUser *self) {
	Auth().api().requestNotifySettings(MTP_inputNotifyUsers());
	Auth().api().requestNotifySettings(MTP_inputNotifyChats());

	if (!self) {
		MTP::send(MTPusers_GetFullUser(MTP_inputUserSelf()), rpcDone(&MainWidget::startWithSelf));
		return;
	} else if (!Auth().validateSelf(*self)) {
		constexpr auto kRequestUserAgainTimeout = TimeMs(10000);
		App::CallDelayed(kRequestUserAgainTimeout, this, [=] {
			MTP::send(MTPusers_GetFullUser(MTP_inputUserSelf()), rpcDone(&MainWidget::startWithSelf));
		});
		return;
	}

	Local::readSavedPeers();
	cSetOtherOnline(0);
	if (const auto user = App::feedUsers(MTP_vector<MTPUser>(1, *self))) {
		user->loadUserpic();
	}

	MTP::send(MTPupdates_GetState(), rpcDone(&MainWidget::gotState));
	update();

	_started = true;
	App::wnd()->sendServiceHistoryRequest();
	Local::readInstalledStickers();
	Local::readFeaturedStickers();
	Local::readRecentStickers();
	Local::readFavedStickers();
	Local::readSavedGifs();
	if (const auto availableAt = Local::ReadExportSettings().availableAt) {
		Auth().data().suggestStartExport(availableAt);
	}

	_history->start();

	Messenger::Instance().checkStartUrl();
}

bool MainWidget::started() {
	return _started;
}

void MainWidget::openPeerByName(const QString &username, MsgId msgId, const QString &startToken) {
	Messenger::Instance().hideMediaView();

	PeerData *peer = App::peerByName(username);
	if (peer) {
		if (msgId == ShowAtGameShareMsgId) {
			if (peer->isUser() && peer->asUser()->botInfo && !startToken.isEmpty()) {
				peer->asUser()->botInfo->shareGameShortName = startToken;
				AddBotToGroupBoxController::Start(peer->asUser());
			} else {
				InvokeQueued(this, [this, peer] {
					_controller->showPeerHistory(
						peer->id,
						SectionShow::Way::Forward);
				});
			}
		} else if (msgId == ShowAtProfileMsgId && !peer->isChannel()) {
			if (peer->isUser() && peer->asUser()->botInfo && !peer->asUser()->botInfo->cantJoinGroups && !startToken.isEmpty()) {
				peer->asUser()->botInfo->startGroupToken = startToken;
				AddBotToGroupBoxController::Start(peer->asUser());
			} else if (peer->isUser() && peer->asUser()->botInfo) {
				// Always open bot chats, even from mention links.
				InvokeQueued(this, [this, peer] {
					_controller->showPeerHistory(
						peer->id,
						SectionShow::Way::Forward);
				});
			} else {
				_controller->showPeerInfo(peer);
			}
		} else {
			if (msgId == ShowAtProfileMsgId || !peer->isChannel()) { // show specific posts only in channels / supergroups
				msgId = ShowAtUnreadMsgId;
			}
			if (peer->isUser() && peer->asUser()->botInfo) {
				peer->asUser()->botInfo->startToken = startToken;
				if (peer == _history->peer()) {
					_history->updateControlsVisibility();
					_history->updateControlsGeometry();
				}
			}
			InvokeQueued(this, [this, peer, msgId] {
				_controller->showPeerHistory(
					peer->id,
					SectionShow::Way::Forward,
					msgId);
			});
		}
	} else {
		MTP::send(MTPcontacts_ResolveUsername(MTP_string(username)), rpcDone(&MainWidget::usernameResolveDone, qMakePair(msgId, startToken)), rpcFail(&MainWidget::usernameResolveFail, username));
	}
}

void MainWidget::joinGroupByHash(const QString &hash) {
	Messenger::Instance().hideMediaView();
	MTP::send(MTPmessages_CheckChatInvite(MTP_string(hash)), rpcDone(&MainWidget::inviteCheckDone, hash), rpcFail(&MainWidget::inviteCheckFail));
}

void MainWidget::stickersBox(const MTPInputStickerSet &set) {
	Messenger::Instance().hideMediaView();
	Ui::show(Box<StickerSetBox>(set));
}

void MainWidget::onSelfParticipantUpdated(ChannelData *channel) {
	auto history = App::historyLoaded(channel->id);
	if (_updatedChannels.contains(channel)) {
		_updatedChannels.remove(channel);
		if (!history) {
			history = App::history(channel);
		}
		if (history->isEmpty()) {
			Auth().api().requestDialogEntry(history);
		} else {
			history->checkJoinedMessage(true);
		}
	} else if (history) {
		history->checkJoinedMessage();
	}
	Auth().data().sendHistoryChangeNotifications();
}

bool MainWidget::contentOverlapped(const QRect &globalRect) {
	return (_history->contentOverlapped(globalRect)
			|| _playerPanel->overlaps(globalRect)
			|| _playerPlaylist->overlaps(globalRect)
			|| (_playerVolume && _playerVolume->overlaps(globalRect)));
}

void MainWidget::usernameResolveDone(QPair<MsgId, QString> msgIdAndStartToken, const MTPcontacts_ResolvedPeer &result) {
	Ui::hideLayer();
	if (result.type() != mtpc_contacts_resolvedPeer) return;

	const auto &d(result.c_contacts_resolvedPeer());
	App::feedUsers(d.vusers);
	App::feedChats(d.vchats);
	PeerId peerId = peerFromMTP(d.vpeer);
	if (!peerId) return;

	PeerData *peer = App::peer(peerId);
	MsgId msgId = msgIdAndStartToken.first;
	QString startToken = msgIdAndStartToken.second;
	if (msgId == ShowAtProfileMsgId && !peer->isChannel()) {
		if (peer->isUser() && peer->asUser()->botInfo && !peer->asUser()->botInfo->cantJoinGroups && !startToken.isEmpty()) {
			peer->asUser()->botInfo->startGroupToken = startToken;
			AddBotToGroupBoxController::Start(peer->asUser());
		} else if (peer->isUser() && peer->asUser()->botInfo) {
			// Always open bot chats, even from mention links.
			InvokeQueued(this, [this, peer] {
				_controller->showPeerHistory(
					peer->id,
					SectionShow::Way::Forward);
			});
		} else {
			_controller->showPeerInfo(peer);
		}
	} else {
		if (msgId == ShowAtProfileMsgId || !peer->isChannel()) { // show specific posts only in channels / supergroups
			msgId = ShowAtUnreadMsgId;
		}
		if (peer->isUser() && peer->asUser()->botInfo) {
			peer->asUser()->botInfo->startToken = startToken;
			if (peer == _history->peer()) {
				_history->updateControlsVisibility();
				_history->updateControlsGeometry();
			}
		}
		InvokeQueued(this, [this, peer, msgId] {
			_controller->showPeerHistory(
				peer->id,
				SectionShow::Way::Forward,
				msgId);
		});
	}
}

bool MainWidget::usernameResolveFail(QString name, const RPCError &error) {
	if (MTP::isDefaultHandledError(error)) return false;

	if (error.code() == 400) {
		Ui::show(Box<InformBox>(lng_username_not_found(lt_user, name)));
	}
	return true;
}

void MainWidget::inviteCheckDone(QString hash, const MTPChatInvite &invite) {
	switch (invite.type()) {
	case mtpc_chatInvite: {
		auto &d = invite.c_chatInvite();

		auto participants = QVector<UserData*>();
		if (d.has_participants()) {
			auto &v = d.vparticipants.v;
			participants.reserve(v.size());
			for_const (auto &user, v) {
				if (auto feededUser = App::feedUser(user)) {
					participants.push_back(feededUser);
				}
			}
		}
		_inviteHash = hash;
		auto box = Box<ConfirmInviteBox>(
			qs(d.vtitle),
			d.is_channel() && !d.is_megagroup(),
			d.vphoto,
			d.vparticipants_count.v,
			participants);
		Ui::show(std::move(box));
	} break;

	case mtpc_chatInviteAlready: {
		auto &d = invite.c_chatInviteAlready();
		if (auto chat = App::feedChat(d.vchat)) {
			_controller->showPeerHistory(
				chat,
				SectionShow::Way::Forward);
		}
	} break;
	}
}

bool MainWidget::inviteCheckFail(const RPCError &error) {
	if (MTP::isDefaultHandledError(error)) return false;

	if (error.code() == 400) {
		Ui::show(Box<InformBox>(lang(lng_group_invite_bad_link)));
	}
	return true;
}

void MainWidget::onInviteImport() {
	if (_inviteHash.isEmpty()) return;
	MTP::send(
		MTPmessages_ImportChatInvite(MTP_string(_inviteHash)),
		rpcDone(&MainWidget::inviteImportDone),
		rpcFail(&MainWidget::inviteImportFail));
}

void MainWidget::inviteImportDone(const MTPUpdates &updates) {
	App::main()->sentUpdatesReceived(updates);

	Ui::hideLayer();
	const QVector<MTPChat> *v = 0;
	switch (updates.type()) {
	case mtpc_updates: v = &updates.c_updates().vchats.v; break;
	case mtpc_updatesCombined: v = &updates.c_updatesCombined().vchats.v; break;
	default: LOG(("API Error: unexpected update cons %1 (MainWidget::inviteImportDone)").arg(updates.type())); break;
	}
	if (v && !v->isEmpty()) {
		const auto &mtpChat = v->front();
		const auto peerId = [&] {
			if (mtpChat.type() == mtpc_chat) {
				return peerFromChat(mtpChat.c_chat().vid.v);
			} else if (mtpChat.type() == mtpc_channel) {
				return peerFromChannel(mtpChat.c_channel().vid.v);
			}
			return PeerId(0);
		}();
		if (const auto peer = App::peerLoaded(peerId)) {
			_controller->showPeerHistory(
				peer,
				SectionShow::Way::Forward);
		}
	}
}

bool MainWidget::inviteImportFail(const RPCError &error) {
	if (MTP::isDefaultHandledError(error)) return false;

	if (error.type() == qstr("CHANNELS_TOO_MUCH")) {
		Ui::show(Box<InformBox>(lang(lng_join_channel_error)));
	} else if (error.code() == 400) {
		Ui::show(Box<InformBox>(lang(error.type() == qstr("USERS_TOO_MUCH") ? lng_group_invite_no_room : lng_group_invite_bad_link)));
	}

	return true;
}

void MainWidget::startWithSelf(const MTPUserFull &result) {
	Expects(result.type() == mtpc_userFull);
	auto &d = result.c_userFull();
	start(&d.vuser);
	if (auto user = App::self()) {
		Auth().api().processFullPeer(user, result);
	}
}

void MainWidget::incrementSticker(DocumentData *sticker) {
	if (!sticker || !sticker->sticker()) return;
	if (sticker->sticker()->set.type() == mtpc_inputStickerSetEmpty) return;

	bool writeRecentStickers = false;
	auto &sets = Auth().data().stickerSetsRef();
	auto it = sets.find(Stickers::CloudRecentSetId);
	if (it == sets.cend()) {
		if (it == sets.cend()) {
			it = sets.insert(Stickers::CloudRecentSetId, Stickers::Set(
				Stickers::CloudRecentSetId,
				uint64(0),
				lang(lng_recent_stickers),
				QString(),
				0, // count
				0, // hash
				MTPDstickerSet_ClientFlag::f_special | 0,
				TimeId(0)));
		} else {
			it->title = lang(lng_recent_stickers);
		}
	}
	auto removedFromEmoji = std::vector<not_null<EmojiPtr>>();
	auto index = it->stickers.indexOf(sticker);
	if (index > 0) {
		if (it->dates.empty()) {
			Auth().api().requestRecentStickersForce();
		} else {
			Assert(it->dates.size() == it->stickers.size());
			it->dates.erase(it->dates.begin() + index);
		}
		it->stickers.removeAt(index);
		for (auto i = it->emoji.begin(); i != it->emoji.end();) {
			if (const auto index = i->indexOf(sticker); index >= 0) {
				removedFromEmoji.push_back(i.key());
				i->removeAt(index);
				if (i->isEmpty()) {
					i = it->emoji.erase(i);
					continue;
				}
			}
			++i;
		}
	}
	if (index) {
		if (it->dates.size() == it->stickers.size()) {
			it->dates.insert(it->dates.begin(), unixtime());
		}
		it->stickers.push_front(sticker);
		if (const auto emojiList = Stickers::GetEmojiListFromSet(sticker)) {
			for (const auto emoji : *emojiList) {
				it->emoji[emoji].push_front(sticker);
			}
		} else if (!removedFromEmoji.empty()) {
			for (const auto emoji : removedFromEmoji) {
				it->emoji[emoji].push_front(sticker);
			}
		} else {
			Auth().api().requestRecentStickersForce();
		}

		writeRecentStickers = true;
	}

	// Remove that sticker from old recent, now it is in cloud recent stickers.
	bool writeOldRecent = false;
	auto &recent = Stickers::GetRecentPack();
	for (auto i = recent.begin(), e = recent.end(); i != e; ++i) {
		if (i->first == sticker) {
			writeOldRecent = true;
			recent.erase(i);
			break;
		}
	}
	while (!recent.isEmpty() && it->stickers.size() + recent.size() > Global::StickersRecentLimit()) {
		writeOldRecent = true;
		recent.pop_back();
	}

	if (writeOldRecent) {
		Local::writeUserSettings();
	}

	// Remove that sticker from custom stickers, now it is in cloud recent stickers.
	bool writeInstalledStickers = false;
	auto custom = sets.find(Stickers::CustomSetId);
	if (custom != sets.cend()) {
		int removeIndex = custom->stickers.indexOf(sticker);
		if (removeIndex >= 0) {
			custom->stickers.removeAt(removeIndex);
			if (custom->stickers.isEmpty()) {
				sets.erase(custom);
			}
			writeInstalledStickers = true;
		}
	}

	if (writeInstalledStickers) {
		Local::writeInstalledStickers();
	}
	if (writeRecentStickers) {
		Local::writeRecentStickers();
	}
	_history->updateRecentStickers();
}

void MainWidget::activate() {
	if (_a_show.animating()) return;
	if (!_mainSection) {
		if (_hider) {
			if (_hider->wasOffered()) {
				_hider->setFocus();
			} else {
				_dialogs->activate();
			}
        } else if (App::wnd() && !Ui::isLayerShown()) {
			if (!cSendPaths().isEmpty()) {
				showSendPathsLayer();
			} else if (_history->peer()) {
				_history->activate();
			} else {
				_dialogs->activate();
			}
		}
	}
	App::wnd()->fixOrder();
}

void MainWidget::destroyData() {
	_history->destroyData();
	_dialogs->destroyData();
}

bool MainWidget::isActive() const {
	return !_isIdle && isVisible() && !_a_show.animating();
}

bool MainWidget::doWeReadServerHistory() const {
	return isActive() && !_mainSection && _history->doWeReadServerHistory();
}

bool MainWidget::doWeReadMentions() const {
	return isActive() && !_mainSection && _history->doWeReadMentions();
}

bool MainWidget::lastWasOnline() const {
	return _lastWasOnline;
}

TimeMs MainWidget::lastSetOnline() const {
	return _lastSetOnline;
}

int32 MainWidget::dlgsWidth() const {
	return _dialogs->width();
}

MainWidget::~MainWidget() {
	if (App::main() == this) _history->showHistory(0, 0);

	if (HistoryHider *hider = _hider) {
		_hider = nullptr;
		delete hider;
	}
	Messenger::Instance().mtp()->clearGlobalHandlers();
}

void MainWidget::updateOnline(bool gotOtherOffline) {
	if (this != App::main()) return;
	InvokeQueued(this, [] { Auth().checkAutoLock(); });

	bool isOnline = App::wnd()->isActive();
	int updateIn = Global::OnlineUpdatePeriod();
	if (isOnline) {
		auto idle = psIdleTime();
		if (idle >= Global::OfflineIdleTimeout()) {
			isOnline = false;
			Bettergram::BettergramSettings::instance()->setIsWindowActive(false);
			if (!_isIdle) {
				_isIdle = true;
				_idleFinishTimer.start(900);
			}
		} else {
			updateIn = qMin(updateIn, int(Global::OfflineIdleTimeout() - idle));
		}
	}
	auto ms = getms(true);
	if (isOnline != _lastWasOnline
		|| (isOnline && _lastSetOnline + Global::OnlineUpdatePeriod() <= ms)
		|| (isOnline && gotOtherOffline)) {
		if (_onlineRequest) {
			MTP::cancel(_onlineRequest);
			_onlineRequest = 0;
		}

		_lastWasOnline = isOnline;
		_lastSetOnline = ms;
		_onlineRequest = MTP::send(MTPaccount_UpdateStatus(MTP_bool(!isOnline)));

		if (App::self()) {
			App::self()->onlineTill = unixtime() + (isOnline ? (Global::OnlineUpdatePeriod() / 1000) : -1);
			Notify::peerUpdatedDelayed(
				App::self(),
				Notify::PeerUpdate::Flag::UserOnlineChanged);
		}
		if (!isOnline) { // Went offline, so we need to save message draft to the cloud.
			saveDraftToCloud();
		}

		_lastSetOnline = ms;
	} else if (isOnline) {
		updateIn = qMin(updateIn, int(_lastSetOnline + Global::OnlineUpdatePeriod() - ms));
	}
	_onlineTimer.start(updateIn);
}

void MainWidget::saveDraftToCloud() {
	_history->saveFieldToHistoryLocalDraft();

	auto peer = _history->peer();
	if (auto history = App::historyLoaded(peer)) {
		writeDrafts(history);

		auto localDraft = history->localDraft();
		auto cloudDraft = history->cloudDraft();
		if (!Data::draftsAreEqual(localDraft, cloudDraft)) {
			Auth().api().saveDraftToCloudDelayed(history);
		}
	}
}

void MainWidget::applyCloudDraft(History *history) {
	_history->applyCloudDraft(history);
}

void MainWidget::writeDrafts(History *history) {
	Local::MessageDraft storedLocalDraft, storedEditDraft;
	MessageCursor localCursor, editCursor;
	if (auto localDraft = history->localDraft()) {
		if (!Data::draftsAreEqual(localDraft, history->cloudDraft())) {
			storedLocalDraft = Local::MessageDraft(localDraft->msgId, localDraft->textWithTags, localDraft->previewCancelled);
			localCursor = localDraft->cursor;
		}
	}
	if (auto editDraft = history->editDraft()) {
		storedEditDraft = Local::MessageDraft(editDraft->msgId, editDraft->textWithTags, editDraft->previewCancelled);
		editCursor = editDraft->cursor;
	}
	Local::writeDrafts(history->peer->id, storedLocalDraft, storedEditDraft);
	Local::writeDraftCursors(history->peer->id, localCursor, editCursor);
}

void MainWidget::checkIdleFinish() {
	if (this != App::main()) return;
	if (psIdleTime() < Global::OfflineIdleTimeout()) {
		_idleFinishTimer.stop();
		_isIdle = false;
		updateOnline();
		if (App::wnd()) App::wnd()->checkHistoryActivation();
	} else {
		_idleFinishTimer.start(900);
	}
}

void MainWidget::updateReceived(const mtpPrime *from, const mtpPrime *end) {
	if (end <= from) return;

	Auth().checkAutoLock();

	if (mtpTypeId(*from) == mtpc_new_session_created) {
		try {
			MTPNewSession newSession;
			newSession.read(from, end);
		} catch (mtpErrorUnexpected &) {
		}
		updSeq = 0;
		MTP_LOG(0, ("getDifference { after new_session_created }%1").arg(cTestMode() ? " TESTMODE" : ""));
		return getDifference();
	} else {
		try {
			MTPUpdates updates;
			updates.read(from, end);

			_lastUpdateTime = getms(true);
			noUpdatesTimer.start(NoUpdatesTimeout);
			if (!requestingDifference()
				|| HasForceLogoutNotification(updates)) {
				feedUpdates(updates);
			}
		} catch (mtpErrorUnexpected &) { // just some other type
		}
	}
	update();
}

namespace {

bool fwdInfoDataLoaded(const MTPMessageFwdHeader &header) {
	if (header.type() != mtpc_messageFwdHeader) {
		return true;
	}
	auto &info = header.c_messageFwdHeader();
	if (info.has_channel_id()) {
		if (!App::channelLoaded(peerFromChannel(info.vchannel_id))) {
			return false;
		}
		if (info.has_from_id() && !App::user(peerFromUser(info.vfrom_id), PeerData::MinimalLoaded)) {
			return false;
		}
	} else {
		if (info.has_from_id() && !App::userLoaded(peerFromUser(info.vfrom_id))) {
			return false;
		}
	}
	return true;
}

bool mentionUsersLoaded(const MTPVector<MTPMessageEntity> &entities) {
	for_const (auto &entity, entities.v) {
		auto type = entity.type();
		if (type == mtpc_messageEntityMentionName) {
			if (!App::userLoaded(peerFromUser(entity.c_messageEntityMentionName().vuser_id))) {
				return false;
			}
		} else if (type == mtpc_inputMessageEntityMentionName) {
			auto &inputUser = entity.c_inputMessageEntityMentionName().vuser_id;
			if (inputUser.type() == mtpc_inputUser) {
				if (!App::userLoaded(peerFromUser(inputUser.c_inputUser().vuser_id))) {
					return false;
				}
			}
		}
	}
	return true;
}

enum class DataIsLoadedResult {
	NotLoaded = 0,
	FromNotLoaded = 1,
	MentionNotLoaded = 2,
	Ok = 3,
};
DataIsLoadedResult allDataLoadedForMessage(const MTPMessage &msg) {
	switch (msg.type()) {
	case mtpc_message: {
		const MTPDmessage &d(msg.c_message());
		if (!d.is_post() && d.has_from_id()) {
			if (!App::userLoaded(peerFromUser(d.vfrom_id))) {
				return DataIsLoadedResult::FromNotLoaded;
			}
		}
		if (d.has_via_bot_id()) {
			if (!App::userLoaded(peerFromUser(d.vvia_bot_id))) {
				return DataIsLoadedResult::NotLoaded;
			}
		}
		if (d.has_fwd_from() && !fwdInfoDataLoaded(d.vfwd_from)) {
			return DataIsLoadedResult::NotLoaded;
		}
		if (d.has_entities() && !mentionUsersLoaded(d.ventities)) {
			return DataIsLoadedResult::MentionNotLoaded;
		}
	} break;
	case mtpc_messageService: {
		const MTPDmessageService &d(msg.c_messageService());
		if (!d.is_post() && d.has_from_id()) {
			if (!App::userLoaded(peerFromUser(d.vfrom_id))) {
				return DataIsLoadedResult::FromNotLoaded;
			}
		}
		switch (d.vaction.type()) {
		case mtpc_messageActionChatAddUser: {
			for_const (const MTPint &userId, d.vaction.c_messageActionChatAddUser().vusers.v) {
				if (!App::userLoaded(peerFromUser(userId))) {
					return DataIsLoadedResult::NotLoaded;
				}
			}
		} break;
		case mtpc_messageActionChatJoinedByLink: {
			if (!App::userLoaded(peerFromUser(d.vaction.c_messageActionChatJoinedByLink().vinviter_id))) {
				return DataIsLoadedResult::NotLoaded;
			}
		} break;
		case mtpc_messageActionChatDeleteUser: {
			if (!App::userLoaded(peerFromUser(d.vaction.c_messageActionChatDeleteUser().vuser_id))) {
				return DataIsLoadedResult::NotLoaded;
			}
		} break;
		}
	} break;
	}
	return DataIsLoadedResult::Ok;
}

} // namespace

void MainWidget::feedUpdates(const MTPUpdates &updates, uint64 randomId) {
	switch (updates.type()) {
	case mtpc_updates: {
		auto &d = updates.c_updates();
		if (d.vseq.v) {
			if (d.vseq.v <= updSeq) {
				return;
			}
			if (d.vseq.v > updSeq + 1) {
				_bySeqUpdates.insert(d.vseq.v, updates);
				return _bySeqTimer.start(WaitForSkippedTimeout);
			}
		}

		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		feedUpdateVector(d.vupdates);

		updSetState(0, d.vdate.v, updQts, d.vseq.v);
	} break;

	case mtpc_updatesCombined: {
		auto &d = updates.c_updatesCombined();
		if (d.vseq_start.v) {
			if (d.vseq_start.v <= updSeq) {
				return;
			}
			if (d.vseq_start.v > updSeq + 1) {
				_bySeqUpdates.insert(d.vseq_start.v, updates);
				return _bySeqTimer.start(WaitForSkippedTimeout);
			}
		}

		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		feedUpdateVector(d.vupdates);

		updSetState(0, d.vdate.v, updQts, d.vseq.v);
	} break;

	case mtpc_updateShort: {
		auto &d = updates.c_updateShort();
		feedUpdate(d.vupdate);

		updSetState(0, d.vdate.v, updQts, updSeq);
	} break;

	case mtpc_updateShortMessage: {
		auto &d = updates.c_updateShortMessage();
		if (!App::userLoaded(d.vuser_id.v)
			|| (d.has_via_bot_id() && !App::userLoaded(d.vvia_bot_id.v))
			|| (d.has_entities() && !mentionUsersLoaded(d.ventities))
			|| (d.has_fwd_from() && !fwdInfoDataLoaded(d.vfwd_from))) {
			MTP_LOG(0, ("getDifference { good - getting user for updateShortMessage }%1").arg(cTestMode() ? " TESTMODE" : ""));
			return getDifference();
		}
		if (ptsUpdateAndApply(d.vpts.v, d.vpts_count.v, updates)) {
			// Update date as well.
			updSetState(0, d.vdate.v, updQts, updSeq);
		}
	} break;

	case mtpc_updateShortChatMessage: {
		auto &d = updates.c_updateShortChatMessage();
		bool noFrom = !App::userLoaded(d.vfrom_id.v);
		if (!App::chatLoaded(d.vchat_id.v)
			|| noFrom
			|| (d.has_via_bot_id() && !App::userLoaded(d.vvia_bot_id.v))
			|| (d.has_entities() && !mentionUsersLoaded(d.ventities))
			|| (d.has_fwd_from() && !fwdInfoDataLoaded(d.vfwd_from))) {
			MTP_LOG(0, ("getDifference { good - getting user for updateShortChatMessage }%1").arg(cTestMode() ? " TESTMODE" : ""));
			if (noFrom) {
				Auth().api().requestFullPeer(App::chatLoaded(d.vchat_id.v));
			}
			return getDifference();
		}
		if (ptsUpdateAndApply(d.vpts.v, d.vpts_count.v, updates)) {
			// Update date as well.
			updSetState(0, d.vdate.v, updQts, updSeq);
		}
	} break;

	case mtpc_updateShortSentMessage: {
		auto &d = updates.c_updateShortSentMessage();
		if (!IsServerMsgId(d.vid.v)) {
			LOG(("API Error: Bad msgId got from server: %1").arg(d.vid.v));
		} else if (randomId) {
			PeerId peerId = 0;
			QString text;
			App::histSentDataByItem(randomId, peerId, text);

			const auto wasAlready = (peerId != 0)
				&& (App::histItemById(peerToChannel(peerId), d.vid.v) != nullptr);
			feedUpdate(MTP_updateMessageID(d.vid, MTP_long(randomId))); // ignore real date
			if (peerId) {
				if (auto item = App::histItemById(peerToChannel(peerId), d.vid.v)) {
					if (d.has_entities() && !mentionUsersLoaded(d.ventities)) {
						Auth().api().requestMessageData(
							item->history()->peer->asChannel(),
							item->id,
							ApiWrap::RequestMessageDataCallback());
					}
					const auto entities = d.has_entities()
						? TextUtilities::EntitiesFromMTP(d.ventities.v)
						: EntitiesInText();
					const auto media = d.has_media() ? &d.vmedia : nullptr;
					item->setText({ text, entities });
					item->updateSentMedia(media);
					if (!wasAlready) {
						item->indexAsNewItem();
					}
				}
			}
		}

		if (ptsUpdateAndApply(d.vpts.v, d.vpts_count.v, updates)) {
			// Update date as well.
			updSetState(0, d.vdate.v, updQts, updSeq);
		}
	} break;

	case mtpc_updatesTooLong: {
		MTP_LOG(0, ("getDifference { good - updatesTooLong received }%1").arg(cTestMode() ? " TESTMODE" : ""));
		return getDifference();
	} break;
	}
	Auth().data().sendHistoryChangeNotifications();
}

void MainWidget::feedUpdate(const MTPUpdate &update) {
	switch (update.type()) {

	// New messages.
	case mtpc_updateNewMessage: {
		auto &d = update.c_updateNewMessage();

		DataIsLoadedResult isDataLoaded = allDataLoadedForMessage(d.vmessage);
		if (!requestingDifference() && isDataLoaded != DataIsLoadedResult::Ok) {
			MTP_LOG(0, ("getDifference { good - "
				"after not all data loaded in updateNewMessage }%1"
				).arg(cTestMode() ? " TESTMODE" : ""));

			// This can be if this update was created by grouping
			// some short message update into an updates vector.
			return getDifference();
		}

		ptsUpdateAndApply(d.vpts.v, d.vpts_count.v, update);
	} break;

	case mtpc_updateNewChannelMessage: {
		auto &d = update.c_updateNewChannelMessage();
		auto channel = App::channelLoaded(peerToChannel(peerFromMessage(d.vmessage)));
		auto isDataLoaded = allDataLoadedForMessage(d.vmessage);
		if (!requestingDifference() && (!channel || isDataLoaded != DataIsLoadedResult::Ok)) {
			MTP_LOG(0, ("getDifference { good - "
				"after not all data loaded in updateNewChannelMessage }%1"
				).arg(cTestMode() ? " TESTMODE" : ""));

			// Request last active supergroup participants if the 'from' user was not loaded yet.
			// This will optimize similar getDifference() calls for almost all next messages.
			if (isDataLoaded == DataIsLoadedResult::FromNotLoaded && channel && channel->isMegagroup()) {
				if (channel->mgInfo->lastParticipants.size() < Global::ChatSizeMax() && (channel->mgInfo->lastParticipants.empty() || channel->mgInfo->lastParticipants.size() < channel->membersCount())) {
					Auth().api().requestLastParticipants(channel);
				}
			}

			if (!_byMinChannelTimer.isActive()) { // getDifference after timeout
				_byMinChannelTimer.start(WaitForSkippedTimeout);
			}
			return;
		}
		if (channel && !_handlingChannelDifference) {
			if (channel->ptsRequesting()) { // skip global updates while getting channel difference
				return;
			}
			channel->ptsUpdateAndApply(d.vpts.v, d.vpts_count.v, update);
		} else {
			Auth().api().applyUpdateNoPtsCheck(update);
		}
	} break;

	case mtpc_updateMessageID: {
		const auto &d = update.c_updateMessageID();
		if (const auto fullId = App::histItemByRandom(d.vrandom_id.v)) {
			const auto channel = fullId.channel;
			const auto newId = d.vid.v;
			if (const auto local = App::histItemById(fullId)) {
				const auto existing = App::histItemById(channel, newId);
				if (existing && !local->mainView()) {
					const auto history = local->history();
					local->destroy();
					if (!history->lastMessageKnown()) {
						Auth().api().requestDialogEntry(history);
					}
				} else {
					if (existing) {
						existing->destroy();
					}
					local->setRealId(d.vid.v);
				}
			}
			App::historyUnregRandom(d.vrandom_id.v);
		}
		App::historyUnregSentData(d.vrandom_id.v);
	} break;

	// Message contents being read.
	case mtpc_updateReadMessagesContents: {
		auto &d = update.c_updateReadMessagesContents();
		ptsUpdateAndApply(d.vpts.v, d.vpts_count.v, update);
	} break;

	case mtpc_updateChannelReadMessagesContents: {
		auto &d = update.c_updateChannelReadMessagesContents();
		auto channel = App::channelLoaded(d.vchannel_id.v);
		if (!channel) {
			if (!_byMinChannelTimer.isActive()) {
				// getDifference after timeout.
				_byMinChannelTimer.start(WaitForSkippedTimeout);
			}
			return;
		}
		auto possiblyReadMentions = base::flat_set<MsgId>();
		for_const (auto &msgId, d.vmessages.v) {
			if (auto item = App::histItemById(channel, msgId.v)) {
				if (item->isMediaUnread()) {
					item->markMediaRead();
					Auth().data().requestItemRepaint(item);
				}
			} else {
				// Perhaps it was an unread mention!
				possiblyReadMentions.insert(msgId.v);
			}
		}
		Auth().api().checkForUnreadMentions(possiblyReadMentions, channel);
	} break;

	// Edited messages.
	case mtpc_updateEditMessage: {
		auto &d = update.c_updateEditMessage();
		ptsUpdateAndApply(d.vpts.v, d.vpts_count.v, update);
	} break;

	case mtpc_updateEditChannelMessage: {
		auto &d = update.c_updateEditChannelMessage();
		auto channel = App::channelLoaded(peerToChannel(peerFromMessage(d.vmessage)));

		if (channel && !_handlingChannelDifference) {
			if (channel->ptsRequesting()) { // skip global updates while getting channel difference
				return;
			} else {
				channel->ptsUpdateAndApply(d.vpts.v, d.vpts_count.v, update);
			}
		} else {
			Auth().api().applyUpdateNoPtsCheck(update);
		}
	} break;

	// Messages being read.
	case mtpc_updateReadHistoryInbox: {
		auto &d = update.c_updateReadHistoryInbox();
		ptsUpdateAndApply(d.vpts.v, d.vpts_count.v, update);
	} break;

	case mtpc_updateReadHistoryOutbox: {
		auto &d = update.c_updateReadHistoryOutbox();
		if (ptsUpdateAndApply(d.vpts.v, d.vpts_count.v, update)) {
			// We could've updated the double checks.
			// Better would be for history to be subscribed to outbox read events.
			_history->update();
		}
	} break;

	case mtpc_updateReadChannelInbox: {
		auto &d = update.c_updateReadChannelInbox();
		App::feedInboxRead(peerFromChannel(d.vchannel_id.v), d.vmax_id.v);
	} break;

	case mtpc_updateReadChannelOutbox: {
		auto &d = update.c_updateReadChannelOutbox();
		auto peerId = peerFromChannel(d.vchannel_id.v);
		auto when = requestingDifference() ? 0 : unixtime();
		App::feedOutboxRead(peerId, d.vmax_id.v, when);
		if (_history->peer() && _history->peer()->id == peerId) {
			_history->update();
		}
	} break;

	//case mtpc_updateReadFeed: { // #feed
	//	const auto &d = update.c_updateReadFeed();
	//	const auto feedId = d.vfeed_id.v;
	//	if (const auto feed = Auth().data().feedLoaded(feedId)) {
	//		feed->setUnreadPosition(
	//			Data::FeedPositionFromMTP(d.vmax_position));
	//		if (d.has_unread_count() && d.has_unread_muted_count()) {
	//			feed->setUnreadCounts(
	//				d.vunread_count.v,
	//				d.vunread_muted_count.v);
	//		} else {
	//			Auth().api().requestDialogEntry(feed);
	//		}
	//	}
	//} break;

	case mtpc_updateDialogUnreadMark: {
		const auto &data = update.c_updateDialogUnreadMark();
		const auto history = data.vpeer.match(
		[&](const MTPDdialogPeer &data) {
			const auto peerId = peerFromMTP(data.vpeer);
			return App::historyLoaded(peerId);
		//}, [&](const MTPDdialogPeerFeed &data) { // #feed
		});
		if (history) {
			history->setUnreadMark(data.is_unread());
		}
	} break;

	// Deleted messages.
	case mtpc_updateDeleteMessages: {
		auto &d = update.c_updateDeleteMessages();

		ptsUpdateAndApply(d.vpts.v, d.vpts_count.v, update);
	} break;

	case mtpc_updateDeleteChannelMessages: {
		auto &d = update.c_updateDeleteChannelMessages();
		auto channel = App::channelLoaded(d.vchannel_id.v);

		if (channel && !_handlingChannelDifference) {
			if (channel->ptsRequesting()) { // skip global updates while getting channel difference
				return;
			}
			channel->ptsUpdateAndApply(d.vpts.v, d.vpts_count.v, update);
		} else {
			Auth().api().applyUpdateNoPtsCheck(update);
		}
	} break;

	case mtpc_updateWebPage: {
		auto &d = update.c_updateWebPage();

		// Update web page anyway.
		Auth().data().webpage(d.vwebpage);
		_history->updatePreview();
		Auth().data().sendWebPageGameNotifications();

		ptsUpdateAndApply(d.vpts.v, d.vpts_count.v, update);
	} break;

	case mtpc_updateChannelWebPage: {
		auto &d = update.c_updateChannelWebPage();

		// Update web page anyway.
		Auth().data().webpage(d.vwebpage);
		_history->updatePreview();
		Auth().data().sendWebPageGameNotifications();

		auto channel = App::channelLoaded(d.vchannel_id.v);
		if (channel && !_handlingChannelDifference) {
			if (channel->ptsRequesting()) { // skip global updates while getting channel difference
				return;
			} else {
				channel->ptsUpdateAndApply(d.vpts.v, d.vpts_count.v, update);
			}
		} else {
			Auth().api().applyUpdateNoPtsCheck(update);
		}
	} break;

	case mtpc_updateUserTyping: {
		auto &d = update.c_updateUserTyping();
		const auto userId = peerFromUser(d.vuser_id);
		const auto history = App::historyLoaded(userId);
		const auto user = App::userLoaded(d.vuser_id.v);
		if (history && user) {
			const auto when = requestingDifference() ? 0 : unixtime();
			App::histories().registerSendAction(history, user, d.vaction, when);
		}
	} break;

	case mtpc_updateChatUserTyping: {
		auto &d = update.c_updateChatUserTyping();
		const auto history = [&]() -> History* {
			if (auto chat = App::chatLoaded(d.vchat_id.v)) {
				return App::historyLoaded(chat->id);
			} else if (auto channel = App::channelLoaded(d.vchat_id.v)) {
				return App::historyLoaded(channel->id);
			}
			return nullptr;
		}();
		const auto user = (d.vuser_id.v == Auth().userId())
			? nullptr
			: App::userLoaded(d.vuser_id.v);
		if (history && user) {
			const auto when = requestingDifference() ? 0 : unixtime();
			App::histories().registerSendAction(history, user, d.vaction, when);
		}
	} break;

	case mtpc_updateChatParticipants: {
		App::feedParticipants(update.c_updateChatParticipants().vparticipants, true);
	} break;

	case mtpc_updateChatParticipantAdd: {
		App::feedParticipantAdd(update.c_updateChatParticipantAdd());
	} break;

	case mtpc_updateChatParticipantDelete: {
		App::feedParticipantDelete(update.c_updateChatParticipantDelete());
	} break;

	case mtpc_updateChatAdmins: {
		App::feedChatAdmins(update.c_updateChatAdmins());
	} break;

	case mtpc_updateChatParticipantAdmin: {
		App::feedParticipantAdmin(update.c_updateChatParticipantAdmin());
	} break;

	case mtpc_updateUserStatus: {
		auto &d = update.c_updateUserStatus();
		if (auto user = App::userLoaded(d.vuser_id.v)) {
			switch (d.vstatus.type()) {
			case mtpc_userStatusEmpty: user->onlineTill = 0; break;
			case mtpc_userStatusRecently:
				if (user->onlineTill > -10) { // don't modify pseudo-online
					user->onlineTill = -2;
				}
			break;
			case mtpc_userStatusLastWeek: user->onlineTill = -3; break;
			case mtpc_userStatusLastMonth: user->onlineTill = -4; break;
			case mtpc_userStatusOffline: user->onlineTill = d.vstatus.c_userStatusOffline().vwas_online.v; break;
			case mtpc_userStatusOnline: user->onlineTill = d.vstatus.c_userStatusOnline().vexpires.v; break;
			}
			Notify::peerUpdatedDelayed(
				user,
				Notify::PeerUpdate::Flag::UserOnlineChanged);
		}
		if (d.vuser_id.v == Auth().userId()) {
			if (d.vstatus.type() == mtpc_userStatusOffline || d.vstatus.type() == mtpc_userStatusEmpty) {
				updateOnline(true);
				if (d.vstatus.type() == mtpc_userStatusOffline) {
					cSetOtherOnline(d.vstatus.c_userStatusOffline().vwas_online.v);
				}
			} else if (d.vstatus.type() == mtpc_userStatusOnline) {
				cSetOtherOnline(d.vstatus.c_userStatusOnline().vexpires.v);
			}
		}
	} break;

	case mtpc_updateUserName: {
		auto &d = update.c_updateUserName();
		if (auto user = App::userLoaded(d.vuser_id.v)) {
			if (user->contactStatus() != UserData::ContactStatus::Contact) {
				user->setName(
					TextUtilities::SingleLine(qs(d.vfirst_name)),
					TextUtilities::SingleLine(qs(d.vlast_name)),
					user->nameOrPhone,
					TextUtilities::SingleLine(qs(d.vusername)));
			} else {
				user->setName(
					TextUtilities::SingleLine(user->firstName),
					TextUtilities::SingleLine(user->lastName),
					user->nameOrPhone,
					TextUtilities::SingleLine(qs(d.vusername)));
			}
		}
	} break;

	case mtpc_updateUserPhoto: {
		auto &d = update.c_updateUserPhoto();
		if (auto user = App::userLoaded(d.vuser_id.v)) {
			user->setPhoto(d.vphoto);
			user->loadUserpic();
			if (mtpIsTrue(d.vprevious) || !user->userpicPhotoId()) {
				Auth().storage().remove(Storage::UserPhotosRemoveAfter(
					user->bareId(),
					user->userpicPhotoId()));
			} else {
				Auth().storage().add(Storage::UserPhotosAddNew(
					user->bareId(),
					user->userpicPhotoId()));
			}
		}
	} break;

	case mtpc_updateContactRegistered: {
		const auto &d = update.c_updateContactRegistered();
		if (const auto user = App::userLoaded(d.vuser_id.v)) {
			if (App::history(user->id)->loadedAtBottom()) {
				App::history(user->id)->addNewService(
					clientMsgId(),
					d.vdate.v,
					lng_action_user_registered(lt_from, user->name),
					MTPDmessage::Flags(0));
			}
		}
	} break;

	case mtpc_updateContactLink: {
		const auto &d = update.c_updateContactLink();
		App::feedUserLink(d.vuser_id, d.vmy_link, d.vforeign_link);
	} break;

	case mtpc_updateNotifySettings: {
		auto &d = update.c_updateNotifySettings();
		Auth().data().applyNotifySetting(d.vpeer, d.vnotify_settings);
	} break;

	case mtpc_updateDcOptions: {
		auto &d = update.c_updateDcOptions();
		Messenger::Instance().dcOptions()->addFromList(d.vdc_options);
	} break;

	case mtpc_updateConfig: {
		Messenger::Instance().mtp()->requestConfig();
	} break;

	case mtpc_updateUserPhone: {
		auto &d = update.c_updateUserPhone();
		if (auto user = App::userLoaded(d.vuser_id.v)) {
			auto newPhone = qs(d.vphone);
			if (newPhone != user->phone()) {
				user->setPhone(newPhone);
				user->setName(
					user->firstName,
					user->lastName,
					((user->contactStatus() == UserData::ContactStatus::Contact
						|| isServiceUser(user->id)
						|| user->isSelf()
						|| user->phone().isEmpty())
						? QString()
						: App::formatPhone(user->phone())),
					user->username);

				Notify::peerUpdatedDelayed(
					user,
					Notify::PeerUpdate::Flag::UserPhoneChanged);
			}
		}
	} break;

	case mtpc_updateNewEncryptedMessage: {
		auto &d = update.c_updateNewEncryptedMessage();
	} break;

	case mtpc_updateEncryptedChatTyping: {
		auto &d = update.c_updateEncryptedChatTyping();
	} break;

	case mtpc_updateEncryption: {
		auto &d = update.c_updateEncryption();
	} break;

	case mtpc_updateEncryptedMessagesRead: {
		auto &d = update.c_updateEncryptedMessagesRead();
	} break;

	case mtpc_updatePhoneCall: {
		Calls::Current().handleUpdate(update.c_updatePhoneCall());
	} break;

	case mtpc_updateUserBlocked: {
		auto &d = update.c_updateUserBlocked();
		if (auto user = App::userLoaded(d.vuser_id.v)) {
			user->setBlockStatus(mtpIsTrue(d.vblocked) ? UserData::BlockStatus::Blocked : UserData::BlockStatus::NotBlocked);
		}
	} break;

	case mtpc_updateServiceNotification: {
		const auto &d = update.c_updateServiceNotification();
		const auto text = TextWithEntities {
			qs(d.vmessage),
			TextUtilities::EntitiesFromMTP(d.ventities.v)
		};
		if (IsForceLogoutNotification(d)) {
			Messenger::Instance().forceLogOut(text);
		} else if (d.is_popup()) {
			Ui::show(Box<InformBox>(text));
		} else {
			App::wnd()->serviceNotification(text, d.vmedia);
			emit App::wnd()->checkNewAuthorization();
		}
	} break;

	case mtpc_updatePrivacy: {
		auto &d = update.c_updatePrivacy();
		Auth().api().handlePrivacyChange(d.vkey.type(), d.vrules);
	} break;

	case mtpc_updatePinnedDialogs: {
		const auto &d = update.c_updatePinnedDialogs();
		if (d.has_order()) {
			const auto &order = d.vorder.v;
			const auto allLoaded = [&] {
				for (const auto &dialogPeer : order) {
					switch (dialogPeer.type()) {
					case mtpc_dialogPeer: {
						const auto &peer = dialogPeer.c_dialogPeer();
						const auto peerId = peerFromMTP(peer.vpeer);
						if (!App::historyLoaded(peerId)) {
							DEBUG_LOG(("API Error: "
								"pinned chat not loaded for peer %1"
								).arg(peerId
								));
							return false;
						}
					} break;
					//case mtpc_dialogPeerFeed: { // #feed
					//	const auto &feed = dialogPeer.c_dialogPeerFeed();
					//	const auto feedId = feed.vfeed_id.v;
					//	if (!Auth().data().feedLoaded(feedId)) {
					//		DEBUG_LOG(("API Error: "
					//			"pinned feed not loaded for feedId %1"
					//			).arg(feedId
					//			));
					//		return false;
					//	}
					//} break;
					}
				}
				return true;
			}();
			if (allLoaded) {
				Auth().data().applyPinnedDialogs(order);
			} else {
				_dialogs->loadPinnedDialogs();
			}
		} else {
			_dialogs->loadPinnedDialogs();
		}
	} break;

	case mtpc_updateDialogPinned: {
		const auto &d = update.c_updateDialogPinned();
		switch (d.vpeer.type()) {
		case mtpc_dialogPeer: {
			//In Bettergram we should not send or receive pin information
			//const auto peerId = peerFromMTP(d.vpeer.c_dialogPeer().vpeer);
			//if (const auto history = App::historyLoaded(peerId)) {
			//	Auth().data().setPinnedDialog(history, d.is_pinned());
			//} else {
			//	DEBUG_LOG(("API Error: "
			//		"pinned chat not loaded for peer %1"
			//		).arg(peerId
			//		));
			//	_dialogs->loadPinnedDialogs();
			//}
		} break;
		//case mtpc_dialogPeerFeed: { // #feed
		//	const auto feedId = d.vpeer.c_dialogPeerFeed().vfeed_id.v;
		//	if (const auto feed = Auth().data().feedLoaded(feedId)) {
		//		Auth().data().setPinnedDialog(feed, d.is_pinned());
		//	} else {
		//		DEBUG_LOG(("API Error: "
		//			"pinned feed not loaded for feedId %1"
		//			).arg(feedId
		//			));
		//		_dialogs->loadPinnedDialogs();
		//	}
		//} break;
		}
	} break;

	case mtpc_updateChannel: {
		auto &d = update.c_updateChannel();
		if (const auto channel = App::channelLoaded(d.vchannel_id.v)) {
			channel->inviter = UserId(0);
			if (channel->amIn()
				&& !channel->amCreator()
				&& App::history(channel->id)) {
				_updatedChannels.insert(channel, true);
				Auth().api().requestSelfParticipant(channel);
			}
			if (const auto feed = channel->feed()) {
				if (!feed->lastMessageKnown()
					|| !feed->unreadCountKnown()) {
					Auth().api().requestDialogEntry(feed);
				}
			} else if (channel->amIn()) {
				const auto history = App::history(channel->id);
				if (!history->lastMessageKnown()
					|| !history->unreadCountKnown()) {
					Auth().api().requestDialogEntry(history);
				}
			}
		}
	} break;

	case mtpc_updateChannelPinnedMessage: {
		auto &d = update.c_updateChannelPinnedMessage();
		if (auto channel = App::channelLoaded(d.vchannel_id.v)) {
			channel->setPinnedMessageId(d.vid.v);
		}
	} break;

	case mtpc_updateChannelTooLong: {
		auto &d = update.c_updateChannelTooLong();
		if (auto channel = App::channelLoaded(d.vchannel_id.v)) {
			if (!d.has_pts() || channel->pts() < d.vpts.v) {
				getChannelDifference(channel);
			}
		}
	} break;

	case mtpc_updateChannelMessageViews: {
		auto &d = update.c_updateChannelMessageViews();
		if (auto item = App::histItemById(d.vchannel_id.v, d.vid.v)) {
			item->setViewsCount(d.vviews.v);
		}
	} break;

	case mtpc_updateChannelAvailableMessages: {
		auto &d = update.c_updateChannelAvailableMessages();
		if (auto channel = App::channelLoaded(d.vchannel_id.v)) {
			channel->setAvailableMinId(d.vavailable_min_id.v);
		}
	} break;

	////// Cloud sticker sets
	case mtpc_updateNewStickerSet: {
		auto &d = update.c_updateNewStickerSet();
		bool writeArchived = false;
		if (d.vstickerset.type() == mtpc_messages_stickerSet) {
			auto &set = d.vstickerset.c_messages_stickerSet();
			if (set.vset.type() == mtpc_stickerSet) {
				auto &s = set.vset.c_stickerSet();
				if (!s.has_installed_date()) {
					LOG(("API Error: "
						"updateNewStickerSet without install_date flag."));
				}
				if (!s.is_masks()) {
					auto &sets = Auth().data().stickerSetsRef();
					auto it = sets.find(s.vid.v);
					if (it == sets.cend()) {
						it = sets.insert(s.vid.v, Stickers::Set(
							s.vid.v,
							s.vaccess_hash.v,
							Stickers::GetSetTitle(s),
							qs(s.vshort_name),
							s.vcount.v,
							s.vhash.v,
							s.vflags.v | MTPDstickerSet::Flag::f_installed_date,
							s.has_installed_date() ? s.vinstalled_date.v : unixtime()));
					} else {
						it->flags |= MTPDstickerSet::Flag::f_installed_date;
						if (!it->installDate) {
							it->installDate = unixtime();
						}
						if (it->flags & MTPDstickerSet::Flag::f_archived) {
							it->flags &= ~MTPDstickerSet::Flag::f_archived;
							writeArchived = true;
						}
					}
					auto inputSet = MTP_inputStickerSetID(MTP_long(it->id), MTP_long(it->access));
					auto &v = set.vdocuments.v;
					it->stickers.clear();
					it->stickers.reserve(v.size());
					for (int i = 0, l = v.size(); i < l; ++i) {
						const auto doc = Auth().data().document(v.at(i));
						if (!doc->sticker()) continue;

						it->stickers.push_back(doc);
						if (doc->sticker()->set.type() != mtpc_inputStickerSetID) {
							doc->sticker()->set = inputSet;
						}
					}
					it->emoji.clear();
					auto &packs = set.vpacks.v;
					for (auto i = 0, l = packs.size(); i != l; ++i) {
						if (packs[i].type() != mtpc_stickerPack) continue;
						auto &pack = packs.at(i).c_stickerPack();
						if (auto emoji = Ui::Emoji::Find(qs(pack.vemoticon))) {
							emoji = emoji->original();
							auto &stickers = pack.vdocuments.v;

							Stickers::Pack p;
							p.reserve(stickers.size());
							for (auto j = 0, c = stickers.size(); j != c; ++j) {
								auto doc = Auth().data().document(stickers[j].v);
								if (!doc->sticker()) continue;

								p.push_back(doc);
							}
							it->emoji.insert(emoji, p);
						}
					}

					auto &order = Auth().data().stickerSetsOrderRef();
					int32 insertAtIndex = 0, currentIndex = order.indexOf(s.vid.v);
					if (currentIndex != insertAtIndex) {
						if (currentIndex > 0) {
							order.removeAt(currentIndex);
						}
						order.insert(insertAtIndex, s.vid.v);
					}

					auto custom = sets.find(Stickers::CustomSetId);
					if (custom != sets.cend()) {
						for (int32 i = 0, l = it->stickers.size(); i < l; ++i) {
							int32 removeIndex = custom->stickers.indexOf(it->stickers.at(i));
							if (removeIndex >= 0) custom->stickers.removeAt(removeIndex);
						}
						if (custom->stickers.isEmpty()) {
							sets.erase(custom);
						}
					}
					Local::writeInstalledStickers();
					if (writeArchived) Local::writeArchivedStickers();
					Auth().data().notifyStickersUpdated();
				}
			}
		}
	} break;

	case mtpc_updateStickerSetsOrder: {
		auto &d = update.c_updateStickerSetsOrder();
		if (!d.is_masks()) {
			auto &order = d.vorder.v;
			auto &sets = Auth().data().stickerSets();
			Stickers::Order result;
			for (int i = 0, l = order.size(); i < l; ++i) {
				if (sets.constFind(order.at(i).v) == sets.cend()) {
					break;
				}
				result.push_back(order.at(i).v);
			}
			if (result.size() != Auth().data().stickerSetsOrder().size() || result.size() != order.size()) {
				Auth().data().setLastStickersUpdate(0);
				Auth().api().updateStickers();
			} else {
				Auth().data().stickerSetsOrderRef() = std::move(result);
				Local::writeInstalledStickers();
				Auth().data().notifyStickersUpdated();
			}
		}
	} break;

	case mtpc_updateStickerSets: {
		Auth().data().setLastStickersUpdate(0);
		Auth().api().updateStickers();
	} break;

	case mtpc_updateRecentStickers: {
		Auth().data().setLastRecentStickersUpdate(0);
		Auth().api().updateStickers();
	} break;

	case mtpc_updateFavedStickers: {
		Auth().data().setLastFavedStickersUpdate(0);
		Auth().api().updateStickers();
	} break;

	case mtpc_updateReadFeaturedStickers: {
		// We read some of the featured stickers, perhaps not all of them.
		// Here we don't know what featured sticker sets were read, so we
		// request all of them once again.
		Auth().data().setLastFeaturedStickersUpdate(0);
		Auth().api().updateStickers();
	} break;

	////// Cloud saved GIFs
	case mtpc_updateSavedGifs: {
		Auth().data().setLastSavedGifsUpdate(0);
		Auth().api().updateStickers();
	} break;

	////// Cloud drafts
	case mtpc_updateDraftMessage: {
		const auto &data = update.c_updateDraftMessage();
		const auto peerId = peerFromMTP(data.vpeer);
		data.vdraft.match([&](const MTPDdraftMessage &data) {
			Data::applyPeerCloudDraft(peerId, data);
		}, [&](const MTPDdraftMessageEmpty &data) {
			Data::clearPeerCloudDraft(
				peerId,
				TimeId(data.has_date() ? data.vdate.v : 0));
		});
	} break;

	////// Cloud langpacks
	case mtpc_updateLangPack: {
		auto &langpack = update.c_updateLangPack();
		Lang::CurrentCloudManager().applyLangPackDifference(langpack.vdifference);
	} break;

	case mtpc_updateLangPackTooLong: {
		Lang::CurrentCloudManager().requestLangPackDifference();
	} break;

	}
}
