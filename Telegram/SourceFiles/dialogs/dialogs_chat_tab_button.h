/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/

#pragma once

#include "ui/widgets/buttons.h"

namespace Dialogs {

/**
 * @brief The ChatTabButton class is used to show chat tabs icons.
 * If user unselects a tab the icon becomes to unselected state immediately.
 */
class ChatTabButton : public Ui::IconButton {
	Q_OBJECT

public:
	ChatTabButton(QWidget *parent, const style::IconButton &st);

	bool selected() const;
	void setSelected(bool selected);

	void select();
	void unselect();

signals:

protected:
	void enterEventHook(QEvent *e) override;
	void leaveEventHook(QEvent *e) override;

private:
	bool _selected = false;
};

} // namespace Dialogs
