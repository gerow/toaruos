/* vim: tabstop=4 shiftwidth=4 noexpandtab
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2015 Mike Gerow
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static const char usage[] =
"Usage: %s [-h] [FILE]\n"
"    -h: Print this help message and exit.\n";

int main(int argc, char * argv[]) {
	int symlink_flag = 0;

	int c;
	while ((c = getopt(argc, argv, "h")) != -1) {
		switch (c) {
			case 'h':
				fprintf(stdout, usage, argv[0]);
				exit(EXIT_SUCCESS);
			default:
				fprintf(stderr, usage, argv[0]);
				exit(EXIT_FAILURE);
		}
	}
	char * file = NULL;
	if (argc - optind == 1) {
		file = argv[opind];
	}
}

typedef buf struct {
	char * lines;
	size_t len;
};
