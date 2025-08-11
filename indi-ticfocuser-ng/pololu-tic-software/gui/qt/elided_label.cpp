#include "elided_label.h"

#include <QPainter>
#include <QResizeEvent>
#include <QStyle>

elided_label::elided_label(QWidget* parent)
  : QLabel(parent)
{
  setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
}

void elided_label::setText(const QString & text)
{
  setToolTip(text);
  QLabel::setText(text);
  compute_elided_text();
}

void elided_label::compute_elided_text()
{
  elided_text = fontMetrics().elidedText(text(), Qt::ElideRight, width());
}

void elided_label::resizeEvent(QResizeEvent * e)
{
  QLabel::resizeEvent(e);
  compute_elided_text();
}

void elided_label::paintEvent(QPaintEvent * e)
{
  (void)e;
  QPainter p(this);
  p.drawText(0, 0, width(), height(),
    QStyle::visualAlignment(Qt::LeftToRight, Qt::AlignVCenter),
      elided_text);
}
