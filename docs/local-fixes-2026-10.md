# Local emulator and TTD fixes

This branch includes the Windows emulation and TTD changes developed during the miner and Flare-On challenge 11 investigations. The changes cover mapped-image sharing, cross-thread contexts and hardware breakpoints, pagefile section view lifetime, stdout file information, relative time, and replay progress.

The TTD metadata-only write hook depends on a local Unicorn change that has not been merged into `momo5502/unicorn`. Its patch is stored at `patches/unicorn/15940c9-metadata-write-hooks.patch`. The exact flag and branch divergence that motivated it is documented in [unicorn-write-hook-divergence.md](unicorn-write-hook-divergence.md). After pulling this branch and initializing submodules, apply it before building:

```powershell
git submodule update --init --recursive
git -C deps/unicorn am ../../patches/unicorn/15940c9-metadata-write-hooks.patch
cmake --build --preset=release
```

The patch recreates the changes from local Unicorn commit `15940c9` on the submodule's pinned `d238107` base. The submodule will appear modified in Sogen's status because this branch retains the upstream gitlink. This makes the change transferable through the Sogen remote without referencing an unreachable submodule commit. Do not run `git submodule update --force` after applying it unless you intend to reapply the patch.

The native miner trace reached `_CorExeMain` after these emulator fixes; the later `0x80131700` status remains a managed-runtime boundary. The challenge 11 write-only TTD trace completed with the local Unicorn patch. Detailed local recordings are not tracked in this repository.
