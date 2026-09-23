# Gauge Profile Schema

The profile describes the scale convention independently from the detector or
image-processing method. The example in `examples/profile_schema.example.json`
uses synthetic demonstration values only.

| Field | Type | Meaning |
|---|---|---|
| `schemaVersion` | integer | Profile schema version. |
| `profileId` | string | Stable identifier chosen by the user. |
| `unit` | string | Display unit or research label. |
| `rangeMin` / `rangeMax` | number | Value interval represented by the scale. |
| `scaleStartAngleDeg` | number | Angle at the minimum end of the scale. |
| `scaleEndAngleDeg` | number | Angle at the maximum end of the scale. |
| `clockwise` | boolean | Direction used when interpreting increasing angle. |
| `source` | string | Provenance label for the profile. |

Profiles are configuration data, not calibration proof. A production system
would need traceable calibration, uncertainty rules and acceptance tests.
