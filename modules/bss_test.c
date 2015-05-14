/* vim: tabstop=4 shiftwidth=4 noexpandtab
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 */

#include <module.h>
#include <mod/shell.h>
#include <printf.h>

uint8_t bss_value;

DEFINE_SHELL_FUNCTION(bss_test, "[debug] Check to make sure bss values are zeroed") {
	fprintf(tty, "bss_value: %d\n", bss_value);

	return 0;
}


static int init(void) {
	BIND_SHELL_FUNCTION(bss_test);

	return 0;
}

static int fini(void) {
	return 0;
}

MODULE_DEF(bss_test, init, fini);
MODULE_DEPENDS(debugshell);
