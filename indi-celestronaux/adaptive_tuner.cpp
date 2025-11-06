#include "adaptive_tuner.h"
#include <numeric> // for std::accumulate
#include <algorithm> // for std::min, std::max
#include <iostream> // For temporary debugging

// A simple clamp function
template <typename T>
T clamp(T value, T min_val, T max_val)
{
    return std::max(min_val, std::min(value, max_val));
}

AdaptivePIDTuner::AdaptivePIDTuner(double dt, double initialKp, double initialKi, double initialKd,
                                   double omega_n_ref, double zeta_ref)
    : m_ref_omega_n(omega_n_ref), m_ref_zeta(zeta_ref),
      m_dt(dt), m_currentKp(initialKp), m_currentKi(initialKi), m_currentKd(initialKd)
{
    reset();
}

AdaptivePIDTuner::~AdaptivePIDTuner() = default;

void AdaptivePIDTuner::setReferenceModelParams(double omega_n, double zeta)
{
    m_ref_omega_n = std::max(0.01, omega_n); // Ensure positive
    m_ref_zeta    = std::max(0.01, zeta);    // Ensure positive
}

void AdaptivePIDTuner::setGainLimits(double minKp, double maxKp, double minKi, double maxKi, double minKd, double maxKd)
{
    m_minKp = minKp;
    m_maxKp = maxKp;
    m_minKi = minKi;
    m_maxKi = maxKi;
    m_minKd = minKd;
    m_maxKd = maxKd;

    // Ensure current gains are within new limits
    m_currentKp = clamp(m_currentKp, m_minKp, m_maxKp);
    m_currentKi = clamp(m_currentKi, m_minKi, m_maxKi);
    m_currentKd = clamp(m_currentKd, m_minKd, m_maxKd);
}

void AdaptivePIDTuner::setAdaptationStepSizes(double stepKp, double stepKi, double stepKd)
{
    m_stepKp = std::fabs(stepKp);
    m_stepKi = std::fabs(stepKi);
    m_stepKd = std::fabs(stepKd);
}

void AdaptivePIDTuner::setAdaptationAggressiveness(double aggressiveness)
{
    m_aggressiveness = std::max(0.0, aggressiveness);
}

void AdaptivePIDTuner::setHistorySize(size_t size)
{
    m_history_size = std::max(static_cast<size_t>(10), size); // Need some minimum history
    m_min_data_for_tuning = std::max(static_cast<size_t>(10), m_history_size / 2); // Update this too

    // Trim histories if they are now too long
    while (m_error_history.size() > m_history_size) m_error_history.pop_front();
    while (m_plant_output_history.size() > m_history_size) m_plant_output_history.pop_front();
    while (m_setpoint_history.size() > m_history_size) m_setpoint_history.pop_front();
}

void AdaptivePIDTuner::startActiveTuning()
{
    m_is_tuning_active = true;
    // m_has_gathered_sufficient_data will be set once enough data points are collected in processMeasurement
}

void AdaptivePIDTuner::stopActiveTuning()
{
    m_is_tuning_active = false;
    m_has_gathered_sufficient_data = false; // Reset this so data gathering restarts if tuning is re-enabled
}

bool AdaptivePIDTuner::isActivelyTuning() const
{
    return m_is_tuning_active && m_has_gathered_sufficient_data;
}

bool AdaptivePIDTuner::hasGatheredSufficientData() const
{
    return m_has_gathered_sufficient_data;
}

void AdaptivePIDTuner::reset()
{
    // Reset reference model state
    m_ref_x1 = 0.0; // Assuming initial plant output is 0 or will be quickly provided
    m_ref_x2 = 0.0;

    // Reset history
    m_error_history.clear();
    m_plant_output_history.clear();
    m_setpoint_history.clear();

    // Reset flags
    // m_is_tuning_active is user-controlled, don't reset here unless intended
    m_has_gathered_sufficient_data = false;

    // Note: Current Kp, Ki, Kd are NOT reset here by default.
    // They hold the last tuned values or initial values.
    // If a full reset to initial gains is needed, it should be done explicitly
    // by the caller by re-setting them or re-initializing the tuner.
}

void AdaptivePIDTuner::processMeasurement(double setpoint_r, double plant_output_yp)
{
    // 1. Update Reference Model State (using Euler integration for simplicity)
    // x1_dot = x2
    // x2_dot = -omega_n^2 * x1 - 2*zeta*omega_n * x2 + omega_n^2 * r
    double x1_prev = m_ref_x1;
    double x2_prev = m_ref_x2;

    m_ref_x1 = x1_prev + m_dt * x2_prev;
    m_ref_x2 = x2_prev + m_dt * (-m_ref_omega_n * m_ref_omega_n * x1_prev -
                                 2 * m_ref_zeta * m_ref_omega_n * x2_prev +
                                 m_ref_omega_n * m_ref_omega_n * setpoint_r);

    // 2. Calculate adaptation error
    double error_adapt = plant_output_yp - m_ref_x1; // y_p - y_m

    // 3. Store in history buffers
    m_error_history.push_back(error_adapt);
    m_plant_output_history.push_back(plant_output_yp);
    m_setpoint_history.push_back(setpoint_r);

    while (m_error_history.size() > m_history_size) m_error_history.pop_front();
    while (m_plant_output_history.size() > m_history_size) m_plant_output_history.pop_front();
    while (m_setpoint_history.size() > m_history_size) m_setpoint_history.pop_front();

    // 4. Check if enough data gathered
    if (m_is_tuning_active && !m_has_gathered_sufficient_data)
    {
        if (m_error_history.size() >= m_min_data_for_tuning)
        {
            m_has_gathered_sufficient_data = true;
            // Optionally, initialize m_ref_x1 to plant_output_yp here if it's the first time
            // to avoid initial large error if setpoint_r was also 0.
            // m_ref_x1 = plant_output_yp; // Consider this if initial transient is an issue
        }
    }

    // 5. If actively tuning and have enough data, adjust gains
    if (isActivelyTuning())
    {
        analyzeErrorAndAdjustGains();
    }
}

void AdaptivePIDTuner::getAdaptedGains(double &outKp, double &outKi, double &outKd) const
{
    outKp = m_currentKp;
    outKi = m_currentKi;
    outKd = m_currentKd;
}

void AdaptivePIDTuner::getCurrentReferenceModelOutput(double &outYm) const
{
    outYm = m_ref_x1;
}


// --- Helper methods for analysis ---

double AdaptivePIDTuner::calculateMean(const std::deque<double> &data) const
{
    if (data.empty()) return 0.0;
    double sum = std::accumulate(data.begin(), data.end(), 0.0);
    return sum / data.size();
}

double AdaptivePIDTuner::calculateStdDev(const std::deque<double> &data, double mean) const
{
    if (data.size() < 2) return 0.0;
    double sq_sum = 0.0;
    for (double val : data)
    {
        sq_sum += (val - mean) * (val - mean);
    }
    return std::sqrt(sq_sum / (data.size() - 1)); // Sample standard deviation
}

int AdaptivePIDTuner::countSignChanges(const std::deque<double> &data) const
{
    if (data.size() < 2) return 0;
    int changes = 0;
    for (size_t i = 0; i < data.size() - 1; ++i)
    {
        if ((data[i] > 0 && data[i + 1] < 0) || (data[i] < 0 && data[i + 1] > 0))
        {
            changes++;
        }
    }
    return changes;
}

// This is the core heuristic logic - needs careful design and testing
void AdaptivePIDTuner::analyzeErrorAndAdjustGains()
{
    if (m_error_history.size() < m_min_data_for_tuning) return;

    // Characteristics of the adaptation error (e_adapt = plant_output - model_output)
    double error_mean   = calculateMean(m_error_history);
    double error_stddev = calculateStdDev(m_error_history, error_mean);
    int error_oscillations = countSignChanges(m_error_history);

    // Characteristics of the plant output (yp) relative to setpoint (r)
    // This can give clues about overall system performance, not just model following.
    // For simplicity, we'll primarily focus on e_adapt for now.

    // --- Heuristic Rules ---
    // These are very basic starting points and will need significant refinement.
    // The goal is to drive e_adapt towards zero.

    double effective_stepKp = m_stepKp * m_aggressiveness;
    double effective_stepKi = m_stepKi * m_aggressiveness;
    double effective_stepKd = m_stepKd * m_aggressiveness;

    // Rule 1: Reduce steady-state error (non-zero mean of e_adapt)
    // If model is consistently above plant (error_mean < 0), or plant above model (error_mean > 0)
    // This suggests the overall gain or integral action of the *plant's PID* might be off.
    // We adjust Ki of the plant's PID to compensate.
    if (std::fabs(error_mean) > 0.01) // Some tolerance
    {
        if (error_mean > 0) // Plant is higher than model, or model is too slow
        {
            // To make plant come down (or model speed up), if we assume plant PID is too aggressive:
            // Try reducing plant's Ki or Kp.
            // Let's try reducing Ki first as it targets steady state.
            m_currentKi -= effective_stepKi;
        }
        else // Plant is lower than model, or model is too fast
        {
            // To make plant come up (or model slow down), if we assume plant PID is not aggressive enough:
            m_currentKi += effective_stepKi;
        }
    }

    // Rule 2: Reduce oscillations (high stddev or many sign changes in e_adapt)
    // This suggests Kp might be too high or Kd too low in the plant's PID.
    // A high number of sign changes in e_adapt indicates the plant is oscillating around the model.
    // A high stddev also indicates poor tracking of the model.
    // Let's use error_oscillations as a primary indicator for now.
    // Threshold for "too many" oscillations needs tuning. e.g. > history.size()/10
    size_t oscillation_threshold = m_history_size / 10;
    if (error_oscillations > static_cast<int>(oscillation_threshold) || error_stddev > 0.1) // Tolerance for stddev
    {
        // Too much oscillation: reduce Kp, increase Kd
        m_currentKp -= effective_stepKp;
        m_currentKd += effective_stepKd;
    }
    else
    {
        // If not oscillating much, but error_mean is still an issue,
        // Kp might be too low (sluggish response to follow the model).
        // This is tricky because increasing Kp can cause oscillations.
        // Let's only do this if error_mean is significant and oscillations are low.
        if (std::fabs(error_mean) > 0.05 && error_oscillations < static_cast<int>(oscillation_threshold / 2))
        {
            if (error_mean < 0) // Plant lagging model
            {
                m_currentKp += effective_stepKp / 2.0; // Smaller increment
            }
            else // Plant leading model (less common if model is well-behaved)
            {
                m_currentKp -= effective_stepKp / 2.0;
            }
        }
    }

    // Rule 3: Responsiveness (not explicitly handled yet, but related to Kp)
    // If the plant is slow to follow changes in the reference model (even if it eventually gets there),
    // Kp might be too low. This is harder to quantify simply without looking at response to setpoint changes
    // in the reference model itself.

    // Clamp gains to their limits
    m_currentKp = clamp(m_currentKp, m_minKp, m_maxKp);
    m_currentKi = clamp(m_currentKi, m_minKi, m_maxKi);
    m_currentKd = clamp(m_currentKd, m_minKd, m_maxKd);

    // Basic logging for now
    // std::cout << "Tuner: Kp=" << m_currentKp << " Ki=" << m_currentKi << " Kd=" << m_currentKd
    //           << " ErrMean=" << error_mean << " ErrStdDev=" << error_stddev
    //           << " ErrOsc=" << error_oscillations << std::endl;
}
