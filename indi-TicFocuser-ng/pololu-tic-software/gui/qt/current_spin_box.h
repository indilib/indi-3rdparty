#pragma once

#include <QSpinBox>

class current_spin_box : public QSpinBox
{
  Q_OBJECT

public:
  current_spin_box(QWidget * parent = Q_NULLPTR);

  // Set the mapping of allowed codes to current values.
  void set_mapping(const QMap<int, int> &);

  int get_code() { return code; }

private slots:
  void editing_finished();
  void set_code_from_value();

private:
  void fix_code_if_not_allowed();
  void set_value_from_code();

  QMap<int, int> mapping;
  int code = -1;

protected:
  virtual void stepBy(int step_value);
};

