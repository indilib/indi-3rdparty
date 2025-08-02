#pragma once

#include <QSpinBox>

// A class for a spinbox that lets the user select a timing parameter,
// given a mapping of allowed codes to time values in nanoseconds.
class time_spin_box : public QSpinBox
{
  Q_OBJECT

  QMap<int, int> mapping;

  // Note: This member probably isn't necessary, it's usually just equal to
  // value().
  int code = -1;

  int decimals = 0;

public:
  time_spin_box(QWidget * parent = NULL);

  // Sets the mapping from encoded timing values which get returned by value(),
  // to the actual times they encode (in nanoseconds), which are displayed to
  // the user.
  void set_mapping(const QMap<int, int> &);

  // Sets the number of digits to show after the decimal point.
  void set_decimals(int decimals) { this->decimals = decimals; }

private slots:
  void set_code_from_value(int value);

private:
  QString text_from_value(int) const;
  int canonical_key_for_text(const QString & text) const;
  int step_up(int code) const;
  int step_down(int code) const;

protected:
  virtual void stepBy(int step_value);
  int valueFromText(const QString & text) const;
  QString textFromValue(int val) const;
  QValidator::State validate(QString & input, int & pos) const;
};
