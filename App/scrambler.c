/* scrambler.c — compilation unit for the frequency-inversion voice scrambler.
 *
 * scrambler.h uses a single-header design: the full implementation is guarded
 * by #ifdef SCRAMBLER_IMPL.  Define that here (in exactly ONE .c file) so the
 * implementation is compiled once; all other files only see the declarations.
 */

#ifdef ENABLE_SCRAMBLER
#define SCRAMBLER_IMPL
#include "scrambler.h"
#endif
