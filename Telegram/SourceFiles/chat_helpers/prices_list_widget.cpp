#include "chat_helpers/prices_list_widget.h"


namespace ChatHelpers {

class PricesListWidget::Footer : public TabbedSelector::InnerFooter
{
public:
   Footer(not_null<PricesListWidget*> parent);

protected:
   void processPanelHideFinished() override;
   void resizeEvent(QResizeEvent* e) override;
};

PricesListWidget::Footer::Footer(not_null<PricesListWidget*> parent)
   :  InnerFooter(parent)
{
}

void PricesListWidget::Footer::resizeEvent(QResizeEvent* e)
{
}

void PricesListWidget::Footer::processPanelHideFinished()
{
}

PricesListWidget::PricesListWidget(QWidget* parent, not_null<Window::Controller*> controller)
   :  Inner(parent, controller)
{
}

void PricesListWidget::refreshRecent()
{
}

void PricesListWidget::clearSelection()
{
}

object_ptr<TabbedSelector::InnerFooter> PricesListWidget::createFooter()
{
   Expects(_footer == nullptr);

   auto  res = object_ptr<Footer>(this);

   _footer = res;
   return std::move(res);
}

TabbedSelector::InnerFooter* PricesListWidget::getFooter() const
{ return _footer; }

int PricesListWidget::countDesiredHeight(int newWidth)
{
   return 1;
}

} // namespace ChatHelpers