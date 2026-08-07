/* AALauncher_OS9_Prefix.h -- CodeWarrior target prefix for the Mac OS 9
 * (PPC/CFM) AALauncher build. Set as the target's Prefix File. A plain
 * classic Toolbox app (no SDL, no SIOUX) -- force-include MacHeaders and
 * relax the pragmas the AA sources rely on.
 */
#include <MacHeaders.c>          /* classic Mac PPC Toolbox, precompiled */

#pragma ANSI_strict off          /* allow // comments etc. */
#pragma mpwc_relax on            /* lenient char / unsigned char pointer rules */
#pragma require_prototypes off
