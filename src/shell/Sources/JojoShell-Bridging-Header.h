/* Swift bridge to the platform C modules. The build passes -Xcc -I<ps1-port>/platform, so these resolve to the
 * shared headers in platform/ without copies. Do not add shell-only C declarations here. */
#ifndef JOJOSHELL_BRIDGING_HEADER_H
#define JOJOSHELL_BRIDGING_HEADER_H

#include "pad.h"
#include "file_layer.h"
#include "gpu_raster.h"   /* GpItem, GpuDisplayEnv and gpu_raster_copy_published for GpuMetal.swift */

/* Hosted runtime (build-host.sh defines JOJO_HOST and adds hybrid/ to the include path). */
#ifdef JOJO_HOST
#include "jojo_host_shell.h"
#endif

#endif
