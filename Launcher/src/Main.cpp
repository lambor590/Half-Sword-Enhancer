#include "../include/Launcher.h"
#include "../include/SelfUpdate.h"

int main() {
    if (auto commandResult = hse::TryRunSelfUpdateCommand()) return *commandResult;
    HSELauncher app;
    return app.Run();
}
