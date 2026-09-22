#include "collaboration_compat_shim.h"

#include <cstdlib>

int main() {
    if (mindarchy_automerge_roundtrip() != 0) return 1;
    if (const char *input = std::getenv("AUTOMERGE_GO_FIXTURE"); input && mindarchy_automerge_read_fixture(input) != 0)
        return 2;
    if (const char *output = std::getenv("AUTOMERGE_CPP_FIXTURE"); output && mindarchy_automerge_write_fixture(output) != 0)
        return 3;
    return 0;
}
