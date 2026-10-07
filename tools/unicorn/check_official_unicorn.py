import sys

if len(sys.argv) > 1:
    sys.path.insert(0, sys.argv[1])

import unicorn
from unicorn.unicorn_py3 import unicorn as unicorn_core
from unicorn.x86_const import (
    UC_X86_REG_EFLAGS,
    UC_X86_REG_R8,
    UC_X86_REG_RAX,
    UC_X86_REG_RCX,
    UC_X86_REG_RDI,
)

code = bytes.fromhex("4181f840020000f348ab7607b800000000eb05b80100000090")

print("package", unicorn.__version__, unicorn.__file__)
print("engine", unicorn.uc_version())
print("library", unicorn_core.uclib._name)
for write_hook in (False, True):
    for code_hook in (False, True):
        uc = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_64)
        uc.mem_map(0x1000, 0x1000)
        uc.mem_map(0x3000, 0x1000)
        uc.mem_write(0x1000, code)
        uc.reg_write(UC_X86_REG_R8, 0x101)
        uc.reg_write(UC_X86_REG_RCX, 8)
        uc.reg_write(UC_X86_REG_RDI, 0x3000)
        if write_hook:
            uc.hook_add(unicorn.UC_HOOK_MEM_WRITE, lambda *_: None)
        if code_hook:
            uc.hook_add(
                unicorn.UC_HOOK_CODE,
                lambda engine, *_: engine.reg_read(UC_X86_REG_EFLAGS),
            )
        uc.emu_start(0x1000, 0x1000 + len(code))
        print(
            f"write_hook={write_hook} code_hook={code_hook} "
            f"rax={uc.reg_read(UC_X86_REG_RAX)} "
            f"eflags={uc.reg_read(UC_X86_REG_EFLAGS):#x}"
        )
