# Write-hook flag divergence in the Flare-On 9 challenge 11 run

The ordinary Unicorn memory-write hook changes this sample's execution. The custom metadata-only hook avoids that change. This note records the first observed control-flow divergence, rather than inferring its cause from the later PyArmor error.

## Controlled comparison

Both runs restored the same checkpoint at Sogen TTD instruction 500,000,000 from `challenge11-complete-write.sogttd`. Both installed an identical no-op metadata write callback and an instruction-address hash callback. The sole switch was whether Unicorn's write-hook path called `cpu_restore_state` before the callback. Instruction-sequence hashes matched through step **529,748,246**. At step **529,748,247**, the instruction hash still matched, but the next RIP differed:

| Hook mode | State at step 529,748,246 | Next RIP |
| --- | --- | --- |
| Skip state update | `R8=0x101`, `RFLAGS=0x83`, `RIP=0x6d621070` | `0x6d621220` |
| Ordinary Unicorn update | `R8=0x101`, `RFLAGS=0x82`, `RIP=0x6d621070` | `0x6d621076` |

The sample's code is:

```asm
0x6d621066  cmp       r8d, 0x240
0x6d62106d  rep stosq
0x6d621070  jbe       0x6d621220
```

`0x101 < 0x240`, so the comparison sets carry. `rep stosq` should preserve the flags, and `jbe` should take the branch. The skip-update run does. In the ordinary-hook run, carry has been cleared, so the branch falls through incorrectly. This is the first observed control-flow difference; the later `Invalid input packet` at 539,063,166 instructions is downstream.

## Internal cause

Unicorn's write-hook path calls `cpu_restore_state` in `qemu/accel/tcg/cputlb.c`. For x86, `restore_state_to_opc` in `qemu/target/i386/translate.c` assigns both the guest instruction pointer and `env->cc_op`, the mode used to interpret lazy condition flags. Temporary diagnostics at the `rep stosq` write showed its first such update changing `cc_op` from **1 (`CC_OP_EFLAGS`)** to **16 (`CC_OP_SUBL`)**. The accompanying `cc_src` was `0x81` and `eflags` was `0x83`. Subsequent writes showed `cc_src=0x80`; at the branch, `RFLAGS=0x82` instead of `0x83`.

Thus the state update reinterprets already-materialized flag data as subtraction operands. The callback itself is empty. The extra state update changes a guest-visible carry flag and causes the incorrect branch. The metadata-only hook does not need guest register state, so skipping that update avoids the corruption in this run.

This comparison establishes the mechanism in Sogen's Unicorn fork and this translated code path. A separately installed official Unicorn 2.1.2 did not reproduce it. The diagnostic edits used for this comparison were temporary and are not part of the branch.

That minimized reproduction is now in [`tools/unicorn/rep_stos_flags.c`](../tools/unicorn/rep_stos_flags.c). It requires both a no-op write hook and a code hook that reads EFLAGS to trigger the same wrong branch in the fork.

The matching [Python reproduction](../tools/unicorn/check_official_unicorn.py) also fails on the official Unicorn 2.1.4 wheel and passes on the official 2.1.2 wheel. The regression is therefore present in the latest published upstream release, not limited to the Sogen fork.
