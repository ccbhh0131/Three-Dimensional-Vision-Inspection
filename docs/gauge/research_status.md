# Gauge Research Status

## Research chain

The research boundary currently separates four ideas:

1. Gauge-region localization.
2. Pointer or geometry extraction.
3. A configurable `GaugeProfile` describing scale and reading conventions.
4. Reading geometry that converts an angle into a profile-relative value.

The public module contains only reviewed, path-independent geometry code and
documentation. It is not linked into the formal Qt runtime in V0.1.0.

## Baseline decision

Pure pose estimation was evaluated as a research baseline. Strong pose metrics
alone did not establish reliable angle accuracy for the intended reading task,
so it is not selected as the sole public measurement path. A hybrid direction
combining localization, image geometry and an explicit profile remains a
research direction rather than a product guarantee.

## Not claimed

This module does not claim industrial accuracy, calibrated pressure readings,
real-time performance, device integration, or robustness across unseen gauge
families. Private images, annotations, model checkpoints, training runs and
external research checkouts remain excluded.
