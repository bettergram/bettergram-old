#pragma once

#include <lang_auto.h>

namespace Ui {
class IconButton;
} // namespace Ui

namespace ChatHelpers {

class TableColumnHeaderWidget : public TWidget
{
	Q_OBJECT

public:
	enum class SortOrder {
		None,
		Ascending,
		Descending
	};

	TableColumnHeaderWidget(QWidget* parent);

	LangKey text() const;
	void setText(const LangKey &text);

	int textFlags() const;
	void setTextFlags(int textFlags);

	SortOrder sortOrder() const;
	void setSortOrder(const SortOrder &sortOrder, bool isEmitSignal = true);
	void resetSortOrder(bool isEmitSignal = true);

public slots:

signals:
	void sortOrderChanged();

protected:
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void leaveEventHook(QEvent *e) override;

	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *e) override;

private:
	LangKey _text = lng_language_name;
	int _textFlags = Qt::AlignRight | Qt::AlignVCenter;
	SortOrder _sortOrder = SortOrder::None;
	Ui::IconButton *_ascendingButton = nullptr;
	Ui::IconButton *_descendingButton = nullptr;

	bool _isPressed = false;

	void onAscendingButtonClicked();
	void onDescendingButtonClicked();

	void updateControlsGeometry();
};

} // namespace ChatHelpers
