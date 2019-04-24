#include "os.hpp"

#define FDP_MODULE "os_linux"
#include "core.hpp"
#include "log.hpp"
#include "reader.hpp"
#include "utils/fnview.hpp"
#include "utils/utils.hpp"

#include "map.hpp"

#include <array>

namespace
{
    enum class cat_e
    {
        REQUIRED,
        OPTIONAL,
    };

    enum offset_e
    {
        TASKSTRUCT_COMM,
        TASKSTRUCT_CRED,
        TASKSTRUCT_PID,
        TASKSTRUCT_TGID,
        TASKSTRUCT_REALPARENT,
        TASKSTRUCT_PARENT,
        TASKSTRUCT_TASKS,
        TASKSTRUCT_MM,
        TASKSTRUCT_ACTIVEMM,
        MMSTRUCT_PGD,
        CRED_UID,
        OFFSET_COUNT,
    };

    struct LinuxOffset
    {
        cat_e      e_cat;
        offset_e   e_id;
        const char module[16];
        const char struc[32];
        const char member[32];
    };
    // clang-format off
    const LinuxOffset g_offsets[] =
    {
            {cat_e::REQUIRED,	TASKSTRUCT_COMM,			"dwarf",	"task_struct",		"comm"},
            {cat_e::REQUIRED,	TASKSTRUCT_CRED,			"dwarf",	"task_struct",		"cred"},
            {cat_e::REQUIRED,	TASKSTRUCT_PID,				"dwarf",	"task_struct",		"pid"},
            {cat_e::REQUIRED,	TASKSTRUCT_TGID,			"dwarf",	"task_struct",		"tgid"},
            {cat_e::REQUIRED,	TASKSTRUCT_REALPARENT,		"dwarf",	"task_struct",		"real_parent"},
            {cat_e::REQUIRED,	TASKSTRUCT_PARENT,			"dwarf",	"task_struct",		"parent"},
            {cat_e::REQUIRED,	TASKSTRUCT_TASKS,			"dwarf",	"task_struct",		"tasks"},
            {cat_e::REQUIRED,	TASKSTRUCT_MM,				"dwarf",	"task_struct",		"mm"},
            {cat_e::REQUIRED,	TASKSTRUCT_ACTIVEMM,		"dwarf",	"task_struct",		"active_mm"},
            {cat_e::REQUIRED,	MMSTRUCT_PGD,				"dwarf",	"mm_struct",		"pgd"},
            {cat_e::REQUIRED,	CRED_UID,					"dwarf",	"cred",				"uid"},
    };
    // clang-format on
    static_assert(COUNT_OF(g_offsets) == OFFSET_COUNT, "invalid offsets");

    enum symbol_e
    {
        CURRENT_TASK,
        SYMBOL_COUNT,
    };

    struct LinuxSymbol
    {
        cat_e      e_cat;
        symbol_e   e_id;
        const char module[16];
        const char name[32];
    };
    // clang-format off
    const LinuxSymbol g_symbols[] =
    {
            {cat_e::REQUIRED,	CURRENT_TASK,				"dwarf",	"current_task"},
    };
    // clang-format on
    static_assert(COUNT_OF(g_symbols) == SYMBOL_COUNT, "invalid symbols");

    using LinuxOffsets = std::array<uint64_t, OFFSET_COUNT>;
    using LinuxSymbols = std::array<uint64_t, SYMBOL_COUNT>;

    struct OsLinux
        : public os::IModule
    {
        OsLinux(core::Core& core);

        // os::IModule
        bool            setup               () override;
        bool            is_kernel_address   (uint64_t ptr) override;
        bool            can_inject_fault    (uint64_t ptr) override;
        bool            reader_setup        (reader::Reader& reader, opt<proc_t> proc) override;
        sym::Symbols&   kernel_symbols      () override;

        bool                proc_list       (on_proc_fn on_process) override;
        opt<proc_t>         proc_current    () override;
        opt<proc_t>         proc_find       (std::string_view name, flags_e flags) override;
        opt<proc_t>         proc_find       (uint64_t pid) override;
        opt<std::string>    proc_name       (proc_t proc) override;
        bool                proc_is_valid   (proc_t proc) override;
        uint64_t            proc_id         (proc_t proc) override;
        flags_e             proc_flags      (proc_t proc) override;
        void                proc_join       (proc_t proc, os::join_e join) override;
        opt<phy_t>          proc_resolve    (proc_t proc, uint64_t ptr) override;
        opt<proc_t>         proc_select     (proc_t proc, uint64_t ptr) override;
        opt<proc_t>         proc_parent     (proc_t proc) override;

        bool            thread_list     (proc_t proc, on_thread_fn on_thread) override;
        opt<thread_t>   thread_current  () override;
        opt<proc_t>     thread_proc     (thread_t thread) override;
        opt<uint64_t>   thread_pc       (proc_t proc, thread_t thread) override;
        uint64_t        thread_id       (proc_t proc, thread_t thread) override;

        bool                mod_list(proc_t proc, on_mod_fn on_module) override;
        opt<std::string>    mod_name(proc_t proc, mod_t mod) override;
        opt<span_t>         mod_span(proc_t proc, mod_t mod) override;
        opt<mod_t>          mod_find(proc_t proc, uint64_t addr) override;

        bool                vm_area_list    (proc_t proc, on_vm_area_fn on_vm_area) override;
        opt<vm_area_t>      vm_area_find    (proc_t proc, uint64_t addr) override;
        opt<span_t>         vm_area_span    (proc_t proc, vm_area_t vm_area) override;
        vma_access_e        vm_area_access  (proc_t proc, vm_area_t vm_area) override;
        vma_type_e          vm_area_type    (proc_t proc, vm_area_t vm_area) override;
        opt<std::string>    vm_area_name    (proc_t proc, vm_area_t vm_area) override;

        bool                driver_list (on_driver_fn on_driver) override;
        opt<driver_t>       driver_find (uint64_t addr) override;
        opt<std::string>    driver_name (driver_t drv) override;
        opt<span_t>         driver_span (driver_t drv) override;

        opt<bpid_t> listen_proc_create  (const on_proc_event_fn& on_create) override;
        opt<bpid_t> listen_proc_delete  (const on_proc_event_fn& on_delete) override;
        opt<bpid_t> listen_thread_create(const on_thread_event_fn& on_create) override;
        opt<bpid_t> listen_thread_delete(const on_thread_event_fn& on_delete) override;
        opt<bpid_t> listen_mod_create   (const on_mod_event_fn& on_create) override;
        opt<bpid_t> listen_drv_create   (const on_drv_event_fn& on_drv) override;
        size_t      unlisten            (bpid_t bpid) override;

        opt<arg_t>  read_stack  (size_t index) override;
        opt<arg_t>  read_arg    (size_t index) override;
        bool        write_arg   (size_t index, arg_t arg) override;

        void debug_print() override;

        // members
        core::Core&    core_;
        sym::Symbols   syms_;
        reader::Reader reader_;
        LinuxOffsets   offsets_;
        LinuxSymbols   symbols_;
    };
}

OsLinux::OsLinux(core::Core& core)
    : core_(core)
    , reader_(reader::make(core))
{
}

bool OsLinux::setup()
{
    if(!syms_.insert("dwarf", {}, {}, {}))
        return FAIL(false, "unable to read dwarf file");

    auto system_map = sym::Map();
    if(!system_map.setup())
        return FAIL(false, "unable to read System.map file");

    bool fail = false;
    int i     = -1;
    memset(&symbols_[0], 0, sizeof symbols_);
    for(const auto& sym : g_symbols)
    {
        fail |= sym.e_id != ++i;
        const auto addr = system_map.symbol(sym.name);
        if(!addr)
        {
            fail |= sym.e_cat == cat_e::REQUIRED;
            if(sym.e_cat == cat_e::REQUIRED)
                LOG(ERROR, "unable to read {}!{} symbol offset", sym.module, sym.name);
            else
                LOG(WARNING, "unable to read optional {}!{} symbol offset", sym.module, sym.name);
            continue;
        }
        symbols_[i] = *addr;
    }

    i = -1;
    memset(&offsets_[0], 0, sizeof offsets_);
    for(const auto& off : g_offsets)
    {
        fail |= off.e_id != ++i;
        const auto offset = syms_.struc_offset(off.module, off.struc, off.member);
        if(!offset)
        {
            fail |= off.e_cat == cat_e::REQUIRED;
            if(off.e_cat == cat_e::REQUIRED)
                LOG(ERROR, "unable to read {}!{}.{} member offset", off.module, off.struc, off.member);
            else
                LOG(WARNING, "unable to read optional {}!{}.{} member offset", off.module, off.struc, off.member);
            continue;
        }
        offsets_[i] = *offset;
    }
    if(fail)
        return false;

    return true;
}

std::unique_ptr<os::IModule> os::make_linux(core::Core& core)
{
    return std::make_unique<OsLinux>(core);
}

bool OsLinux::proc_list(on_proc_fn on_process)
{
    const auto init_proc = core_.os->proc_current();
    if(!init_proc)
        return false;

    const auto head = init_proc->id + offsets_[TASKSTRUCT_TASKS];
    for(auto link = reader_.read(head); link != head; link = reader_.read(*link))
    {
        const auto task_struc = *link - offsets_[TASKSTRUCT_TASKS];
        auto pgd              = reader_.read(task_struc + offsets_[TASKSTRUCT_MM] + offsets_[MMSTRUCT_PGD]);
        if(!pgd)
        {
            pgd = reader_.read(task_struc + offsets_[TASKSTRUCT_ACTIVEMM] + offsets_[MMSTRUCT_PGD]);
            if(!pgd)
            {
                LOG(ERROR, "unable to read task_struct.mm_struct.pgd from {:#x}", task_struc);
                continue;
            }
        }

        const auto err = on_process({task_struc, {*pgd}});
        if(err == WALK_STOP)
            break;
    }
    return true;
}

opt<proc_t> OsLinux::proc_current() // proc init either ?
{
    auto kpcr = core_.regs.read(MSR_GS_BASE);
    /*if (!is_kernel(kpcr))
		kpcr_ = core_.regs.read(MSR_KERNEL_GS_BASE);
	if (!is_kernel(kpcr_))
		return FAIL(false, "unable to read KPCR");*/

    const auto addr = kpcr + symbols_[CURRENT_TASK];

    const auto proc_id = reader_.read(addr);
    return proc_t{*proc_id, reader_.kdtb_}; // proc located at init_task
}

opt<proc_t> OsLinux::proc_find(std::string_view name, flags_e /*flags*/)
{
    opt<proc_t> found;
    proc_list([&](proc_t proc)
    {
        const auto got = proc_name(proc);
        if(*got != name)
            return WALK_NEXT;

        found = proc;
        return WALK_STOP;
    });
    return found;
}

opt<proc_t> OsLinux::proc_find(uint64_t pid)
{
    opt<proc_t> found;
    proc_list([&](proc_t proc)
    {
        const auto got = proc_id(proc);
        if(got != pid)
            return WALK_NEXT;

        found = proc;
        return WALK_STOP;
    });
    return found;
}

opt<std::string> OsLinux::proc_name(proc_t proc)
{
    char buffer[20 + 1];
    const auto ok             = reader_.read(buffer, proc.id + offsets_[TASKSTRUCT_COMM], sizeof buffer);
    buffer[sizeof buffer - 1] = 0;
    if(!ok)
        return {};

    const auto name = std::string{buffer};
    if(name.size() == sizeof buffer - 1)
        LOG(ERROR, "Process name buffer is too small");

    return name;
}

uint64_t OsLinux::proc_id(proc_t proc)
{
    const auto reader = reader::make(core_);
    const auto pid    = reader.le32(proc.id + offsets_[TASKSTRUCT_PID]);
    if(!pid)
        return 0;

    return 0x0000000000000000 | *pid;
}

bool OsLinux::proc_is_valid(proc_t /*proc*/)
{
    return true;
}

bool OsLinux::is_kernel_address(uint64_t /*ptr*/)
{
    return true;
}

bool OsLinux::can_inject_fault(uint64_t /*ptr*/)
{
    return false;
}

flags_e OsLinux::proc_flags(proc_t /*proc*/)
{
    return FLAGS_NONE;
}

void OsLinux::proc_join(proc_t /*proc*/, os::join_e /*join*/)
{
}

opt<phy_t> OsLinux::proc_resolve(proc_t /*proc*/, uint64_t /*ptr*/)
{
    return {};
}

opt<proc_t> OsLinux::proc_select(proc_t proc, uint64_t /*ptr*/)
{
    return proc;
}

opt<proc_t> OsLinux::proc_parent(proc_t /*proc*/)
{
    return {};
}

bool OsLinux::reader_setup(reader::Reader& reader, opt<proc_t> proc)
{
    if(!proc)
        return true;

    reader.udtb_ = proc->dtb;
    reader.kdtb_ = proc->dtb;
    return true;
}

sym::Symbols& OsLinux::kernel_symbols()
{
    return syms_;
}

bool OsLinux::thread_list(proc_t /*proc*/, on_thread_fn on_thread)
{
    thread_t dummy_thread = {0};
    on_thread(dummy_thread);
    return true;
}

/*
 * Threads are really just processes on Linux.
 */
opt<thread_t> OsLinux::thread_current()
{
    return thread_t{proc_current()->id};
}

opt<proc_t> OsLinux::thread_proc(thread_t thread)
{
    return proc_t{thread.id, {}};
}

opt<uint64_t> OsLinux::thread_pc(proc_t /*proc*/, thread_t /*thread*/)
{
    return {};
}

uint64_t OsLinux::thread_id(proc_t /*proc*/, thread_t /*thread*/)
{
    return 0;
}

bool OsLinux::mod_list(proc_t /*proc*/, on_mod_fn on_module)
{
    mod_t dummy_mod = {0, FLAGS_NONE};
    on_module(dummy_mod);
    return true;
}

opt<std::string> OsLinux::mod_name(proc_t /*proc*/, mod_t /*mod*/)
{
    return {};
}

opt<span_t> OsLinux::mod_span(proc_t /*proc*/, mod_t /*mod*/)
{
    return {};
}

opt<mod_t> OsLinux::mod_find(proc_t /*proc*/, uint64_t /*addr*/)
{
    return {};
}

bool OsLinux::vm_area_list(proc_t /*proc*/, on_vm_area_fn /*on_vm_area*/)
{
    return false;
}

opt<vm_area_t> OsLinux::vm_area_find(proc_t /*proc*/, uint64_t /*addr*/)
{
    return {};
}

opt<span_t> OsLinux::vm_area_span(proc_t /*proc*/, vm_area_t /*vm_area*/)
{
    return {};
}

vma_access_e OsLinux::vm_area_access(proc_t /*proc*/, vm_area_t /*vm_area*/)
{
    return VMA_ACCESS_NONE;
}

vma_type_e OsLinux::vm_area_type(proc_t /*proc*/, vm_area_t /*vm_area*/)
{
    return vma_type_e::none;
}

opt<std::string> OsLinux::vm_area_name(proc_t /*proc*/, vm_area_t /*vm_area*/)
{
    return {};
}

bool OsLinux::driver_list(on_driver_fn on_driver)
{
    driver_t dummy_driver = {0};
    on_driver(dummy_driver);
    return true;
}

opt<driver_t> OsLinux::driver_find(uint64_t /*addr*/)
{
    return {};
}

opt<std::string> OsLinux::driver_name(driver_t /*drv*/)
{
    return {};
}

opt<span_t> OsLinux::driver_span(driver_t /*drv*/)
{
    return {};
}

opt<arg_t> OsLinux::read_stack(size_t /*index*/)
{
    return {};
}

opt<arg_t> OsLinux::read_arg(size_t /*index*/)
{
    return {};
}

bool OsLinux::write_arg(size_t /*index*/, arg_t /*arg*/)
{
    return false;
}

void OsLinux::debug_print()
{
}

opt<OsLinux::bpid_t> OsLinux::listen_proc_create(const on_proc_event_fn& /*on_create*/)
{
    return {};
}

opt<OsLinux::bpid_t> OsLinux::listen_proc_delete(const on_proc_event_fn& /*on_delete*/)
{
    return {};
}

opt<OsLinux::bpid_t> OsLinux::listen_thread_create(const on_thread_event_fn& /*on_create*/)
{
    return {};
}

opt<OsLinux::bpid_t> OsLinux::listen_thread_delete(const on_thread_event_fn& /*on_remove*/)
{
    return {};
}

opt<OsLinux::bpid_t> OsLinux::listen_mod_create(const on_mod_event_fn& /*on_create*/)
{
    return {};
}

opt<OsLinux::bpid_t> OsLinux::listen_drv_create(const on_drv_event_fn&)
{
    return {};
}

size_t OsLinux::unlisten(bpid_t /*bpid*/)
{
    return {};
}
