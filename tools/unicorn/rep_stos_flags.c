#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <unicorn/unicorn.h>
#include <unicorn/x86.h>

static void on_write(uc_engine *uc, uc_mem_type type, uint64_t address,
                     int size, int64_t value, void *user_data)
{
    (void)uc;
    (void)type;
    (void)address;
    (void)size;
    (void)value;
    (void)user_data;
}

static void on_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data)
{
    uint64_t flags;
    (void)address;
    (void)size;
    (void)user_data;
    uc_reg_read(uc, UC_X86_REG_EFLAGS, &flags);
}

static int run(bool hook, bool skip_sync, bool code_hook)
{
    // cmp r8d,0x240; rep stosq; jbe taken; mov eax,0; jmp done;
    // taken: mov eax,1; done: nop. Expected result is always 1.
    const uint8_t code[] = {
        0x41,0x81,0xf8,0x40,0x02,0x00,0x00,
        0xf3,0x48,0xab,
        0x76,0x07,
        0xb8,0x00,0x00,0x00,0x00,
        0xeb,0x05,
        0xb8,0x01,0x00,0x00,0x00,
        0x90
    };
    uc_engine *uc = NULL;
    uc_hook hh;
    uint64_t r8 = 0x101, rcx = 8, rdi = 0x3000, rax = 0;
    uint64_t flags = 0, rip = 0;
    uc_err err = uc_open(UC_ARCH_X86, UC_MODE_64, &uc);
    if (err != UC_ERR_OK) goto fail;
    if ((err = uc_mem_map(uc, 0x1000, 0x1000, UC_PROT_ALL)) != UC_ERR_OK) goto fail;
    if ((err = uc_mem_map(uc, 0x3000, 0x1000, UC_PROT_ALL)) != UC_ERR_OK) goto fail;
    if ((err = uc_mem_write(uc, 0x1000, code, sizeof(code))) != UC_ERR_OK) goto fail;
    if ((err = uc_reg_write(uc, UC_X86_REG_R8, &r8)) != UC_ERR_OK) goto fail;
    if ((err = uc_reg_write(uc, UC_X86_REG_RCX, &rcx)) != UC_ERR_OK) goto fail;
    if ((err = uc_reg_write(uc, UC_X86_REG_RDI, &rdi)) != UC_ERR_OK) goto fail;
    if ((err = uc_reg_write(uc, UC_X86_REG_RAX, &rax)) != UC_ERR_OK) goto fail;
    if (hook) {
        if ((err = uc_hook_add(uc, &hh, UC_HOOK_MEM_WRITE, on_write, NULL, 1, 0)) != UC_ERR_OK) goto fail;
#ifdef UNICORN_HAS_SKIP_PC_SYNC
        if (skip_sync && (err = uc_hook_set_skip_pc_sync(uc, hh, true)) != UC_ERR_OK) goto fail;
#else
        (void)skip_sync;
#endif
    }
    if (code_hook && (err = uc_hook_add(uc, &hh, UC_HOOK_CODE, on_code, NULL, 1, 0)) != UC_ERR_OK) goto fail;
    err = uc_emu_start(uc, 0x1000, 0x1000 + sizeof(code), 0, 0);
    if (err != UC_ERR_OK) goto fail;
    uc_reg_read(uc, UC_X86_REG_RAX, &rax);
    uc_reg_read(uc, UC_X86_REG_EFLAGS, &flags);
    uc_reg_read(uc, UC_X86_REG_RIP, &rip);
    printf("hook=%d skip_sync=%d code_hook=%d result=%" PRIu64 " flags=%#" PRIx64 " rip=%#" PRIx64 "\n", hook, skip_sync, code_hook, rax, flags, rip);
    uc_close(uc);
    return rax == 1 ? 0 : 1;
fail:
    fprintf(stderr, "Unicorn error: %s\n", uc_strerror(err));
    if (uc) uc_close(uc);
    return 2;
}

int main(void)
{
    int result = 0;
    result |= run(false, false, false);
    result |= run(true, false, false);
    result |= run(false, false, true);
    result |= run(true, false, true);
#ifdef UNICORN_HAS_SKIP_PC_SYNC
    result |= run(true, true, false);
    result |= run(true, true, true);
#endif
    return result;
}
