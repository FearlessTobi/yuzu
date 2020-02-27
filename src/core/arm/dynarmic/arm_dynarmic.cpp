// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include <memory>
#include <dynarmic/A32/a32.h>
#include <dynarmic/A32/config.h>
#include <dynarmic/A32/context.h>
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/swap.h"
#include "core/arm/dynarmic/arm_dynarmic.h"
#include "core/arm/dynarmic/arm_dynarmic_cp15.h"
#include "core/core.h"
#include "core/core_manager.h"
#include "core/core_timing.h"
#include "core/core_timing_util.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hardware_properties.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/scheduler.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/vm_manager.h"
#include "core/memory.h"

namespace Core {

class ARM_Dynarmic_Callbacks : public Dynarmic::A32::UserCallbacks {
public:
    explicit ARM_Dynarmic_Callbacks(ARM_Dynarmic& parent) : parent(parent) {}

    u8 MemoryRead8(u32 vaddr) override {
        return parent.system.Memory().Read8(vaddr);
    }
    u16 MemoryRead16(u32 vaddr) override {
        return parent.system.Memory().Read16(vaddr);
    }
    u32 MemoryRead32(u32 vaddr) override {
        return parent.system.Memory().Read32(vaddr);
    }
    u64 MemoryRead64(u32 vaddr) override {
        return parent.system.Memory().Read64(vaddr);
    }

    void MemoryWrite8(u32 vaddr, u8 value) override {
        parent.system.Memory().Write8(vaddr, value);
    }
    void MemoryWrite16(u32 vaddr, u16 value) override {
        parent.system.Memory().Write16(vaddr, value);
    }
    void MemoryWrite32(u32 vaddr, u32 value) override {
        parent.system.Memory().Write32(vaddr, value);
    }
    void MemoryWrite64(u32 vaddr, u64 value) override {
        parent.system.Memory().Write64(vaddr, value);
    }

    void InterpreterFallback(u32 pc, std::size_t num_instructions) override {
        printf("Unicorn fallback @ 0x%08X for %d instructions (instr = {%08X})", pc,
               num_instructions, MemoryReadCode(pc));

        // ARM_Interface::ThreadContext ctx;
        // parent.SaveContext(ctx);
        // parent.inner_unicorn.LoadContext(ctx);
        // parent.inner_unicorn.ExecuteInstructions(num_instructions);
        // parent.inner_unicorn.SaveContext(ctx);
        // parent.LoadContext(ctx);
        // num_interpreted_instructions += num_instructions;

        ASSERT(false);
    }

    void ExceptionRaised(u32 pc, Dynarmic::A32::Exception exception) override {
        switch (exception) {
        case Dynarmic::A32::Exception::UndefinedInstruction:
        case Dynarmic::A32::Exception::UnpredictableInstruction:
            break;
        case Dynarmic::A32::Exception::Breakpoint:
            break;
        }
        LOG_CRITICAL(HW_GPU, "ExceptionRaised(exception = {}, pc = {:08X}, code = {:08X})",
                     static_cast<std::size_t>(exception), pc, MemoryReadCode(pc));

        std::exit(0);
    }

    void CallSVC(u32 swi) override {
        Kernel::CallSVC(parent.system, swi);
    }

    void AddTicks(u64 ticks) override {
        // Divide the number of ticks by the amount of CPU cores. TODO(Subv): This yields only a
        // rough approximation of the amount of executed ticks in the system, it may be thrown off
        // if not all cores are doing a similar amount of work. Instead of doing this, we should
        // device a way so that timing is consistent across all cores without increasing the ticks 4
        // times.
        u64 amortized_ticks = (ticks - num_interpreted_instructions) / Core::NUM_CPU_CORES;
        // Always execute at least one tick.
        amortized_ticks = std::max<u64>(amortized_ticks, 1);

        parent.system.CoreTiming().AddTicks(amortized_ticks);
        num_interpreted_instructions = 0;
    }
    u64 GetTicksRemaining() override {
        return std::max(parent.system.CoreTiming().GetDowncount(), s64{0});
    }

    ARM_Dynarmic& parent;
    std::size_t num_interpreted_instructions = 0;
    u64 tpidrro_el0 = 0;
    u64 tpidr_el0 = 0;
};

std::unique_ptr<Dynarmic::A32::Jit> ARM_Dynarmic::MakeJit(Common::PageTable& page_table,
                                                          std::size_t address_space_bits) const {
    Dynarmic::A32::UserConfig config;
    config.callbacks = cb.get();
    // config.page_table = &page_table.pointers;
    config.coprocessors[15] = std::make_shared<DynarmicCP15>((u32*)&CP15_regs[0]);
    config.define_unpredictable_behaviour = true;
    return std::make_unique<Dynarmic::A32::Jit>(config);
}

MICROPROFILE_DEFINE(ARM_Jit_Dynarmic, "ARM JIT", "Dynarmic", MP_RGB(255, 64, 64));

void ARM_Dynarmic::Run() {
    MICROPROFILE_SCOPE(ARM_Jit_Dynarmic);

    // u64 pc = GetPC();
    // printf("%08X", Common::swap32(Core::System::GetInstance().Memory().Read32(pc)));
    // pc += 4;
    // printf("%08X", Common::swap32(Core::System::GetInstance().Memory().Read32(pc)));
    // pc += 4;
    // printf("%08X", Common::swap32(Core::System::GetInstance().Memory().Read32(pc)));
    // pc += 4;
    // printf("%08X", Common::swap32(Core::System::GetInstance().Memory().Read32(pc)));
    // pc += 4;
    // printf("%08X", Common::swap32(Core::System::GetInstance().Memory().Read32(pc)));
    // pc += 4;
    // printf("%08X", Common::swap32(Core::System::GetInstance().Memory().Read32(pc)));
    // pc += 4;
    // printf("%08X", Common::swap32(Core::System::GetInstance().Memory().Read32(pc)));
    // pc += 4;
    // printf("%08X", Common::swap32(Core::System::GetInstance().Memory().Read32(pc)));
    // pc += 4;
    // printf("%08X", Common::swap32(Core::System::GetInstance().Memory().Read32(pc)));
    // pc += 4;
    // printf("%08X", Common::swap32(Core::System::GetInstance().Memory().Read32(pc)));
    // pc += 4;
    // printf("%08X", Common::swap32(Core::System::GetInstance().Memory().Read32(pc)));
    // pc += 4;
    // printf("%08X", Common::swap32(Core::System::GetInstance().Memory().Read32(pc)));
    // pc += 4;
    // printf("%08X", Common::swap32(Core::System::GetInstance().Memory().Read32(pc)));
    // pc += 4;
    // printf("%08X", Common::swap32(Core::System::GetInstance().Memory().Read32(pc)));
    // pc += 4;
    // printf("%08X", Common::swap32(Core::System::GetInstance().Memory().Read32(pc)));

    jit->Run();
}

void ARM_Dynarmic::Step() {
    cb->InterpreterFallback(jit->Regs()[15], 1);
}

ARM_Dynarmic::ARM_Dynarmic(System& system, ExclusiveMonitor& exclusive_monitor,
                           std::size_t core_index)
    : ARM_Interface{system},
      cb(std::make_unique<ARM_Dynarmic_Callbacks>(*this)), core_index{core_index},
      exclusive_monitor{dynamic_cast<DynarmicExclusiveMonitor&>(exclusive_monitor)} {}

ARM_Dynarmic::~ARM_Dynarmic() = default;

void ARM_Dynarmic::SetPC(u64 pc) {
    jit->Regs()[15] = (u32)pc;
}

u64 ARM_Dynarmic::GetPC() const {
    return jit->Regs()[15];
}

u64 ARM_Dynarmic::GetReg(int index) const {
    return jit->Regs()[index];
}

void ARM_Dynarmic::SetReg(int index, u64 value) {
    jit->Regs()[index] = (u32)value;
}

u128 ARM_Dynarmic::GetVectorReg(int index) const {
    return {};
    // jit->GetVector(index);
}

void ARM_Dynarmic::SetVectorReg(int index, u128 value) {
    // jit->SetVector(index, value);
}

u32 ARM_Dynarmic::GetPSTATE() const {
    return jit->Cpsr();
}

void ARM_Dynarmic::SetPSTATE(u32 cpsr) {
    jit->SetCpsr(cpsr);
}

u64 ARM_Dynarmic::GetTlsAddress() const {
    return CP15_regs[CP15_THREAD_URO];
}

void ARM_Dynarmic::SetTlsAddress(VAddr address) {
    CP15_regs[CP15_THREAD_URO] = (u32)address;
}

u64 ARM_Dynarmic::GetTPIDR_EL0() const {
    return cb->tpidr_el0;
}

void ARM_Dynarmic::SetTPIDR_EL0(u64 value) {
    cb->tpidr_el0 = value;
}

void ARM_Dynarmic::SaveContext(ThreadContext& ctx) {
    Dynarmic::A32::Context context;

    jit->SaveContext(context);

    ctx.cpu_registers = context.Regs();
    ctx.ext_regs = context.ExtRegs();
    ctx.cpsr = context.Cpsr();
}

void ARM_Dynarmic::LoadContext(const ThreadContext& ctx) {
    Dynarmic::A32::Context context;
    context.Regs() = ctx.cpu_registers;
    context.ExtRegs() = ctx.ext_regs;
    context.SetCpsr(ctx.cpsr);

    jit->LoadContext(context);
}

void ARM_Dynarmic::PrepareReschedule() {
    jit->HaltExecution();
}

void ARM_Dynarmic::ClearInstructionCache() {
    jit->ClearCache();
}

void ARM_Dynarmic::ClearExclusiveState() {
    // ASSERT(false);
}

void ARM_Dynarmic::PageTableChanged(Common::PageTable& page_table,
                                    std::size_t new_address_space_size_in_bits) {
    jit = MakeJit(page_table, new_address_space_size_in_bits);
}

DynarmicExclusiveMonitor::DynarmicExclusiveMonitor(Memory::Memory& memory_, std::size_t core_count)
    : monitor(core_count), memory{memory_} {}

DynarmicExclusiveMonitor::~DynarmicExclusiveMonitor() = default;

void DynarmicExclusiveMonitor::SetExclusive(std::size_t core_index, VAddr addr) {
    // Size doesn't actually matter.
    monitor.Mark(core_index, addr, 16);
}

void DynarmicExclusiveMonitor::ClearExclusive() {
    monitor.Clear();
}

bool DynarmicExclusiveMonitor::ExclusiveWrite8(std::size_t core_index, VAddr vaddr, u8 value) {
    return monitor.DoExclusiveOperation(core_index, vaddr, 1, [&] { memory.Write8(vaddr, value); });
}

bool DynarmicExclusiveMonitor::ExclusiveWrite16(std::size_t core_index, VAddr vaddr, u16 value) {
    return monitor.DoExclusiveOperation(core_index, vaddr, 2,
                                        [&] { memory.Write16(vaddr, value); });
}

bool DynarmicExclusiveMonitor::ExclusiveWrite32(std::size_t core_index, VAddr vaddr, u32 value) {
    return monitor.DoExclusiveOperation(core_index, vaddr, 4,
                                        [&] { memory.Write32(vaddr, value); });
}

bool DynarmicExclusiveMonitor::ExclusiveWrite64(std::size_t core_index, VAddr vaddr, u64 value) {
    return monitor.DoExclusiveOperation(core_index, vaddr, 8,
                                        [&] { memory.Write64(vaddr, value); });
}

bool DynarmicExclusiveMonitor::ExclusiveWrite128(std::size_t core_index, VAddr vaddr, u128 value) {
    return monitor.DoExclusiveOperation(core_index, vaddr, 16, [&] {
        memory.Write64(vaddr + 0, value[0]);
        memory.Write64(vaddr + 8, value[1]);
    });
}

} // namespace Core
