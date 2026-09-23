import math
import unittest

from gauge_reading_geometry import compute_reading


def point_at(angle_deg: float, radius: float = 1.0) -> tuple[float, float]:
    radians = math.radians(angle_deg)
    return radius * math.cos(radians), -radius * math.sin(radians)


PROFILE_CW = {
    "rangeMin": 0.0,
    "rangeMax": 100.0,
    "scaleStartAngleDeg": 180.0,
    "scaleEndAngleDeg": 0.0,
    "clockwise": True,
}


class GaugeReadingGeometryTests(unittest.TestCase):
    def test_normal_middle_angle(self) -> None:
        result = compute_reading((0, 0), point_at(90), PROFILE_CW)
        self.assertTrue(result.valid)
        self.assertAlmostEqual(result.needle_angle_deg, 90.0)
        self.assertAlmostEqual(result.normalized_position, 0.5)
        self.assertAlmostEqual(result.reading, 50.0)

    def test_start_and_end(self) -> None:
        start = compute_reading((0, 0), point_at(180), PROFILE_CW)
        end = compute_reading((0, 0), point_at(0), PROFILE_CW)
        self.assertAlmostEqual(start.normalized_position, 0.0)
        self.assertAlmostEqual(start.reading, 0.0)
        self.assertAlmostEqual(end.normalized_position, 1.0)
        self.assertAlmostEqual(end.reading, 100.0)

    def test_clockwise_wraparound(self) -> None:
        profile = {**PROFILE_CW, "scaleStartAngleDeg": 30.0, "scaleEndAngleDeg": 330.0}
        result = compute_reading((0, 0), point_at(0), profile)
        self.assertAlmostEqual(result.normalized_position, 0.5)

    def test_counter_clockwise_wraparound(self) -> None:
        profile = {**PROFILE_CW, "scaleStartAngleDeg": 330.0, "scaleEndAngleDeg": 30.0, "clockwise": False}
        result = compute_reading((0, 0), point_at(0), profile)
        self.assertAlmostEqual(result.normalized_position, 0.5)

    def test_direction_changes_mapping(self) -> None:
        counter = {**PROFILE_CW, "scaleStartAngleDeg": 0.0, "scaleEndAngleDeg": 180.0, "clockwise": False}
        result = compute_reading((0, 0), point_at(90), counter)
        self.assertAlmostEqual(result.normalized_position, 0.5)

    def test_out_of_range_clamps_to_nearest_boundary(self) -> None:
        before_start = compute_reading((0, 0), point_at(220), PROFILE_CW)
        beyond_end = compute_reading((0, 0), point_at(340), PROFILE_CW)
        self.assertAlmostEqual(before_start.normalized_position, 0.0)
        self.assertAlmostEqual(beyond_end.normalized_position, 1.0)

    def test_invalid_center_and_tip(self) -> None:
        invalid = compute_reading({"x": "bad", "y": 0}, (1, 1), PROFILE_CW)
        self.assertFalse(invalid.valid)
        self.assertEqual(invalid.error, "INVALID_CENTER_OR_TIP")

    def test_zero_length_vector(self) -> None:
        invalid = compute_reading((1, 1), (1, 1), PROFILE_CW)
        self.assertFalse(invalid.valid)
        self.assertEqual(invalid.error, "ZERO_LENGTH_VECTOR")

    def test_unconfirmed_profile_keeps_reading_deferred(self) -> None:
        profile = {
            "profileId": "25-range-gauge",
            "rangeMin": None,
            "rangeMax": None,
            "unit": None,
            "scaleStartAngleDeg": None,
            "scaleEndAngleDeg": None,
            "clockwise": None,
        }
        result = compute_reading((0, 0), point_at(90), profile)
        self.assertTrue(result.valid)
        self.assertIsNotNone(result.needle_angle_deg)
        self.assertIsNone(result.normalized_position)
        self.assertIsNone(result.reading)


if __name__ == "__main__":
    unittest.main()
