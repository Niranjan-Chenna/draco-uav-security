# Approved no-op current-policy revalidation

The initial review found that `NO_OP_REUPLOAD` returned `ALLOW` before the
current contract's command and geometry checks. A network-free diagnostic
reproduced an exact 50 m mission re-upload being allowed under a 40 m envelope.

The owner explicitly approved moving the no-op success return after existing
intent checks. That change is applied in `mission_authorization.cpp` and covered
by `tests/no_op_revalidation_test.cpp` and runtime freshness regressions.
No-op causality, delta, budgets, hard causality failures, authority tiers,
emergency policy, and PX4 ACK/commit behavior remain intact. Existing committed
PX4 missions are not automatically changed when policy changes.

The temporary diagnostic and unapplied patch were retired after the regression
tests passed. Initial logs remain in ignored `evaluation/results/raw/initial/`.
See [the engineering report](../ENGINEERING_REPORT.md) for the final results.

## PX4 accepted ACK / local commit consistency

A final review found that an unexpected local revision commit failure after a
PX4 `MAV_MISSION_ACCEPTED` result could still report success to the GCS. The ACK
reconciliation now preserves the normal successful path, but on local failure
it clears any proposed revision, leaves the prior committed revision unchanged,
latches PX4 state uncertainty, sends `MAV_MISSION_ERROR` to the GCS, and emits
`LOCAL_COMMIT_FAILED_AFTER_PX4_ACCEPT`. The focused regression also verifies the
successful ACK path remains unchanged.
