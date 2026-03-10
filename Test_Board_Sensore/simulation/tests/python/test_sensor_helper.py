import unittest

from simulation.python.sensor_helper import (
    DEFAULT_DISTANCE_CM,
    DEFAULT_TIMEOUT_US,
    JSNSR04TSimulation,
    MAX_DISTANCE_CM,
    MIN_DISTANCE_CM,
)


class TestSensorHelper(unittest.TestCase):
    def setUp(self):
        self.sensor = JSNSR04TSimulation()

    def test_defaults(self):
        self.assertEqual(self.sensor.get_distance(), DEFAULT_DISTANCE_CM)
        self.assertEqual(self.sensor.get_timeout(), DEFAULT_TIMEOUT_US)
        self.assertEqual(self.sensor.get_echo_pulse_width(), DEFAULT_DISTANCE_CM * 58)

    def test_distance_clamping(self):
        self.assertEqual(self.sensor.set_distance(500), MAX_DISTANCE_CM)
        self.assertEqual(self.sensor.set_distance(-3), 0)
        self.assertEqual(self.sensor.set_distance(MIN_DISTANCE_CM), MIN_DISTANCE_CM)

    def test_echo_formula(self):
        self.sensor.set_distance(100)
        self.assertEqual(self.sensor.get_echo_pulse_width(), 5800)

    def test_trigger_success(self):
        self.sensor.set_distance(120)
        width = self.sensor.trigger(simulate_timing=False)
        self.assertEqual(width, 120 * 58)
        self.assertEqual(self.sensor.get_state(), "idle")

    def test_trigger_timeout(self):
        self.sensor.set_distance(0)
        width = self.sensor.trigger(simulate_timing=False)
        self.assertEqual(width, 0)
        self.assertEqual(self.sensor.timeout_count, 1)

    def test_reset(self):
        self.sensor.set_distance(200)
        self.sensor.reset(reset_distance=True)
        self.assertEqual(self.sensor.get_distance(), DEFAULT_DISTANCE_CM)


if __name__ == "__main__":
    unittest.main()
