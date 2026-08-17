#include "pch.h"

#include <Ring0/Lifecycle/DbgkRundownPolicy.h>
#include <Ring0/Lifecycle/EptWatchTransactionPolicy.h>

namespace
{
    void expect(bool condition, const char* message, int& failures)
    {
        if (!condition)
        {
            std::fprintf(stderr, "FAILED: %s\n", message);
            ++failures;
        }
    }
}

int RunLifecyclePolicyTests()
{
    int failures = 0;

    using namespace ept::watch_transaction_policy;

    expect(IsReserved(state::installing),
        "an installing watch must reserve its table slot",
        failures);
    expect(!IsPublished(state::installing),
        "an installing watch must remain invisible to consumers",
        failures);
    expect(IsPublished(state::active),
        "only an active watch may be published",
        failures);
    expect(OwnsHardware(state::removing),
        "a removing watch retains hardware ownership until rollback completes",
        failures);
    expect(!ContributesPermissions(state::removing),
        "a removing watch must not be re-applied during permission recomputation",
        failures);
    expect(!CanPublish(true, true, false),
        "guest ID writeback failure must abort watch publication",
        failures);
    expect(!CanPublish(true, false, true),
        "partial processor installation must abort watch publication",
        failures);
    expect(CanPublish(true, true, true),
        "complete installation and guest acknowledgement may publish the group",
        failures);
    expect(!CanBeginRemoval(true, false),
        "missing hardware resources must block whole-group removal",
        failures);
    expect(CanBeginRemoval(true, true),
        "a fully preflighted group may enter removing state",
        failures);

    using dbgk::rundown_policy::CanReleaseOwnedState;
    using dbgk::rundown_policy::restore_action;
    using dbgk::rundown_policy::SelectRestoreAction;

    constexpr std::uintptr_t installed = 0x1000;
    constexpr std::uintptr_t original = 0x2000;
    constexpr std::uintptr_t replacement = 0x3000;

    expect(SelectRestoreAction(false, false, false, 0, installed) ==
        restore_action::complete_without_write,
        "unowned callback state requires no restoration",
        failures);
    expect(SelectRestoreAction(true, true, true, installed, installed) ==
        restore_action::write_original,
        "the installed callback must be restored to its original owner",
        failures);
    expect(SelectRestoreAction(true, true, true, original, installed) ==
        restore_action::complete_without_write,
        "an already restored callback must not be overwritten",
        failures);
    expect(SelectRestoreAction(true, true, true, replacement, installed) ==
        restore_action::complete_without_write,
        "a third-party replacement must not be clobbered",
        failures);
    expect(SelectRestoreAction(true, false, false, 0, installed) ==
        restore_action::fail_stop,
        "lost callback ownership evidence must fail closed",
        failures);
    expect(!CanReleaseOwnedState(true, false),
        "owned Dbgk state must survive an unconfirmed callback restoration",
        failures);
    expect(CanReleaseOwnedState(true, true),
        "confirmed callback restoration may release Dbgk state",
        failures);

    return failures;
}
