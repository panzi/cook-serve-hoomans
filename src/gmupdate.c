#include "game_maker.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
	int status = 0;
	const char *indir = ".";
	const char *gamename = NULL;

	if (argc < 2) {
		fprintf(stderr, "*** usage: %s archive [dir]\n", argc < 1 ? "gmupdate" : argv[0]);
		goto error;
	}

	if (argc > 2) {
		indir = argv[2];
	}

	gamename = argv[1];

	// patch the archive
	if (gm_patch_archive_from_dir(gamename, indir) != 0) {
		fprintf(stderr, "*** ERROR: Error patching archive: %s\n", strerror(errno));
		goto error;
	}
	
	printf("Successfully pached game.\n");

	goto end;

error:
	status = 1;

end:

#ifdef GM_WINDOWS
	printf("Press ENTER to continue...");
	getchar();
#endif

	return status;
}
