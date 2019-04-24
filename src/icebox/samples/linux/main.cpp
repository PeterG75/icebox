#define FDP_MODULE "linux"
#include <icebox/core.hpp>
#include <icebox/log.hpp>
#include <icebox/os.hpp>
#include <icebox/utils/fnview.hpp>

#include <icebox/linux/map.hpp>

int main(int argc, char** argv)
{
    logg::init(argc, argv);

    // core initialization
    if(argc != 2)
        return FAIL(-1, "usage: linux <name>");

    const auto name = std::string{argv[1]};
    LOG(INFO, "starting on {}", name.data());

    core::Core core;
    const auto ok = core.setup(name);
    if(!ok)
        return FAIL(-1, "unable to start core at {}", name.data());

    // get list of processes
    core.state.pause();
    core.os->proc_list([&](proc_t proc)
    {
        const auto proc_pid  = core.os->proc_id(proc);
        const auto proc_name = core.os->proc_name(proc);
        LOG(INFO, "proc: {:#x} pid:{} '{}'", proc.id, proc_pid ? proc_pid : 0, proc_name ? proc_name->data() : "<noname>");
        return WALK_NEXT;
    });
    core.state.resume();

    // get the current process pressing a key
    /*LOG(INFO, "");
    while(true)
    {
        core.state.pause();
        const auto proc = core.os->proc_current();
        if(proc)
        {
            const auto proc_pid = core.os->proc_id(*proc);
            LOG(INFO, "current proc: {:#x} pid:{}", (*proc).id, proc_pid ? proc_pid : 0);
        }
        core.state.resume();
        system("pause");
    }*/

    return 0;
}