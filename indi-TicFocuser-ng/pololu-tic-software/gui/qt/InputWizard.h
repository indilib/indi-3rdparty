#pragma once

#include <array>

#include <QWizard>

class InputWizard;
class QLabel;
class QProgressBar;

class main_window;
class main_controller;

enum wizard_page { INTRO, LEARN, CONCLUSION };
enum wizard_learn_step { NEUTRAL, MAX, MIN };


class input_range
{
public:
  input_range() {}

  void compute_from_samples(std::vector<uint16_t> const & samples);

  // Widens the range around the average, then shifts min and max in unison to
  // be equidistant from average (or as close to equidistant as possible if
  // limits don't allow).
  void widen_and_center_on_average(uint16_t range);

  uint16_t max = 0;
  uint16_t min = 0;
  uint16_t average = 0;

  uint16_t range() const { return max - min; }
  uint16_t distance_to(input_range const & other) const;
  bool intersects(input_range const & other) const { return distance_to(other) == 0; }
  bool is_entirely_above(input_range const & other) const { return min > other.max; }

  std::string to_string() const;
};


class LearnPage : public QWizardPage
{
  Q_OBJECT

public:
  LearnPage(InputWizard * parent);

  bool isComplete() const override;

protected:
  InputWizard * wizard() const { return (InputWizard *)(QWizardPage::wizard()); }
  main_window * window() const { return (main_window *)(QWizardPage::window()); }

private:
  void set_next_button_enabled(bool enabled);
  void set_progress_visible(bool visible);
  void set_text_from_step();

  bool handle_back();
  bool handle_next();

  void sample(uint16_t input);
  void learn_parameter();

  uint16_t full_range() const;

  void warn_if_close_to_neutral() const;

  int step = NEUTRAL;
  bool enable_next_button = true;

  bool sampling = false;
  std::vector<uint16_t> samples;
  std::array<input_range, 3> learned_ranges;

  bool input_invert;
  uint16_t input_min;
  uint16_t input_neutral_min;
  uint16_t input_neutral_max;
  uint16_t input_max;

  QLayout * setup_input_layout();

  QLabel * instruction_label;
  QLabel * input_label;
  QLabel * input_value;
  QLabel * input_pretty;
  QLabel * sampling_label;
  QProgressBar * sampling_progress;

  friend class InputWizard;
};


class InputWizard : public QWizard
{
  Q_OBJECT

public:
  InputWizard(main_window * parent);

  void set_control_mode(uint8_t control_mode);
  uint8_t control_mode() const { return cmode; }
  QString control_mode_name() const;
  QString input_pin_name() const;

  void handle_input(uint16_t input);

  bool learned_input_invert() const           { return learn_page->input_invert; }
  uint16_t learned_input_min() const          { return learn_page->input_min; }
  uint16_t learned_input_neutral_min() const  { return learn_page->input_neutral_min; }
  uint16_t learned_input_neutral_max() const  { return learn_page->input_neutral_max; }
  uint16_t learned_input_max() const          { return learn_page->input_max; }

  void force_back();
  void force_next();

protected:
  void showEvent(QShowEvent * event) override;

private slots:
  void on_currentIdChanged(int id);

private:
  bool suppress_events = false;


  QWizardPage * setup_intro_page();
  QWizardPage * setup_learn_page();
  QWizardPage * setup_conclusion_page();

  QLabel * intro_label;
  LearnPage * learn_page;

  uint8_t cmode;
  void set_text_from_control_mode();
};
