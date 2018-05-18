#include "chat_helpers/prices_list_widget.h"

#include "ui/widgets/buttons.h"
#include "lang/lang_keys.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_widgets.h"

namespace ChatHelpers {

class PricesListWidget::Footer : public TabbedSelector::InnerFooter
{
public:
   Footer(not_null<PricesListWidget*> parent);

protected:
   void processPanelHideFinished() override;
   void resizeEvent(QResizeEvent* e) override;
   void customizeClick();

private:
   not_null<PricesListWidget*>   _parent;
   object_ptr<Ui::LinkButton>    _link;
};

PricesListWidget::Footer::Footer(not_null<PricesListWidget*> parent)
   :  InnerFooter(parent)
   ,  _parent(parent)
   ,  _link(object_ptr<Ui::LinkButton>(this, lang(lng_prices_customize_list), st::largeLinkButton))
{
   _link->setClickedCallback([this] { customizeClick(); });
}

void PricesListWidget::Footer::resizeEvent(QResizeEvent* e)
{
   _link->move(rect().center() - _link->rect().center());
}

void PricesListWidget::Footer::processPanelHideFinished()
{
}

void PricesListWidget::Footer::customizeClick()
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
