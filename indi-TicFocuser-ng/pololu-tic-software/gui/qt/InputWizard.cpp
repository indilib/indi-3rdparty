#include "InputWizard.h"
#include "main_window.h"
#include "tic.hpp"
#include "to_string.h"

#include <QChar>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

#ifdef __APPLE__
#define NEXT_BUTTON_TEXT tr("Continue")
#define FINISH_BUTTON_TEXT tr("Done")
#else
#define NEXT_BUTTON_TEXT tr("Next")
#define FINISH_BUTTON_TEXT tr("Finish")
#endif

// Take 20 samples, one sample every 50 ms. Total time is 1 s.
static uint32_t const SAMPLE_COUNT = 20;

InputWizard::InputWizard(main_window * parent)
  : QWizard(parent)
{
  setPage(INTRO, setup_intro_page());
  setPage(LEARN, setup_learn_page());
  setPage(CONCLUSION, setup_conclusion_page());

  setWindowTitle(tr("Input Setup Wizard"));
  setWindowIcon(QIcon(":app_icon"));
  setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);
  // todo: maybe set Qt::MSWindowsFixedSizeDialogHint? it gets rid of unwanted resize handles at least on left, right, and bottom edges

  setFixedSize(sizeHint());

  connect(this, SIGNAL(currentIdChanged(int)), this, SLOT(on_currentIdChanged(int)));
}

void InputWizard::set_control_mode(uint8_t control_mode)
{
  cmode = control_mode;
  set_text_from_control_mode();
}

QString InputWizard::control_mode_name() const
{
  switch (cmode)
  {
  case TIC_CONTROL_MODE_RC_POSITION:
  case TIC_CONTROL_MODE_RC_SPEED:
    return tr("RC");

  case TIC_CONTROL_MODE_ANALOG_POSITION:
  case TIC_CONTROL_MODE_ANALOG_SPEED:
    return tr("analog");

  default:
    return tr("(Invalid)");
  }
}

QString InputWizard::input_pin_name() const
{
  switch (cmode)
  {
  case TIC_CONTROL_MODE_RC_POSITION:
  case TIC_CONTROL_MODE_RC_SPEED:
    return "RC";

  case TIC_CONTROL_MODE_ANALOG_POSITION:
  case TIC_CONTROL_MODE_ANALOG_SPEED:
    return u8"SDA\u200A/\u200AAN";

  default:
    return tr("(Invalid)");
  }
}

void InputWizard::handle_input(uint16_t input)
{
  bool input_not_null = (input != TIC_INPUT_NULL);

  if (input_not_null)
  {
    learn_page->input_value->setText(QString::number(input));

    switch (cmode)
    {
    case TIC_CONTROL_MODE_RC_POSITION:
    case TIC_CONTROL_MODE_RC_SPEED:
      learn_page->input_pretty->setText("(" + QString::fromStdString(
        convert_input_to_us_string(input)) + ")");
      break;

    case TIC_CONTROL_MODE_ANALOG_POSITION:
    case TIC_CONTROL_MODE_ANALOG_SPEED:
      learn_page->input_pretty->setText("(" + QString::fromStdString(
        convert_input_to_v_string(input)) + ")");
      break;

    default:
      learn_page->input_pretty->setText("");
    }
  }
  else
  {
    learn_page->input_value->setText(tr("N/A"));
    learn_page->input_pretty->setText("");
  }

  if (learn_page->sampling)
  {
    learn_page->sample(input);
  }
}

void InputWizard::force_back()
{
  suppress_events = true;
  back();
  suppress_events = false;
}

void InputWizard::force_next()
{
  suppress_events = true;
  next();
  suppress_events = false;
}

void InputWizard::showEvent(QShowEvent * event)
{
  (void)event;
  learn_page->step = NEUTRAL;
  learn_page->set_text_from_step();
  restart();
}

void InputWizard::on_currentIdChanged(int id)
{
  if (suppress_events) { return; }

  if (id == INTRO)
  {
    // User clicked Back from the learn page.
    if (!learn_page->handle_back())
    {
      force_next();
    }
  }
  else if (id == CONCLUSION)
  {
    // User clicked Next from the learn page.
    if (!learn_page->handle_next())
    {
      force_back();
    }
  }
}

static QString capitalize(QString const & str)
{
  QString new_str = str;
  new_str[0] = new_str[0].toTitleCase();
  return new_str;
}

void InputWizard::set_text_from_control_mode()
{
  intro_label->setText(
    tr("This wizard will help you quickly set the scaling parameters for the Tic's ") +
    control_mode_name() + tr(" input."));

  learn_page->input_label->setText(capitalize(control_mode_name()) + tr(" input:"));

  learn_page->set_text_from_step();
}

QWizardPage * InputWizard::setup_intro_page()
{
  QWizardPage * page = new QWizardPage();
  QVBoxLayout * layout = new QVBoxLayout();

  page->setTitle(tr("Input setup wizard"));

  intro_label = new QLabel();
  intro_label->setWordWrap(true);
  layout->addWidget(intro_label);
  layout->addStretch(1);

  QLabel * deenergized_label = new QLabel(
    tr("NOTE: Your motor has been automatically de-energized so that it does not "
    "cause problems while you are using this wizard.  To energize it manually "
    "later, you can click the \"Resume\" button (after fixing any errors)."));
  deenergized_label->setWordWrap(true);
  layout->addWidget(deenergized_label);

  page->setLayout(layout);
  return page;
}

QWizardPage * InputWizard::setup_learn_page()
{
  learn_page = new LearnPage(this);
  return learn_page;
}

QWizardPage * InputWizard::setup_conclusion_page()
{
  QWizardPage * page = new QWizardPage();
  QVBoxLayout * layout = new QVBoxLayout();

  page->setTitle(tr("Input setup finished"));

  QLabel * completed_label = new QLabel(
    tr("You have successfully completed this wizard.  You can see your new "
    "settings on the \"Input and motor settings\" tab after you click ") +
    FINISH_BUTTON_TEXT + tr(".  "
    "To use the new settings, you must first apply them to the device."));
  completed_label->setWordWrap(true);
  layout->addWidget(completed_label);

  layout->addStretch(1);

  page->setLayout(layout);
  return page;

  // todo: checkbox to automatically apply and resume? 2 separate checkboxes?
}


LearnPage::LearnPage(InputWizard * parent)
  : QWizardPage(parent)
{
  QVBoxLayout * layout = new QVBoxLayout();

  instruction_label = new QLabel();
  instruction_label->setWordWrap(true);
  instruction_label->setAlignment(Qt::AlignTop);
  instruction_label->setMinimumHeight(fontMetrics().lineSpacing() * 2);
  layout->addWidget(instruction_label);
  layout->addSpacing(fontMetrics().height());

  layout->addLayout(setup_input_layout());
  layout->addSpacing(fontMetrics().height());

  QLabel * next_label = new QLabel(
    tr("When you click ") + NEXT_BUTTON_TEXT +
    tr(", this wizard will sample the input values for one second.  "
    "Please do not change the input while it is being sampled."));
  next_label->setWordWrap(true);
  layout->addWidget(next_label);
  layout->addSpacing(fontMetrics().height());

  sampling_label = new QLabel(tr("Sampling..."));
  layout->addWidget(sampling_label);

  sampling_progress = new QProgressBar();
  sampling_progress->setMaximum(SAMPLE_COUNT);
  sampling_progress->setTextVisible(false);
  layout->addWidget(sampling_progress);

  set_progress_visible(false);

  layout->addStretch(1);

  setLayout(layout);
}

QLayout * LearnPage::setup_input_layout()
{
  QHBoxLayout * layout = new QHBoxLayout();

  input_label = new QLabel();
  layout->addWidget(input_label);

  input_value = new QLabel();
  layout->addWidget(input_value);

  input_pretty = new QLabel();
  layout->addWidget(input_pretty);

  layout->addStretch(1);

  // Set fixed sizes for performance.
  {
    input_value->setText(QString::number(4500 * 12));
    input_value->setFixedSize(input_value->sizeHint());

    input_pretty->setText("(" + QString::fromStdString(
      convert_input_to_us_string(4500 * 12)) + ")");
    input_pretty->setFixedSize(input_pretty->sizeHint());
  }

  return layout;
}

bool LearnPage::isComplete() const
{
  return enable_next_button;
}

void LearnPage::set_next_button_enabled(bool enabled)
{
  enable_next_button = enabled;
  emit completeChanged();
}

void LearnPage::set_progress_visible(bool visible)
{
  sampling_label->setVisible(visible);
  sampling_progress->setVisible(visible);
}

void LearnPage::set_text_from_step()
{
  switch (step)
  {
  case NEUTRAL:
    setTitle(tr("Step 1 of 3: Neutral"));
    instruction_label->setText(
      tr("Verify that you have connected your input to the ") +
      wizard()->input_pin_name() +
      tr(" pin.  Next, move the input to its neutral position."));
    break;

  case MAX:
    setTitle(tr("Step 2 of 3: Maximum"));
    instruction_label->setText(
      tr("Move the input to its maximum (full forward) position."));
    break;

  case MIN:
    setTitle(tr("Step 3 of 3: Minimum"));
    instruction_label->setText(
      tr("Move the input to its minimum (full reverse) position."));
    break;
  }
}

bool LearnPage::handle_back()
{
  if (sampling)
  {
    // We were in the middle of sampling, so just cancel that.
    sampling = false;
    set_progress_visible(false);
    set_next_button_enabled(true);
    return false;
  }
  else
  {
    // We were not sampling, so go back to the previous step/page.
    if (step == NEUTRAL)
    {
      return true;
    }
    else
    {
      step--;
      set_text_from_step();
      return false;
    }
  }
}

bool LearnPage::handle_next()
{
  if (!sampling)
  {
    sampling = true;
    samples.clear();
    sampling_progress->setValue(0);
    set_progress_visible(true);
    set_next_button_enabled(false);
  }
  // The next button should not actually advance the page immediately; it will
  // advance once sampling is finished.
  return false;
}

void LearnPage::sample(uint16_t input)
{
  if (input == TIC_INPUT_NULL)
  {
    sampling = false;
    set_progress_visible(false);
    set_next_button_enabled(true);
    window()->show_error_message(
      "Sampling was aborted because the input was invalid.  Please try again.");
    return;
  }

  samples.push_back(input);
  sampling_progress->setValue(sampling_progress->value() + 1);

  if (sampling_progress->value() == SAMPLE_COUNT)
  {
    sampling = false;
    set_progress_visible(false);
    set_next_button_enabled(true);

    learn_parameter();
  }
}

void LearnPage::learn_parameter()
{
   std::string const try_again = "\n"
   "\n"
   "Please verify that your input is connected properly to the " +
   wizard()->input_pin_name().toStdString() +
   " pin by moving it while looking at the input value and try again.";

  learned_ranges[step].compute_from_samples(samples);

  // Check range. Complain to the user if the range is bigger than about 7.5% of
  // the standard full range.
  if (learned_ranges[step].range() > (full_range() * 3 + 20) / 40)
  {
    window()->show_error_message("The input value varied too widely (" +
      learned_ranges[step].to_string() + ") during the sampling time.\n"
      "Please hold the input still and try again so an accurate reading can be "
      "obtained.");
    return;
  }

  switch(step)
  {
  case NEUTRAL:
    // Widen the deadband to 5% of the standard full range or 3 times the
    // sampled range, whichever is greater.
    learned_ranges[NEUTRAL].widen_and_center_on_average(
      std::max((full_range() + 10) / 20, 3 * learned_ranges[NEUTRAL].range()));
    step++;
    set_text_from_step();
    return;

  case MAX:
    warn_if_close_to_neutral();
    step++;
    set_text_from_step();
    return;

  case MIN:
    // Check that max does not intersect min.
    if (learned_ranges[MIN].intersects(learned_ranges[MAX]))
    {
      window()->show_error_message(
        "The values sampled for the minimum input (" +
        learned_ranges[MIN].to_string() + ") intersect the values sampled for "
        "the maximum input (" + learned_ranges[MAX].to_string() + ")." +
        try_again);
      return;
    }

    // Check that at least one of max or min does not intersect with neutral.
    if (learned_ranges[MIN].intersects(learned_ranges[NEUTRAL]) &&
      learned_ranges[MAX].intersects(learned_ranges[NEUTRAL]))
    {
      window()->show_error_message(
        "The values sampled for the minimum input (" +
        learned_ranges[MIN].to_string() + ") and the values sampled for the "
        "maximum input (" + learned_ranges[MAX].to_string() +
        ") both intersect the calculated neutral deadband (" +
        learned_ranges[NEUTRAL].to_string() + ")." + try_again);
      return;
    }

    // Invert the channel if necessary.
    // This ensures that real_max is entirely above real_min.
    input_range * real_max_range = &learned_ranges[MAX];
    input_range * real_min_range = &learned_ranges[MIN];
    if (learned_ranges[MIN].is_entirely_above(learned_ranges[MAX]))
    {
      input_invert = true;
      real_max_range = &learned_ranges[MIN];
      real_min_range = &learned_ranges[MAX];
    }
    else
    {
      input_invert = false;
    }

    // Check that real_max_range and real_min_range are not both on the same
    // side of the deadband.
    {
      if (real_min_range->is_entirely_above(learned_ranges[NEUTRAL]))
      {
        window()->show_error_message(
          "The maximum and minimum values measured for the input (" +
          learned_ranges[MAX].to_string() + " and " +
          learned_ranges[MIN].to_string() +
          ") were both above the neutral deadband (" +
          learned_ranges[NEUTRAL].to_string() + ")." + try_again);
        return;
      }
      if (learned_ranges[NEUTRAL].is_entirely_above(*real_max_range))
      {
        window()->show_error_message(
          "The maximum and minimum values measured for the input (" +
          learned_ranges[MAX].to_string() + " and " +
          learned_ranges[MIN].to_string() +
          ") were both below the neutral deadband (" +
          learned_ranges[NEUTRAL].to_string() + ")." + try_again);
        return;
      }
    }

    warn_if_close_to_neutral();

    // All the checks are done, so figure out the new settings.

    input_neutral_min = learned_ranges[NEUTRAL].min;
    input_neutral_max = learned_ranges[NEUTRAL].max;

    // Set input_max.
    if (real_max_range->intersects(learned_ranges[NEUTRAL]))
    {
      // real_max_range intersects the deadband.
      // Set input_max equal to input_neutral_max so that the scaled value will
      // never go in this direction.
      // If input_invert is false, this means that the scaled value for this
      // channel will never be positive.
      // If input_invert is true, this means that the scaled value for this
      // channel will never be negative.
      input_max = input_neutral_max;
    }
    else
    {
      // real_max_range does NOT intersect the deadband.
      // Try to set input_max to a value that is a little bit below
      // real_max_range->min (by ~3% of the distance to input_neutral_max or 1%
      // of the total range, whichever is lesser) so that when the user pushes
      // his stick to the max, he is guaranteed to get full speed.
      int margin = std::min(
        (real_max_range->min - input_neutral_max + 15) / 30,
        (full_range() + 50) / 100);
      input_max = real_max_range->min - margin;
    }

    // Set input_min.
    if (real_min_range->intersects(learned_ranges[NEUTRAL]))
    {
      // real_min_range intersects the deadband.
      // Set input_min equal to input_neutral_min so that the scaled value will
      // never go in this direction.
      // If input_invert is false, this means that the scaled value for this
      // channel will never be negative.
      // If input_invert is true, this means that the scaled value for this
      // channel will never be positive.
      input_min = input_neutral_min;
    }
    else
    {
      // real_min_range does NOT intersect the deadband.
      // Try to set input_min to a value that is a little bit above
      // real_min_range->max (by ~3% of the distance to input_neutral_min or 1%
      // of the total range, whichever is lesser) so that when the user pushes
      // his stick to the min, he is guaranteed to get full speed.
      int margin = std::min(
        (input_neutral_min - real_min_range->max + 15) / 30,
        (full_range() + 50) / 100);
      input_min = real_min_range->max + margin;
    }

    wizard()->force_next();
  }
}

uint16_t LearnPage::full_range() const
{
  switch (wizard()->control_mode())
  {
  case TIC_CONTROL_MODE_RC_POSITION:
  case TIC_CONTROL_MODE_RC_SPEED:
    // Standard RC full range is 1500 to 3000 (units of 2/3 us).
    return 1500;

  case TIC_CONTROL_MODE_ANALOG_POSITION:
  case TIC_CONTROL_MODE_ANALOG_SPEED:
    // Standard analog full range is 0 to 4095.
    return 4095;

  default:
    return 0;
  }
}

void LearnPage::warn_if_close_to_neutral() const
{
  if (learned_ranges[step].intersects(learned_ranges[NEUTRAL]))
  {
    // The input was indistinguishable from the neutral inputs, so warn the user!

    std::string direction = (step == MAX) ? "maximum" : "minimum";

    window()->show_warning_message(
      "The values sampled for the " + direction + " input (" +
      learned_ranges[step].to_string() +
      ") intersect with the calculated neutral deadband (" +
      learned_ranges[NEUTRAL].to_string() + ").\n"
      "\n"
      "If you continue, you will only be able to use the " +
      wizard()->control_mode_name().toStdString() + " input in one direction, "
      "and you should set the target " + direction +
      " setting to 0 to make the target unidirectional.\n"
      "\n"
      "You can go back and try again if this is not the desired behavior.");
  }
}

void input_range::compute_from_samples(std::vector<uint16_t> const & samples)
{
  uint32_t sum = 0;
  min = UINT16_MAX;
  max = 0;

  for (uint16_t s : samples)
  {
    sum += s;
    if (s < min) { min = s; }
    if (s > max) { max = s; }
  }
  average = (sum + samples.size() / 2) / samples.size();
}

void input_range::widen_and_center_on_average(uint16_t desired_range)
{
  // Use an upper limit of 4095 for both analog and RC. For RC, this corresponds
  // to a pulse width of 2730 us, which should be more than enough.
  static uint16_t const upper_limit = 4095;

  if (average > upper_limit) { average = upper_limit; }
  if (desired_range > upper_limit) { desired_range = upper_limit; }

  uint16_t half_range = desired_range / 2;

  // Adjust the center so that it is within the inclusive range
  //   [halfRange, upperLimit-halfRange].
  if (average < half_range) { average = half_range; }
  if (average > (upper_limit - half_range)) { average = upper_limit - half_range; }

  min = average - half_range;
  max = average + half_range;

  if ((desired_range % 2) == 1)
  {
    // Widen the range by one to correct the rounding error we introduced
    // when we computed half_range above.
    if (min > 0) { min--; }
    else if (max < upper_limit) { max++; }
  }
}

uint16_t input_range::distance_to(input_range const & other) const
{
  if (other.min > max)
  {
    return other.min - max;
  }
  else if (min > other.max)
  {
    return min - other.max;
  }
  else
  {
    return 0;
  }
}

std::string input_range::to_string() const
{
  return std::to_string(min) + u8"\u2013" + std::to_string(max);
}
