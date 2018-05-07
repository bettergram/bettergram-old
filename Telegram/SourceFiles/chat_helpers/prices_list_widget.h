#pragma once
#include "chat_helpers/tabbed_selector.h"

namespace Window {
class Controller;
} // namespace Window

namespace ChatHelpers {

class PricesListWidget : public TabbedSelector::Inner
{
   Q_OBJECT

public:
   PricesListWidget(QWidget* parent, not_null<Window::Controller*> controller);

   using Section = Ui::Emoji::Section;

   void refreshRecent() override;
   void clearSelection() override;
   object_ptr<TabbedSelector::InnerFooter> createFooter() override;

public slots:

//signals:

protected:
   TabbedSelector::InnerFooter* getFooter() const override;
   int countDesiredHeight(int newWidth) override;

private:
   class Footer;

   Footer   *  _footer = nullptr;

};

}