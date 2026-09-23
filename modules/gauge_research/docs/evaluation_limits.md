# Gauge Evaluation Limits

The public research module is evaluated only with deterministic synthetic
geometry cases. It does not include the private image set, annotations,
weights, training outputs or external benchmark checkouts used during earlier
research.

The current checks can detect geometry and profile-handling regressions. They
cannot establish:

- accuracy on a real gauge population;
- robustness to glare, occlusion, perspective or damaged pointers;
- metric calibration or uncertainty bounds;
- real-time throughput or deployment resource usage;
- equivalence to a formal industrial measurement instrument.

Any future accuracy claim must identify the reviewed dataset, profile version,
calibration method, split policy and acceptance threshold.
