#include "../include/Launcher.h"
#include "../include/SelfUpdate.h"

int main(int argc, char* argv[]) {
    if (auto commandResult = hse::TryRunSelfUpdateCommand()) return *commandResult;
    HSELauncher app;
    return app.Run(argc, argv);
}
