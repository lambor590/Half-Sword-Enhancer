#include <Windows.h>

#include "../include/Launcher.h"
#include "../include/SelfUpdate.h"

int main() {
    if (!SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32)) return 1;
    if (auto commandResult = hse::TryRunSelfUpdateCommand()) return *commandResult;
    HSELauncher app;
    return app.Run();
}
