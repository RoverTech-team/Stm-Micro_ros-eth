"""
JSN-SR04T ultrasonic sensor helper for Renode and offline validation.

This module can run in two modes:
1) In Renode (loaded via `emulation LoadPythonExtension`) to provide helper commands.
2) Outside Renode for fast local logic checks.

Public commands exposed for Renode scripts:
- sensor_set_distance(cm)
- sensor_get_distance()
- sensor_set_timeout(timeout_us)
- sensor_get_timeout()
- sensor_get_echo_pulse_width()
- sensor_trigger(simulate_timing=False)
- sensor_reset(reset_distance=True)
- sensor_get_state()
- sensor_get_info()
- sensor_get_info_json()
- sensor_print_result(key, value)
- sensor_dump_standard_results()
"""

import json
import time

DEFAULT_DISTANCE_CM = 50
MIN_DISTANCE_CM = 20
MAX_DISTANCE_CM = 450
DEFAULT_TIMEOUT_US = 30000
ECHO_MULTIPLIER = 58
PROCESSING_DELAY_US = 100


class JSNSR04TSimulation:
    """Software model of JSN-SR04T (Mode 1 pulse protocol)."""

    def __init__(self, machine=None):
        self.machine = machine
        self.default_distance = DEFAULT_DISTANCE_CM
        self.min_distance = MIN_DISTANCE_CM
        self.max_distance = MAX_DISTANCE_CM
        self.timeout_us = DEFAULT_TIMEOUT_US

        self.distance_cm = DEFAULT_DISTANCE_CM
        self.state = "idle"
        self.last_echo_width_us = self._distance_to_echo_width(self.distance_cm)
        self.measurement_count = 0
        self.timeout_count = 0
        self.last_trigger_epoch = None

    def _distance_to_echo_width(self, distance_cm):
        if distance_cm < self.min_distance or distance_cm <= 0:
            return 0
        return distance_cm * ECHO_MULTIPLIER

    def set_distance(self, cm):
        if cm < 0:
            self.distance_cm = 0
        elif cm > self.max_distance:
            self.distance_cm = self.max_distance
        else:
            self.distance_cm = int(cm)

        self.last_echo_width_us = self._distance_to_echo_width(self.distance_cm)
        return self.distance_cm

    def get_distance(self):
        return self.distance_cm

    def set_timeout(self, timeout_us):
        self.timeout_us = max(0, int(timeout_us))
        return self.timeout_us

    def get_timeout(self):
        return self.timeout_us

    def get_echo_pulse_width(self):
        return self._distance_to_echo_width(self.distance_cm)

    def trigger(self, simulate_timing=False):
        if self.state in ("measuring", "triggered"):
            return self.last_echo_width_us

        self.state = "triggered"
        self.last_trigger_epoch = time.time()

        echo_width_us = self.get_echo_pulse_width()
        if echo_width_us == 0:
            self.state = "timeout"
            self.timeout_count += 1
            if simulate_timing and self.timeout_us > 0:
                time.sleep(self.timeout_us / 1000000.0)
            self.state = "idle"
            self.last_echo_width_us = 0
            return 0

        self.state = "measuring"
        if simulate_timing:
            time.sleep(PROCESSING_DELAY_US / 1000000.0)
            time.sleep(echo_width_us / 1000000.0)

        self.measurement_count += 1
        self.last_echo_width_us = echo_width_us
        self.state = "ready"

        # Return to idle immediately after publishing the result.
        self.state = "idle"
        return echo_width_us

    def reset(self, reset_distance=True):
        self.state = "idle"
        self.last_echo_width_us = 0
        if reset_distance:
            self.distance_cm = self.default_distance
            self.last_echo_width_us = self._distance_to_echo_width(self.distance_cm)

    def get_state(self):
        return self.state

    def get_info(self):
        return {
            "distance_cm": self.distance_cm,
            "default_distance_cm": self.default_distance,
            "min_distance_cm": self.min_distance,
            "max_distance_cm": self.max_distance,
            "timeout_us": self.timeout_us,
            "echo_pulse_width_us": self.get_echo_pulse_width(),
            "measurement_count": self.measurement_count,
            "timeout_count": self.timeout_count,
            "state": self.state,
        }


_sensor = None


try:
    from Antmicro.Renode.Core import EmulationManager
except ImportError:
    # Fallback for older Renode versions or when running outside Renode
    try:
        import EmulationManager
    except ImportError:
        EmulationManager = None

def _try_get_machine_from_renode():
    if EmulationManager is None:
        return None
    try:
        return EmulationManager.Instance.CurrentEmulation.Machine
    except Exception:
        try:
            return EmulationManager.CurrentEmulation.Machine
        except Exception:
            return None


def _initialize_sensor(machine=None):
    global _sensor
    _sensor = JSNSR04TSimulation(machine=machine)
    print("[JSN_SR04T] helper initialized")
    return _sensor


def get_sensor():
    global _sensor
    if _sensor is None:
        _initialize_sensor(machine=_try_get_machine_from_renode())
    return _sensor


# ===== Public API exposed in Renode =====


def create_sensor(machine):
    sensor = _initialize_sensor(machine=machine)
    try:
        machine.SetExternalProperty(sensor, "jsn_sr04t")
    except Exception:
        pass
    return sensor


def sensor_set_distance(distance):
    value = get_sensor().set_distance(int(distance))
    return value


def sensor_get_distance():
    return get_sensor().get_distance()


def sensor_set_timeout(timeout_us):
    return get_sensor().set_timeout(int(timeout_us))


def sensor_get_timeout():
    return get_sensor().get_timeout()


def sensor_get_echo_pulse_width():
    return get_sensor().get_echo_pulse_width()


def sensor_trigger(simulate_timing=False):
    return get_sensor().trigger(simulate_timing=bool(simulate_timing))


def sensor_reset(reset_distance=True):
    get_sensor().reset(reset_distance=bool(reset_distance))


def sensor_get_state():
    return get_sensor().get_state()


def sensor_get_info():
    return get_sensor().get_info()


def sensor_get_info_json():
    return json.dumps(sensor_get_info(), sort_keys=True)


def sensor_print_result(key, value):
    print("RESULT {}={}".format(key, value))


def sensor_dump_standard_results():

    get_sensor().reset(reset_distance=True)
    sensor_print_result("initialized", 1)
    sensor_print_result("default_distance", get_sensor().get_distance())

    get_sensor().set_distance(100)
    sensor_print_result("distance_100", get_sensor().get_distance())

    get_sensor().set_distance(MIN_DISTANCE_CM)
    sensor_print_result("distance_min", get_sensor().get_distance())
    sensor_print_result("echo_min", get_sensor().get_echo_pulse_width())

    get_sensor().set_distance(MAX_DISTANCE_CM)
    sensor_print_result("distance_max", get_sensor().get_distance())
    sensor_print_result("echo_max", get_sensor().get_echo_pulse_width())

    get_sensor().set_distance(500)
    sensor_print_result("clamped_high", get_sensor().get_distance())

    get_sensor().set_distance(-10)
    sensor_print_result("clamped_negative", get_sensor().get_distance())

    # Sequence test
    seq = [70, 90, 110, 130, 150]
    seq_ok = 1
    for idx, expected in enumerate(seq):
        get_sensor().set_distance(expected)
        if get_sensor().get_distance() != expected:
            sensor_print_result("sequence_fail_idx", idx)
            break
    sensor_print_result("sequence_ok", seq_ok)

    # Echo width formula samples
    for distance in (20, 50, 100, 200, 450):
        get_sensor().set_distance(distance)
        sensor_print_result(
            "echo_{}".format(distance), get_sensor().get_echo_pulse_width()
        )

    sensor_print_result("timeout_us", get_sensor().get_timeout())

    get_sensor().set_distance(200)
    get_sensor().trigger(simulate_timing=False)
    sensor_print_result("trigger_echo_width", get_sensor().last_echo_width_us)
    sensor_print_result("state_after_trigger", get_sensor().get_state())

    get_sensor().reset(reset_distance=True)
    sensor_print_result("distance_after_reset", get_sensor().get_distance())


def register_jsn_sensor():
    """Register the JSN-SR04T sensor with GPIO pins PD0 (ECHO) and PD1 (TRIG)."""
    try:
        machine = EmulationManager.CurrentEmulation.Machine
        sensor = get_sensor()

        trig_gpio = machine.GetOrCreateGPIO(1)  # PD1
        echo_gpio = machine.GetOrCreateGPIO(0)  # PD0

        def on_trig_changed(value):
            if sensor.trigger(simulate_timing=False):
                echo_width_us = sensor.last_echo_width_us
                if echo_width_us > 0:
                    echo_gpio.Set(True)
                    echo_gpio.Set(False)

        trig_gpio.Connect(on_trig_changed)

        sensor_print_result("sensor_registered", 1)
        print("[JSN_SR04T] Sensor registered with GPIO bindings")
        return True
    except Exception as e:
        print("[JSN_SR04T] Failed to register sensor: {}".format(e))
        sensor_print_result("sensor_registered", 0)
        return False


# Backward-compatible aliases used by older scripts.
def set_distance(distance):
    return sensor_set_distance(distance)


def get_distance():
    return sensor_get_distance()


def set_timeout(timeout_us):
    return sensor_set_timeout(timeout_us)


def get_timeout():
    return sensor_get_timeout()


def get_echo_pulse_width():
    return sensor_get_echo_pulse_width()


def trigger(simulate_timing=False):
    return sensor_trigger(simulate_timing=simulate_timing)


def reset(reset_distance=True):
    sensor_reset(reset_distance=reset_distance)


# Initialize automatically when running in Renode (or standalone fallback).
try:
    _initialize_sensor(machine=_try_get_machine_from_renode())
except Exception:
    _initialize_sensor(machine=None)
