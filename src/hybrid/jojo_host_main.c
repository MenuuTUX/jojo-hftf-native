/* Executable entry for the JOJO_SHELL_LIB build (development stage only). The runtime's own main() is renamed to
 * psxrecomp_main in that build, so psx-runtime still needs a main to link as an executable. The Swift shell links every
 * object of psx-runtime except this one (shell/build-host.sh), so its own main is the only one. */
#include "jojo_host_shell.h"

int main(int argc, char **argv) {
    return jojo_host_run(argc, argv);
}
