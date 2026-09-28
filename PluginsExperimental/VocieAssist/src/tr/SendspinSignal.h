
#pragma once

namespace lva::tr {

// Send SIGUSR1 to sendspin-client (if running). Best-effort.
void SendspinDuck();

// Send SIGUSR2 to sendspin-client (if running). Best-effort.
void SendspinUnduck();

}  // namespace lva::tr
