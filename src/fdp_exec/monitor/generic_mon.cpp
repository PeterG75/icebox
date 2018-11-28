#include "generic_mon.hpp"

#define FDP_MODULE "generic_monitor"
#include "log.hpp"
#include "monitor_helpers.hpp"
#include "nt/nt.hpp"
#include "os.hpp"
#include "core/helpers.hpp"

#include <unordered_map>

namespace
{
    static const std::string dll = "ntdll";

    struct OnFunction
    {
        char name[64];
        void (monitor::GenericMonitor::*on_function)();
    };

    static const OnFunction functions[] =
    {
        DECLARE_SYSCALLS_HANDLERS
    };
}

struct monitor::GenericMonitor::Data
{
    std::vector<core::Breakpoint> bps;

    DECLARE_SYSCALLS_OBSERVERS
};

monitor::GenericMonitor::GenericMonitor(core::Core& core)
    : d_(std::make_unique<Data>())
    , core_(core)
{
}

monitor::GenericMonitor::~GenericMonitor()
{
}

bool monitor::GenericMonitor::setup_all(proc_t proc, on_function_generic_fn& on_function_generic)
{
    //TODO Load pdb to be sure that it is loaded ?

    for(const auto& f : functions)
    {
        const auto function_addr = core_.sym.symbol(dll, f.name);
        if (!function_addr)
        {
            // FAIL(false, "Unable to find symbol %s", s.name);
            LOG(ERROR, "Unable to find symbol %s", f.name);
            continue;
        }

        const auto b = core_.state.set_breakpoint(*function_addr, proc, core::FILTER_CR3, on_function_generic);

        d_->bps.push_back(b);
    }

    return true;
}

bool monitor::GenericMonitor::setup_func(proc_t proc, std::string fname)
{
    for (const auto& f : functions){
        if (fname.compare(f.name) != 0)
            continue;

        const auto function_addr = core_.sym.symbol(dll, f.name);
        if (!function_addr)
            FAIL(false, "Unable to find symbol %s", f.name);

        const auto b = core_.state.set_breakpoint(*function_addr, proc, core::FILTER_CR3, [&]()
        {
            ((this)->*(f.on_function))();
        });

        d_->bps.push_back(b);
        return true;
    }
    return false;
}

bool monitor::GenericMonitor::get_raw_args(size_t nargs, const on_arg_fn& on_arg)
{
    for (size_t j=0; j < nargs; j++)
    {
        const auto arg = monitor::get_arg_by_index(core_, j);
        if (on_arg(*arg) == WALK_STOP)
            break;
    }

    return true;
}

#include "syscall_mon_private.gen.hpp"