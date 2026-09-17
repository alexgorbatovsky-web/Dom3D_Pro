# Shell input

Starting SolidShell always requests a fresh surface pick, then opens the
distance editor. The initial distance is 2. OK stores the signed distance in
QSettings under `tools/SolidShell/lastAcceptedDistance`; Cancel does not change
it. The saved value is reused across application sessions. Existing Shell
objects retain their own parameters.

The ExtractFaceUI interaction regression also checks Shell picking, initial
distance, accepted-value reuse and cancellation without overwriting it.
