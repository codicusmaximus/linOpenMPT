/*
 * Controls.cpp
 * ------------
 * Purpose: Qt widgets for Windows common controls with no Qt counterpart.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "mfcless/Controls.h"

#include <QtGui/QPainter>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyleOptionTabWidgetFrame>
#include <QtWidgets/QStylePainter>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QToolButton>

#include <algorithm>

namespace mfcless
{

UpDown::UpDown(QWidget *parent)
	: QWidget(parent)
{
	auto *layout = new QBoxLayout(QBoxLayout::TopToBottom, this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	m_up = new QToolButton(this);
	m_down = new QToolButton(this);
	for(QToolButton *b : {m_up, m_down})
	{
		b->setAutoRepeat(true);
		b->setFocusPolicy(Qt::NoFocus);
		b->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	}
	layout->addWidget(m_up);
	layout->addWidget(m_down);
	SetVertical(true);
	connect(m_up, &QToolButton::clicked, this, [this] { Step(1); });
	connect(m_down, &QToolButton::clicked, this, [this] { Step(-1); });
}

void UpDown::SetVertical(bool vertical)
{
	m_vertical = vertical;
	// Vertical: up above down. Horizontal: left (down) before right (up).
	static_cast<QBoxLayout *>(layout())->setDirection(vertical ? QBoxLayout::TopToBottom : QBoxLayout::RightToLeft);
	m_up->setArrowType(vertical ? Qt::UpArrow : Qt::RightArrow);
	m_down->setArrowType(vertical ? Qt::DownArrow : Qt::LeftArrow);
	updateGeometry();
}

void UpDown::SetRange(int lower, int upper)
{
	m_lower = lower;
	m_upper = upper;
	SetPos(m_pos);
}

void UpDown::SetPos(int pos)
{
	m_pos = std::clamp(pos, std::min(m_lower, m_upper), std::max(m_lower, m_upper));
	if(m_buddy && m_buddyInt)
		m_buddy->setText(QString::number(m_pos));
}

int UpDown::GetPos() const
{
	if(m_buddy && m_buddyInt)
	{
		bool ok = false;
		int v = m_buddy->text().toInt(&ok);
		if(ok)
			return v;
	}
	return m_pos;
}

void UpDown::Step(int delta)
{
	bool accepted = true;
	const int cur = GetPos();
	emit deltaPos(cur, delta, &accepted);
	if(!accepted)
		return;
	// "Up" moves towards the upper end of the range, which may be smaller.
	const int dir = (m_upper >= m_lower) ? delta : -delta;
	int next = cur + dir;
	const int lo = std::min(m_lower, m_upper), hi = std::max(m_lower, m_upper);
	if(next < lo)
		next = m_wrap ? hi : lo;
	if(next > hi)
		next = m_wrap ? lo : hi;
	if(next == cur && !(m_buddy && m_buddyInt))
		return;
	SetPos(next);
	emit posChanged(m_pos);
}

QSize UpDown::sizeHint() const
{
	return m_vertical ? QSize(16, 22) : QSize(32, 16);
}


void GroupFrame::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	const QFontMetrics fm(font());
	QRect frame = rect();
	frame.setTop(fm.height() / 2);
	// Etched: a dark line with a light one inside it, offset by a pixel.
	const QColor dark = palette().color(QPalette::Mid), light = palette().color(QPalette::Light);
	p.setPen(light);
	p.drawRect(frame.adjusted(1, 1, -1, -1));
	p.setPen(dark);
	p.drawRect(frame.adjusted(0, 0, -2, -2));
	const QString caption = title();
	if(!caption.isEmpty())
	{
		const int width = fm.horizontalAdvance(QString(caption).remove(QLatin1Char('&'))) + 4;
		int left = 8;
		if(alignment() & Qt::AlignHCenter)
			left = (this->width() - width) / 2;
		else if(alignment() & Qt::AlignRight)
			left = this->width() - 8 - width;
		const QRect back(left, 0, std::min(width, this->width() - left), fm.height());
		p.fillRect(back, palette().color(backgroundRole()));
		p.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::WindowText));
		style()->drawItemText(&p, back.adjusted(2, 0, 0, 0), Qt::AlignLeft | Qt::AlignTop | Qt::TextShowMnemonic, palette(), isEnabled(), caption);
	}
}


TabControl::TabControl(QWidget *parent)
	: QWidget(parent)
{
	m_bar = new QTabBar(this);
	m_bar->setDrawBase(false);
	m_bar->setExpanding(false);
}

int TabControl::InsertItem(int index, const QString &text) { return m_bar->insertTab(index, text); }
int TabControl::InsertItem(int index, const char8_t *text) { return InsertItem(index, QString::fromUtf8(reinterpret_cast<const char *>(text))); }
int TabControl::currentIndex() const { return m_bar->currentIndex(); }
void TabControl::setCurrentIndex(int index) { m_bar->setCurrentIndex(index); }
int TabControl::count() const { return m_bar->count(); }

QRect TabControl::DisplayRect() const
{
	return QRect(0, m_bar->sizeHint().height(), width(), height() - m_bar->sizeHint().height()).adjusted(4, 4, -4, -4);
}

void TabControl::resizeEvent(QResizeEvent *)
{
	m_bar->setGeometry(0, 0, width(), m_bar->sizeHint().height());
}

void TabControl::paintEvent(QPaintEvent *)
{
	QStylePainter p(this);
	QStyleOptionTabWidgetFrame opt;
	opt.initFrom(this);
	opt.rect = QRect(0, m_bar->sizeHint().height() - 1, width(), height() - m_bar->sizeHint().height() + 1);
	opt.tabBarSize = m_bar->sizeHint();
	opt.shape = QTabBar::RoundedNorth;
	p.drawPrimitive(QStyle::PE_FrameTabWidget, opt);
}

} // namespace mfcless
