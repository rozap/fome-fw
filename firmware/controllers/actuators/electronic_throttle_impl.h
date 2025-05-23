/**
 * @file	electronic_throttle_impl.h
 *
 * @date Dec 7, 2013
 * @author Andrey Belomutskiy, (c) 2012-2020
 */

#pragma once

// include the "public" ETB interface
#include "electronic_throttle.h"

#include "sensor.h"
#include "efi_pid.h"
#include "electronic_throttle_generated.h"

/**
 * Hard code ETB update speed.
 * Since this is a safety critical system with no real reason for a user to ever need to change the update rate,
 * it's locked to 500hz, along with the ADC.
 * https://en.wikipedia.org/wiki/Nyquist%E2%80%93Shannon_sampling_theorem
 */
#define ETB_LOOP_FREQUENCY 500
#define DEFAULT_ETB_PWM_FREQUENCY 800

class EtbController : public IEtbController, public electronic_throttle_s {
public:
	bool init(dc_function_e function, DcMotor *motor, pid_s *pidParameters, const ValueProvider3D* pedalMap, bool initializeThrottles) override;
	void setIdlePosition(percent_t pos) override;
	void setWastegatePosition(percent_t pos) override;
	void reset() override;

	// Update the controller's state: read sensors, send output, etc
	void update() override;

	// Called when the configuration may have changed.  Controller will
	// reset if necessary.
	void onConfigurationChange(pid_s* previousConfiguration);

	// Helpers for individual parts of throttle control
	expected<percent_t> observePlant() const override;

	expected<percent_t> getSetpoint() override;
	expected<percent_t> getSetpointEtb();
	expected<percent_t> getSetpointWastegate() const;

	expected<percent_t> getOpenLoop(percent_t target) override;
	expected<percent_t> getClosedLoop(percent_t setpoint, percent_t observation) override;
	expected<percent_t> getClosedLoopAutotune(percent_t setpoint, percent_t actualThrottlePosition);

	void checkJam(percent_t setpoint, percent_t observation);

	void setOutput(expected<percent_t> outputValue) override;

	// Used to inspect the internal PID controller's state
	const pid_state_s& getPidState() const override { return m_pid; };

	// Override if this throttle needs special per-throttle adjustment (bank-to-bank trim, for example)
	virtual percent_t getThrottleTrim(float /*rpm*/, percent_t /*targetPosition*/) const {
		return 0;
	}

	// Lua throttle adjustment
	void setLuaAdjustment(percent_t adjustment) override;
	float getLuaAdjustment() const;

	float prevOutput = 0;

protected:
	bool hadTpsError = false;
	bool hadPpsError = false;

	dc_function_e getFunction() const { return m_function; }
	DcMotor* getMotor() { return m_motor; }

private:
	dc_function_e m_function = DC_None;
	SensorType m_positionSensor = SensorType::Invalid;
	DcMotor *m_motor = nullptr;
	Pid m_pid;
	bool m_shouldResetPid = false;

	/**
	 * @return true if OK, false if should be disabled
	 */
	bool checkStatus();
	bool isEtbMode() const override {
		return m_function == DC_Throttle1 || m_function == DC_Throttle2;
	}

	Timer m_jamDetectTimer;

	// Pedal -> target map
	const ValueProvider3D* m_pedalMap = nullptr;

	float m_idlePosition = 0;

	// This is set if automatic PID cal should be run
	bool m_isAutotune = false;

	// Autotune helpers
	bool m_lastIsPositive = false;
	Timer m_autotuneCycleStart;
	float m_minCycleTps = 0;
	float m_maxCycleTps = 0;
	// Autotune measured parameters: gain and ultimate period
	// These are set to correct order of magnitude starting points
	// so we converge more quickly on the correct values
	float m_a = 8;
	float m_tu = 0.1f; 

	uint8_t m_autotuneCounter = 0;
	uint8_t m_autotuneCurrentParam = 0;

	Timer m_luaAdjustmentTimer;
};

void etbPidReset();

class EtbController1 : public EtbController { };

class EtbController2 : public EtbController {
public:
	percent_t getThrottleTrim(float rpm, percent_t targetPosition) const override;
};
