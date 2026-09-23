"""Pure mathematical GaugeProfile -> needle angle/reading proof of concept."""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Any, Mapping, Sequence


@dataclass(frozen=True)
class GeometryResult:
    valid: bool
    needle_angle_deg: float | None
    normalized_position: float | None
    reading: float | None
    error: str | None = None

    def to_dict(self) -> dict[str, Any]:
        return {
            "valid": self.valid,
            "needleAngle": self.needle_angle_deg,
            "normalizedPosition": self.normalized_position,
            "reading": self.reading,
            "error": self.error,
        }


def _point(point: Mapping[str, Any] | Sequence[float]) -> tuple[float, float] | None:
    try:
        if isinstance(point, Mapping):
            x, y = point["x"], point["y"]
        else:
            x, y = point[0], point[1]
        x, y = float(x), float(y)
    except (KeyError, IndexError, TypeError, ValueError):
        return None
    if not (math.isfinite(x) and math.isfinite(y)):
        return None
    return x, y


def _angle_deg(center: tuple[float, float], tip: tuple[float, float]) -> float | None:
    dx = tip[0] - center[0]
    dy = center[1] - tip[1]
    length = math.hypot(dx, dy)
    if not math.isfinite(length) or length == 0:
        return None
    return math.degrees(math.atan2(dy, dx)) % 360.0


def _forward_distance(start: float, end: float, clockwise: bool) -> float:
    if clockwise:
        return (start - end) % 360.0
    return (end - start) % 360.0


def _distance_on_circle(first: float, second: float) -> float:
    difference = abs((first - second) % 360.0)
    return min(difference, 360.0 - difference)


def compute_reading(
    center: Mapping[str, Any] | Sequence[float],
    tip: Mapping[str, Any] | Sequence[float],
    profile: Mapping[str, Any],
) -> GeometryResult:
    """Compute geometry without image/model dependencies.

    Angles use the image-safe convention used by this project: 0 degrees is
    right, 90 degrees is up, and positive angles turn counter-clockwise in the
    displayed dial.  For a clockwise profile the scale moves from start to end
    by decreasing angle.
    """

    center_xy = _point(center)
    tip_xy = _point(tip)
    if center_xy is None or tip_xy is None:
        return GeometryResult(False, None, None, None, "INVALID_CENTER_OR_TIP")
    angle = _angle_deg(center_xy, tip_xy)
    if angle is None:
        return GeometryResult(False, None, None, None, "ZERO_LENGTH_VECTOR")

    start = profile.get("scaleStartAngleDeg")
    end = profile.get("scaleEndAngleDeg")
    if start is None or end is None:
        return GeometryResult(True, angle, None, None, "PROFILE_ANGLE_MISSING")
    try:
        start, end = float(start) % 360.0, float(end) % 360.0
        clockwise = bool(profile.get("clockwise", True))
    except (TypeError, ValueError):
        return GeometryResult(True, angle, None, None, "INVALID_PROFILE_ANGLES")
    if not (math.isfinite(start) and math.isfinite(end)):
        return GeometryResult(True, angle, None, None, "INVALID_PROFILE_ANGLES")

    total = _forward_distance(start, end, clockwise)
    if total == 0:
        return GeometryResult(True, angle, None, None, "ZERO_LENGTH_PROFILE_ARC")
    current = _forward_distance(start, angle, clockwise)
    if current <= total:
        normalized = current / total
    else:
        # The point is outside the configured arc. Clamp to the nearer end.
        normalized = 0.0 if _distance_on_circle(angle, start) <= _distance_on_circle(angle, end) else 1.0

    range_min = profile.get("rangeMin")
    range_max = profile.get("rangeMax")
    reading = None
    if range_min is not None and range_max is not None:
        try:
            range_min, range_max = float(range_min), float(range_max)
            if math.isfinite(range_min) and math.isfinite(range_max):
                reading = range_min + normalized * (range_max - range_min)
        except (TypeError, ValueError):
            reading = None
    return GeometryResult(True, angle, max(0.0, min(1.0, normalized)), reading)
