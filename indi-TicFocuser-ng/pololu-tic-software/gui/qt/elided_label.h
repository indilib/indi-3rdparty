#pragma once

#include <QLabel>
#include <QWidget>

// QLabel subclass to make a label that elides if it is too long,
// instead of changing the size of the parent layout.
class elided_label : public QLabel
{
  Q_OBJECT

private:
  QString elided_text;

public:
  elided_label(QWidget* parent = NULL);

  void setText(const QString &);

protected:
  virtual void paintEvent(QPaintEvent *) override;
  virtual void resizeEvent(QResizeEvent *) override;

protected:
  void compute_elided_text();
};
