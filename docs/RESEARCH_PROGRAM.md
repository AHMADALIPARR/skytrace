# SKYTRACE program specification v0.1

## Research questions

- What fraction of cases can be resolved when independent aviation, orbital, meteorological and astronomical data are synchronized?
- Which unexplained cases retain calibrated distance, direction and angular velocity with quantified uncertainties?
- Which cases have multiple demonstrably independent synchronized sensors?
- Which reported accelerations survive camera ego-motion, parallax and rolling-shutter corrections?
- Do known-source negative controls produce false anomaly detections?

## Proposed 7 processing gates

G0 Source legitimacy: archival provenance, access and license.
G1 Media integrity: checksum, format, frame continuity, metadata.
G2 Spatiotemporal normalization: UTC, uncertainty windows, WGS84, clock offset.
G3 Known-object matching: aviation, satellites, meteors, balloons, weather, celestial bodies.
G4 Sensor modeling: PSF, focal length, pointing, camera motion, rolling shutter and parallax.
G5 Hypothesis comparison: evidence likelihoods, uncertainty propagation, sensitivity analysis.
G6 Human review and reproducibility: independent analyst and machine-readable verdict.

## Planned tools by layer

- Evidence/data: SHA-256, JSON Schema, JSONL, Parquet, DuckDB, SQLite, Apache Arrow, Git, Git LFS, WORM snapshots.
- Documents: PyMuPDF or pdftotext, Tesseract only as a fallback, OCR validation, citation graph, de-duplication, local embeddings.
- Video: FFmpeg, ffprobe, OpenCV, optical flow, feature matching, stabilized trajectory fits.
- Sky/geometry: Astropy, Skyfield, SPICE, SGP4, WGS84 geodesy, coordinate transforms, ephemerides.
- Aviation: OpenSky, aircraft state vectors, flight tracking archives where accessible, restricted-area NOTAM context.
- Atmospheric: NOAA, ERA5, radiosonde, wind fields, cloud and lightning observations.
- Radio/sensors: GNU Radio, RTL-SDR, time-synchronized GPSDO, calibrated optical and thermal sensors where legally available.
- Physics: SciPy, Julia, Bayesian model comparison, Monte Carlo uncertainty propagation, particle filtering.
- Formal: Lean 4, TLA+, Z3, refinement types, cross-implementation differential tests.
- Interfaces: offline static browser, geospatial event timelines, uncertainty visualizations, source provenance DAG.

## Evidence grading

A — calibrated, original, synchronized independent sensors and documented geometry.
B — original instrument/media data with incomplete calibration or one independent sensor.
C — credible documents or witness narratives lacking sufficient instrumental reconstruction.
D — reposts, unsourced media, missing provenance, or conflicting metadata.

Grades reflect measurement quality rather than the probability of an extraterrestrial explanation.

## First case studies

1. Reproduce official AARO explanations using public media, checking frame transformations and parallax.
2. Select an unresolved official IR case with enough metadata for measured angular motion and uncertainty bounds.
3. GEIPAN open cases vs confirmed ordinary cases, as blinded positive/negative controls.
4. Cross-check a recent public report against contemporaneous flights, satellites, meteors and weather.

## Research publication standard

Publish observation, source hash, methods, excluded hypotheses, priors, uncertainties, sensitivity analysis, and alternate hypotheses. Publish an explicit 'insufficient data' result where geometry, calibration or timestamps prevent reliable conclusions.
