# Patches to upstream

The goal is to keep this folder empty. Port code lives outside `upstream/`, and
upstream files are only changed when there is no other way.

If a patch is unavoidable:

1. Make the change inside `upstream/`, then save it with
   `git -C upstream diff > patches/NNNN-short-name.patch`.
2. Add a line to the table below explaining why it is needed.
3. Prefer changes that upstream could accept (for example, a missing
   `#if MPT_OS_WINDOWS` guard), and consider sending them upstream so the patch
   can be dropped.

`scripts/update-upstream.sh` re-applies every patch here after a sync and stops
if one no longer applies.

| Patch | Why |
|-------|-----|
| (none yet) | |
