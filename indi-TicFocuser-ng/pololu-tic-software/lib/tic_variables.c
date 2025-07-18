// Functions for representing variables read from the device in memory.

#include "tic_internal.h"

#define PIN_COUNT 5

struct tic_variables
{
  uint8_t product;
  uint8_t operation_state;
  bool energized;
  bool position_uncertain;
  bool forward_limit_active;
  bool reverse_limit_active;
  bool homing_active;
  uint16_t error_status;
  uint32_t errors_occurred;
  uint8_t planning_mode;
  int32_t target_position;
  int32_t target_velocity;
  uint32_t starting_speed;
  uint32_t max_speed;
  uint32_t max_decel;
  uint32_t max_accel;
  int32_t current_position;
  int32_t current_velocity;
  int32_t acting_target_position;
  uint32_t time_since_last_step;
  uint8_t device_reset;
  uint16_t vin_voltage;
  uint32_t up_time;
  int32_t encoder_position;
  uint16_t rc_pulse_width;
  uint8_t step_mode;
  uint8_t current_limit_code;
  uint8_t decay_mode;
  uint8_t input_state;
  uint16_t input_after_averaging;
  uint16_t input_after_hysteresis;
  int32_t input_after_scaling;
  uint8_t last_motor_driver_error;
  uint8_t agc_mode;
  uint8_t agc_bottom_current_limit;
  uint8_t agc_current_boost_steps;
  uint8_t agc_frequency_limit;

  struct {
    uint16_t analog_reading;
    bool digital_reading;
    uint8_t pin_state;
  } pin_info[TIC_CONTROL_PIN_COUNT];

  uint8_t last_hp_driver_errors;
};

tic_error * tic_variables_create(tic_variables ** variables)
{
  if (variables == NULL)
  {
    return tic_error_create("Variables output pointer is null.");
  }

  *variables = NULL;

  tic_error * error = NULL;

  tic_variables * new_variables = NULL;
  if (error == NULL)
  {
    new_variables = (tic_variables *)calloc(1, sizeof(tic_variables));
    if (new_variables == NULL) { error = &tic_error_no_memory; }
  }

  if (error == NULL)
  {
    *variables = new_variables;
    new_variables = NULL;
  }

  tic_variables_free(new_variables);

  return error;
}

tic_error * tic_variables_copy(const tic_variables * source, tic_variables ** dest)
{
  if (dest == NULL)
  {
    return tic_error_create("Variables output pointer is null.");
  }

  *dest = NULL;

  if (source == NULL)
  {
    return NULL;
  }

  tic_error * error = NULL;

  tic_variables * new_variables = NULL;
  if (error == NULL)
  {
    new_variables = (tic_variables *)calloc(1, sizeof(tic_variables));
    if (new_variables == NULL) { error = &tic_error_no_memory; }
  }

  if (error == NULL)
  {
    memcpy(new_variables, source, sizeof(tic_variables));
  }

  if (error == NULL)
  {
    *dest = new_variables;
    new_variables = NULL;
  }

  tic_variables_free(new_variables);

  return error;
}

void tic_variables_free(tic_variables * variables)
{
  free(variables);
}

static void write_buffer_to_variables(const uint8_t * buf,
  tic_variables * vars, uint8_t product)
{
  assert(vars != NULL);
  assert(buf != NULL);

  vars->operation_state = buf[TIC_VAR_OPERATION_STATE];
  uint8_t misc_flags1 = buf[TIC_VAR_MISC_FLAGS1];
  vars->energized = misc_flags1 >> TIC_MISC_FLAGS1_ENERGIZED & 1;
  vars->position_uncertain = misc_flags1 >> TIC_MISC_FLAGS1_POSITION_UNCERTAIN & 1;
  vars->forward_limit_active = misc_flags1 >> TIC_MISC_FLAGS1_FORWARD_LIMIT_ACTIVE & 1;
  vars->reverse_limit_active = misc_flags1 >> TIC_MISC_FLAGS1_REVERSE_LIMIT_ACTIVE & 1;
  vars->homing_active = misc_flags1 >> TIC_MISC_FLAGS1_HOMING_ACTIVE & 1;
  vars->error_status = read_u16(buf + TIC_VAR_ERROR_STATUS);
  vars->errors_occurred = read_u32(buf + TIC_VAR_ERRORS_OCCURRED);
  vars->planning_mode = buf[TIC_VAR_PLANNING_MODE];
  vars->target_position = read_i32(buf + TIC_VAR_TARGET_POSITION);
  vars->target_velocity = read_i32(buf + TIC_VAR_TARGET_VELOCITY);
  vars->starting_speed = read_u32(buf + TIC_VAR_STARTING_SPEED);
  vars->max_speed = read_u32(buf + TIC_VAR_MAX_SPEED);
  vars->max_decel = read_u32(buf + TIC_VAR_MAX_DECEL);
  vars->max_accel = read_u32(buf + TIC_VAR_MAX_ACCEL);
  vars->current_position = read_i32(buf + TIC_VAR_CURRENT_POSITION);
  vars->current_velocity = read_i32(buf + TIC_VAR_CURRENT_VELOCITY);
  vars->acting_target_position = read_i32(buf + TIC_VAR_ACTING_TARGET_POSITION);
  vars->time_since_last_step = read_i32(buf + TIC_VAR_TIME_SINCE_LAST_STEP);
  vars->device_reset = buf[TIC_VAR_DEVICE_RESET];
  vars->vin_voltage = read_u16(buf + TIC_VAR_VIN_VOLTAGE);
  vars->up_time = read_u32(buf + TIC_VAR_UP_TIME);
  vars->encoder_position = read_i32(buf + TIC_VAR_ENCODER_POSITION);
  vars->rc_pulse_width = read_u16(buf + TIC_VAR_RC_PULSE_WIDTH);
  vars->step_mode = buf[TIC_VAR_STEP_MODE];
  vars->current_limit_code = buf[TIC_VAR_CURRENT_LIMIT];

  if (product == TIC_PRODUCT_T825 ||
    product == TIC_PRODUCT_N825 ||
    product == TIC_PRODUCT_T834)
  {
    vars->decay_mode = buf[TIC_VAR_DECAY_MODE];
  }
  else
  {
    // Ignore the Decay mode variable here since it does not really apply,
    // and ignoring it here makes it safer to reuse its byte for a different
    // variable in the future.
  }

  vars->input_state = buf[TIC_VAR_INPUT_STATE];
  vars->input_after_averaging = read_u16(buf + TIC_VAR_INPUT_AFTER_AVERAGING);
  vars->input_after_hysteresis = read_u16(buf + TIC_VAR_INPUT_AFTER_HYSTERESIS);
  vars->input_after_scaling = read_i32(buf + TIC_VAR_INPUT_AFTER_SCALING);

  {
    uint8_t d = buf[TIC_VAR_DIGITAL_READINGS];
    vars->pin_info[TIC_PIN_NUM_SCL].digital_reading = d >> TIC_PIN_NUM_SCL & 1;
    vars->pin_info[TIC_PIN_NUM_SDA].digital_reading = d >> TIC_PIN_NUM_SDA & 1;
    vars->pin_info[TIC_PIN_NUM_TX].digital_reading = d >> TIC_PIN_NUM_TX & 1;
    vars->pin_info[TIC_PIN_NUM_RX].digital_reading = d >> TIC_PIN_NUM_RX & 1;
    vars->pin_info[TIC_PIN_NUM_RC].digital_reading = d >> TIC_PIN_NUM_RC & 1;
  }

  {
    uint8_t s = buf[TIC_VAR_PIN_STATES];
    vars->pin_info[TIC_PIN_NUM_SCL].pin_state = s >> (TIC_PIN_NUM_SCL * 2) & 3;
    vars->pin_info[TIC_PIN_NUM_SDA].pin_state = s >> (TIC_PIN_NUM_SDA * 2) & 3;
    vars->pin_info[TIC_PIN_NUM_TX].pin_state = s >> (TIC_PIN_NUM_TX * 2) & 3;
    vars->pin_info[TIC_PIN_NUM_RX].pin_state = s >> (TIC_PIN_NUM_RX * 2) & 3;
  }

  vars->pin_info[TIC_PIN_NUM_SCL].analog_reading =
    read_u16(buf + TIC_VAR_ANALOG_READING_SCL);
  vars->pin_info[TIC_PIN_NUM_SDA].analog_reading =
    read_u16(buf + TIC_VAR_ANALOG_READING_SDA);
  vars->pin_info[TIC_PIN_NUM_TX].analog_reading =
    read_u16(buf + TIC_VAR_ANALOG_READING_TX);
  vars->pin_info[TIC_PIN_NUM_RX].analog_reading =
    read_u16(buf + TIC_VAR_ANALOG_READING_RX);

  // Because of hardware limitations, the RC pin is always an input and it
  // cannot do analog readings.
  vars->pin_info[TIC_PIN_NUM_RC].pin_state = TIC_PIN_STATE_HIGH_IMPEDANCE;
  vars->pin_info[TIC_PIN_NUM_RC].analog_reading = 0;

  if (vars->product == TIC_PRODUCT_T249)
  {
    vars->last_motor_driver_error = buf[TIC_VAR_LAST_MOTOR_DRIVER_ERROR];
    vars->agc_mode = buf[TIC_VAR_AGC_MODE];
    vars->agc_bottom_current_limit = buf[TIC_VAR_AGC_BOTTOM_CURRENT_LIMIT];
    vars->agc_current_boost_steps = buf[TIC_VAR_AGC_CURRENT_BOOST_STEPS];
    vars->agc_frequency_limit = buf[TIC_VAR_AGC_FREQUENCY_LIMIT];
  }

  if (vars->product == TIC_PRODUCT_36V4)
  {
    vars->last_hp_driver_errors = buf[TIC_VAR_LAST_HP_DRIVER_ERRORS];
  }
}

tic_error * tic_get_variables(tic_handle * handle, tic_variables ** variables,
  bool clear_errors_occurred)
{
  if (variables == NULL)
  {
    return tic_error_create("Variables output pointer is null.");
  }

  *variables = NULL;

  if (handle == NULL)
  {
    return tic_error_create("Handle is null.");
  }

  tic_error * error = NULL;

  // Create a variables object.
  tic_variables * new_variables = NULL;
  if (error == NULL)
  {
    error = tic_variables_create(&new_variables);
  }

  uint8_t product = tic_device_get_product(tic_handle_get_device(handle));

  // Define the different variable segments we will read.
  uint8_t general_size = TIC_VAR_INPUT_AFTER_SCALING + 4;
  uint8_t product_specific_offset = 0;

  if (product == TIC_PRODUCT_T249)
  {
    // The Tic T249 actually has some product-specific variables that were
    // placed in the general variables area.
    general_size = TIC_VAR_AGC_FREQUENCY_LIMIT + 1;
  }

  if (product == TIC_PRODUCT_36V4)
  {
    product_specific_offset = TIC_VAR_LAST_HP_DRIVER_ERRORS;
  }

  // Read all the variables from the device.
  uint8_t buf[256] = { 0 };
  if (error == NULL)
  {
    uint8_t offset = 0;
    error = tic_get_variable_segment(handle, offset, general_size,
      buf + offset, clear_errors_occurred);
  }

  if (error == NULL && product_specific_offset)
  {
    uint8_t size = 256 - product_specific_offset;
    error = tic_get_variable_segment(handle, product_specific_offset,
      size, buf + product_specific_offset, false);
  }

  // Store the variables in the new variables object.
  if (error == NULL)
  {
    new_variables->product = tic_device_get_product(tic_handle_get_device(handle));
    write_buffer_to_variables(buf, new_variables, product);
  }

  // Pass the new variables to the caller.
  if (error == NULL)
  {
    *variables = new_variables;
    new_variables = NULL;
  }

  tic_variables_free(new_variables);

  if (error != NULL)
  {
    error = tic_error_add(error,
      "There was an error reading variables from the device.");
  }

  return error;
}

uint8_t tic_variables_get_operation_state(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->operation_state;
}

bool tic_variables_get_energized(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->energized;
}

bool tic_variables_get_position_uncertain(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->position_uncertain;
}

bool tic_variables_get_forward_limit_active(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->forward_limit_active;
}

bool tic_variables_get_reverse_limit_active(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->reverse_limit_active;
}

bool tic_variables_get_homing_active(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->homing_active;
}

uint16_t tic_variables_get_error_status(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->error_status;
}

uint32_t tic_variables_get_errors_occurred(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->errors_occurred;
}

uint8_t tic_variables_get_planning_mode(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->planning_mode;
}

int32_t tic_variables_get_target_position(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->target_position;
}

int32_t tic_variables_get_target_velocity(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->target_velocity;
}

uint32_t tic_variables_get_starting_speed(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->starting_speed;
}

uint32_t tic_variables_get_max_speed(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->max_speed;
}

uint32_t tic_variables_get_max_decel(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->max_decel;
}

uint32_t tic_variables_get_max_accel(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->max_accel;
}

int32_t tic_variables_get_current_position(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->current_position;
}

int32_t tic_variables_get_current_velocity(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->current_velocity;
}

int32_t tic_variables_get_acting_target_position(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->acting_target_position;
}

uint32_t tic_variables_get_time_since_last_step(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->time_since_last_step;
}

uint8_t tic_variables_get_device_reset(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->device_reset;
}

uint32_t tic_variables_get_vin_voltage(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->vin_voltage;
}

uint32_t tic_variables_get_up_time(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->up_time;
}

int32_t tic_variables_get_encoder_position(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->encoder_position;
}

uint16_t tic_variables_get_rc_pulse_width(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->rc_pulse_width;
}

uint8_t tic_variables_get_step_mode(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->step_mode;
}

uint32_t tic_variables_get_current_limit(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return tic_current_limit_code_to_ma(variables->product, variables->current_limit_code);
}

uint8_t tic_variables_get_current_limit_code(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->current_limit_code;
}

uint8_t tic_variables_get_decay_mode(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->decay_mode;
}

uint8_t tic_variables_get_input_state(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->input_state;
}

uint16_t tic_variables_get_input_after_averaging(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->input_after_averaging;
}

uint16_t tic_variables_get_input_after_hysteresis(const tic_variables * variables)
{
  if (variables == NULL) { return 0xFFFF; }
  return variables->input_after_hysteresis;
}

static uint8_t tic_input_shift_before_scaling(
  uint8_t control_mode, bool input_averaging_enabled)
{
  switch (control_mode)
  {
  case TIC_CONTROL_MODE_RC_POSITION:
  case TIC_CONTROL_MODE_RC_SPEED:
    // In RC modes, inputAfterHysteresis always has units of 1/12 us, (typical
    // values from 12000 to 24000) and the firmware divides it by 8 before
    // scaling so that it can be between 0 and 4095.
    return 3;

  case TIC_CONTROL_MODE_ANALOG_POSITION:
  case TIC_CONTROL_MODE_ANALOG_SPEED:
    if (input_averaging_enabled)
    {
      // In an analog mode with input averaging enabled, we take 8 10-bit ADC
      // readings for each sample, and then we add 4 samples together, so
      // inputAfterHysteresis is a 15-bit number.  The firmware shifts it by 3
      // to make it a 12-bit number between 0 and 4095.
      return 3;
    }
    else
    {
      // In an analog mode with input averaging disabled, we take 8 10-bit ADC
      // readings for each sample, so inputAfterHysteresis is a 13-bit number.
      // The firmware shifts it by 1 to make it a 12-bit number.
      return 1;
    }

  default:
    return 0;
  }
}

uint16_t tic_variables_get_input_before_scaling(const tic_variables * variables,
  const tic_settings * settings)
{
  if (variables == NULL || settings == NULL)
  {
    return 0;
  }

  uint16_t input = tic_variables_get_input_after_hysteresis(variables);

  if (input == TIC_INPUT_NULL)
  {
    return TIC_INPUT_NULL;
  }

  uint8_t shift = tic_input_shift_before_scaling(
    tic_settings_get_control_mode(settings),
    tic_settings_get_input_averaging_enabled(settings));

  return input >> shift;
}

int32_t tic_variables_get_input_after_scaling(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->input_after_scaling;
}

uint8_t tic_variables_get_last_motor_driver_error(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->last_motor_driver_error;
}

uint8_t tic_variables_get_agc_mode(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->agc_mode;
}

uint8_t tic_variables_get_agc_bottom_current_limit(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->agc_bottom_current_limit;
}

uint8_t tic_variables_get_agc_current_boost_steps(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->agc_current_boost_steps;
}

uint8_t tic_variables_get_agc_frequency_limit(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->agc_frequency_limit;
}

uint16_t tic_variables_get_analog_reading(const tic_variables * variables,
  uint8_t pin)
{
  if (variables == NULL || pin >= PIN_COUNT) { return 0; }
  return variables->pin_info[pin].analog_reading;
}

bool tic_variables_get_digital_reading(const tic_variables * variables,
  uint8_t pin)
{
  if (variables == NULL || pin >= PIN_COUNT) { return 0; }
  return variables->pin_info[pin].digital_reading;
}

uint8_t tic_variables_get_pin_state(const tic_variables * variables,
  uint8_t pin)
{
  if (variables == NULL || pin >= PIN_COUNT) { return 0; }
  return variables->pin_info[pin].pin_state;
}

uint32_t tic_variables_get_last_hp_driver_errors(const tic_variables * variables)
{
  if (variables == NULL) { return 0; }
  return variables->last_hp_driver_errors;
}

tic_variables * tic_variables_fake(void)
{
  tic_variables * vars = (tic_variables *)calloc(1, sizeof(tic_variables));
  vars->product = TIC_PRODUCT_T825;
  vars->operation_state = TIC_OPERATION_STATE_NORMAL;
  vars->energized = 1;
  vars->position_uncertain = 1;
  vars->forward_limit_active = 0;
  vars->reverse_limit_active = 1;
  vars->homing_active = 0;
  vars->error_status = 0x3FF;
  vars->errors_occurred = 0x3F03FF;
  vars->planning_mode = TIC_PLANNING_MODE_TARGET_POSITION;
  vars->target_position = 1234;
  vars->target_velocity = -44;
  vars->starting_speed = 1000;
  vars->max_speed = 2023714;
  vars->max_decel = 23456;
  vars->max_accel = 34567;
  vars->current_position = 500;
  vars->current_velocity = 951145;
  vars->acting_target_position = vars->target_position + 100;
  vars->time_since_last_step = 90;
  vars->device_reset = TIC_RESET_STACK_UNDERFLOW;
  vars->vin_voltage = 7005;
  vars->up_time = 3725010;
  vars->encoder_position = -801;
  vars->rc_pulse_width = 1555 * 12;
  vars->step_mode = TIC_STEP_MODE_HALF;
  vars->current_limit_code = 0xFF;
  vars->decay_mode = TIC_DECAY_MODE_SLOW;
  vars->input_state = TIC_INPUT_STATE_POSITION;
  vars->input_after_averaging = 0;
  vars->input_after_hysteresis = 0;
  vars->input_after_scaling = 0;
  vars->last_motor_driver_error = TIC_MOTOR_DRIVER_ERROR_OVERCURRENT;
  vars->agc_mode = TIC_AGC_MODE_ON;
  vars->agc_bottom_current_limit = 0xFF;
  vars->agc_current_boost_steps = 0xFF;
  vars->agc_frequency_limit = 0xFF;

  vars->pin_info[TIC_PIN_NUM_SCL].analog_reading = 100;
  vars->pin_info[TIC_PIN_NUM_SCL].digital_reading = 0;
  vars->pin_info[TIC_PIN_NUM_SCL].pin_state = TIC_PIN_STATE_HIGH_IMPEDANCE;

  vars->pin_info[TIC_PIN_NUM_SDA].analog_reading = 900;
  vars->pin_info[TIC_PIN_NUM_SDA].digital_reading = 1;
  vars->pin_info[TIC_PIN_NUM_SDA].pin_state = TIC_PIN_STATE_OUTPUT_HIGH;

  vars->pin_info[TIC_PIN_NUM_TX].analog_reading = 200;
  vars->pin_info[TIC_PIN_NUM_TX].digital_reading = 0;
  vars->pin_info[TIC_PIN_NUM_TX].pin_state = TIC_PIN_STATE_OUTPUT_LOW;

  vars->pin_info[TIC_PIN_NUM_RX].analog_reading = 800;
  vars->pin_info[TIC_PIN_NUM_RX].digital_reading = 1;
  vars->pin_info[TIC_PIN_NUM_RX].pin_state = TIC_PIN_STATE_PULLED_UP;

  vars->pin_info[TIC_PIN_NUM_RC].digital_reading = 0;
  vars->pin_info[TIC_PIN_NUM_RC].pin_state = TIC_PIN_STATE_HIGH_IMPEDANCE;

  return vars;
}
