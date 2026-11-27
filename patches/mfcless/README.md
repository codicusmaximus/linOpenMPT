# mfcless patches

Patches for the mfcless build (`-DLINOPENMPT_UI=qt`) only. They are applied
to a git worktree of `upstream/` in the build folder
(`<build>/upstream-mfcless/`), after `patches/*.patch`, which that worktree
has committed. `upstream/` itself and the MFC build never see them.

To change one: edit the files in `<build>/upstream-mfcless/`, then save
`git -C <build>/upstream-mfcless diff -- <files>` over the patch. CMake
re-applies everything when a patch or the upstream release changes.

| Patch | Why |
|-------|-----|
| `0001-engine-no-mfc-types.patch` | In the tracker configuration, upstream's engine (`soundlib/`, `sounddsp/`) uses MFC's `CString` for plugin parameter, program and macro names, and Win32's `DWORD` and `UINT`. These become `mpt::ustring` and `uint32`; `CString::Format` calls become `MPT_UFORMAT` and `mpt::ufmt`, `_T("...")` becomes `U_("...")`. Code inside `#if MPT_OS_WINDOWS` is left alone. Mostly mechanical; when it stops applying, redo it with the same substitutions. |
