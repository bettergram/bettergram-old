#pragma once
#include "chat_helpers/tabbed_selector.h"

namespace Window {
class Controller;
} // namespace Window

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

public slots:

//signals:

protected:
	TabbedSelector::InnerFooter* getFooter() const override;
	int countDesiredHeight(int newWidth) override;
	void paintEvent(QPaintEvent *event) override;

private:
	class Footer;

	Footer *_footer = nullptr;
};

} // namespace ChatHelpers
