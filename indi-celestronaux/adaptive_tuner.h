#pragma once

#include <vector>
#include <deque>
#include <cmath> // For std::fabs, std::sqrt

// Forward declaration
class PID;

class AdaptivePIDTuner
{
    public:
        AdaptivePIDTuner(double dt, double initialKp, double initialKi, double initialKd,
                         double omega_n_ref, double zeta_ref);
        ~AdaptivePIDTuner();

        void setReferenceModelParams(double omega_n, double zeta);
        void setGainLimits(double minKp, double maxKp, double minKi, double maxKi, double minKd, double maxKd);
        void setAdaptationStepSizes(double stepKp, double stepKi, double stepKd);
        void setAdaptationAggressiveness(double aggressiveness); // General tuning parameter for step sizes
        void setHistorySize(size_t size); // How many samples to keep for analysis

        void startActiveTuning();
        void stopActiveTuning();
        bool isActivelyTuning() const;
        bool hasGatheredSufficientData() const;

        void processMeasurement(double setpoint_r, double plant_output_yp);
        void getAdaptedGains(double &outKp, double &outKi, double &outKd) const;
        void getCurrentReferenceModelOutput(double &outYm) const;

        void reset();

    private:
        // Internal state for the reference model (2nd order system)
        // y_m_dot_dot + 2*zeta*omega_n*y_m_dot + omega_n^2*y_m = omega_n^2*r
        // Using state-space representation:
        // x1 = y_m
        // x2 = y_m_dot
        // x1_dot = x2
        // x2_dot = -omega_n^2 * x1 - 2*zeta*omega_n * x2 + omega_n^2 * r
        double m_ref_x1 { 0.0 }; // y_m (reference model output)
        double m_ref_x2 { 0.0 }; // y_m_dot (reference model output derivative)

        // Reference Model Parameters
        double m_ref_omega_n { 1.0 }; // Natural frequency (rad/s)
        double m_ref_zeta { 1.0 };    // Damping ratio

        // Tuner state
        double m_dt { 0.1 };         // Sample time (seconds)
        double m_currentKp { 0.0 };
        double m_currentKi { 0.0 };
        double m_currentKd { 0.0 };

        // Gain limits
        double m_minKp { 0.0 }, m_maxKp { 100.0 };
        double m_minKi { 0.0 }, m_maxKi { 100.0 };
        double m_minKd { 0.0 }, m_maxKd { 100.0 };

        // Adaptation parameters
        double m_stepKp { 0.01 };
        double m_stepKi { 0.001 };
        double m_stepKd { 0.001 };
        double m_aggressiveness { 1.0 }; // Multiplier for step sizes

        // History buffers for analysis
        std::deque<double> m_error_history;      // e_adapt = plant_output_yp - y_m
        std::deque<double> m_plant_output_history; // yp
        std::deque<double> m_setpoint_history;     // r
        size_t m_history_size { 100 }; // e.g., 10 seconds of data if dt = 0.1s
        size_t m_min_data_for_tuning { 50 }; // Need at least this much data to start tuning

        bool m_is_tuning_active { false };
        bool m_has_gathered_sufficient_data { false };

        // Helper methods for analysis (to be implemented in .cpp)
        void analyzeErrorAndAdjustGains();
        double calculateMean(const std::deque<double> &data) const;
        double calculateStdDev(const std::deque<double> &data, double mean) const;
        int countSignChanges(const std::deque<double> &data) const;
};
