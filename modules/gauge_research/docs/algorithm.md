# Gauge Reading Geometry Algorithm

The public helper implements a small, deterministic geometry layer:

1. Normalize the pointer angle according to the configured angular convention.
2. Compute the signed progress between the profile's scale start and end
   angles.
3. Map that progress linearly to the configured value range.
4. Return the value together with validation information for invalid or
   degenerate profiles.

The implementation is intentionally independent of image files, model
checkpoints, training frameworks and machine-specific directories. It is a
building block for research evaluation, not a complete gauge reader.

See `gauge_reading_geometry.py` for the implementation and
`test_gauge_reading_geometry.py` for synthetic unit cases.
