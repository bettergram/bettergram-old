/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/

#pragma once

#include "window/section_widget.h"

namespace Ui {
class IconButton;
} // namespace Ui

namespace Dialogs {

class ChatTabs : public TWidget {
	Q_OBJECT

public:
	explicit ChatTabs(QWidget *parent);

signals:

public slots:

private slots:

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	object_ptr<Ui::IconButton> _favoritesButton;
	object_ptr<Ui::IconButton> _groupButton;
	object_ptr<Ui::IconButton> _oneOnOneButton;
	object_ptr<Ui::IconButton> _announcementsButton;

	QList<Ui::IconButton*> _listButtons;

	void updateControlsGeometry();
};

} // namespace Dialogs
