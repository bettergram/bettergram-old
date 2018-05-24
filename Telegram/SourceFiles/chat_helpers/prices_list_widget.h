#pragma once
#include "chat_helpers/tabbed_selector.h"

namespace Window {
class Controller;
} // namespace Window

namespace Ui {
class FlatLabel;
} // namespace Ui

namespace ChatHelpers {

/**
 * @brief The PricesListWidget class shows cryptocurrency price list.
 * In normal Qt application we should use QTableView, but it would be strange for this application
 * because it uses low level painting for drawing custom widgets.
 */
class PricesListWidget : public TabbedSelector::Inner
{
	Q_OBJECT

public:
	PricesListWidget(QWidget* parent, not_null<Window::Controller*> controller);

	void refreshRecent() override;
	void clearSelection() override;
	object_ptr<TabbedSelector::InnerFooter> createFooter() override;

	void afterShown() override;
	void beforeHiding() override;

public slots:

//signals:

protected:
	TabbedSelector::InnerFooter* getFooter() const override;
	int countDesiredHeight(int newWidth) override;
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *e) override;
	void timerEvent(QTimerEvent *event) override;

private:
	class Footer;

	int _timerIntervalMs = 5000;
	int _timerId = -1;

	Ui::FlatLabel *_siteName;
	Ui::FlatLabel *_marketCap;
	Footer *_footer = nullptr;

	void updateControlsGeometry();
};

} // namespace ChatHelpers
