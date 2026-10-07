# x86-64 flags change when code and write hooks are combined

`rep_stos_flags.c` is a standalone Unicorn C API reproducer. It runs the same
25-byte program four times. Each run starts with `R8=0x101`, `RCX=8`, and a
mapped `RDI` destination. The guest executes:

```asm
cmp r8d, 0x240
rep stosq
jbe taken
mov eax, 0
jmp done
taken: mov eax, 1
done: nop
```

The comparison sets carry, and `rep stosq` preserves flags. Therefore every
run should finish with `RAX=1` and `EFLAGS=0x83`. Both callbacks are passive:
the write hook does nothing, while the code hook reads `EFLAGS` before each
instruction. The program exits with status 1 if any result is wrong.

## Observed results

Built against `momo5502/unicorn` commit `d23810735f792db1977327f3609fa3fa0446ede1`
plus Sogen's local `15940c97e524f1f8a9767315a06f5a157f9371ea` patch, with
that patch's skip option **disabled**:

| Write hook | Code hook reads EFLAGS | RAX | EFLAGS |
| --- | --- | ---: | ---: |
| No | No | 1 | `0x83` |
| Yes | No | 1 | `0x83` |
| No | Yes | 1 | `0x83` |
| Yes | Yes | **0** | **`0x82`** |

Enabling the local patch's skip option in the last row gives `RAX=1` and
`EFLAGS=0x83`. The source compiles without that extension by default; define
`UNICORN_HAS_SKIP_PC_SYNC` only to show the workaround cases.

`check_official_unicorn.py` runs the same four cases through the official
Python wheels, independently of the Sogen fork. On Windows x86-64, official
Unicorn **2.1.4** reproduces the last row: `RAX=0`, `EFLAGS=0x82`. Official
Unicorn **2.1.2** and **2.1.3** produce the correct `RAX=1`, `EFLAGS=0x83`
for all four cases. The latest published official release is affected.

An upstream source comparison identifies the first failing commit:

| Upstream source commit | Both hooks: RAX | Both hooks: EFLAGS |
| --- | ---: | ---: |
| [`3a7bde03`](https://github.com/unicorn-engine/unicorn/commit/3a7bde03b843ed8e6f6adc3478096bae3547f5a4), parent | 1 | `0x83` |
| [`4a13bc7c`](https://github.com/unicorn-engine/unicorn/commit/4a13bc7cb8c79250fb6151aabb0691a82123f4f4) | **0** | **`0x82`** |

Both source revisions were built on Windows x86-64 with CMake Release,
`UNICORN_ARCH=x86`, and the same Visual Studio toolchain. The Python test
used the same 2.1.3 bindings for both runs and loaded each built `unicorn.dll`
through `LIBUNICORN_PATH`; the loaded DLL path was verified. Commit
`4a13bc7c` moves memory-hook PC synchronization from x86 translation code to
`cpu_restore_state` in `cputlb.c`. That new state restoration also changes
`cc_op` during `rep stosq`, causing the observed carry loss.

To reproduce with an isolated wheel installation:

```sh
python -m pip install --no-deps --target ./unicorn-2.1.4 unicorn==2.1.4
python check_official_unicorn.py ./unicorn-2.1.4
```

## Build

Build this single file as a C program and link it against Unicorn. For the
existing Windows Sogen build, from `sogen-publish` in a Visual Studio
developer command prompt:

```bat
cl /nologo /MD /TC /I ..\sogen-ttd\deps\unicorn\include tools\unicorn\rep_stos_flags.c /link /LTCG /OUT:rep_stos_flags.exe ..\sogen-ttd\build\msvc-release\artifacts\unicorn.lib ..\sogen-ttd\build\msvc-release\artifacts\x86_64-softmmu.lib ..\sogen-ttd\build\msvc-release\artifacts\unicorn-common.lib ws2_32.lib
```

For a system installation that provides `pkg-config`:

```sh
cc -std=c11 -Wall -Wextra rep_stos_flags.c $(pkg-config --cflags --libs unicorn) -o rep_stos_flags
./rep_stos_flags
```

## Upstream context

[Unicorn issue #1717](https://github.com/unicorn-engine/unicorn/issues/1717)
reports a similar memory-hook-induced x86 flag/control-flow error involving
`shl`, with a different proposed internal mechanism. It is closed. [Issue
#2041](https://github.com/unicorn-engine/unicorn/issues/2041) reports that
adding memory hooks changes execution, but concerns self-modifying code and
is still open. Neither is confirmed to be this exact `rep stosq`/lazy-flags
failure. The official 2.1.4 result makes this suitable for an upstream bug
report using the standalone reproducer.

In the Sogen case, the ordinary write-hook path calls `cpu_restore_state`.
At the first `rep stosq` write, `restore_state_to_opc` changes `cc_op` from
`CC_OP_EFLAGS` to `CC_OP_SUBL` after flags have been materialized. The carry
bit is then lost before `jbe`. See
[`../../docs/unicorn-write-hook-divergence.md`](../../docs/unicorn-write-hook-divergence.md)
for the full-sample comparison.
