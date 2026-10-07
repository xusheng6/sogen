#pragma once
#include "arch_emulator.hpp"

namespace sogen
{

    namespace cpu_context
    {
        constexpr uint32_t effective_64bit_flags(const uint32_t flags)
        {
            if ((flags & CONTEXT_AMD64_MAIN) == 0 && (flags & CONTEXT_X86_MAIN) != 0)
            {
                return (flags & ~CONTEXT_X86_MAIN) | CONTEXT_AMD64_MAIN;
            }

            return flags;
        }

        void save(x86_64_cpu& emu, CONTEXT64& context);
        void restore(x86_64_cpu& emu, const CONTEXT64& context);
    }

} // namespace sogen
