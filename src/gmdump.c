#include "game_maker.h"

#include <stdio.h>

int main(int argc, char *argv[]) {
	int status = 0;
	FILE *game = NULL;
	struct gm_index *index = NULL;
	const char *outdir = ".";
	const char *gamename = NULL;

	if (argc < 2) {
		fprintf(stderr, "*** usage: %s archive [outdir]\n", argc < 1 ? "gmdump" : argv[0]);
		goto error;
	}

	if (argc > 2) {
		outdir = argv[2];
	}

	gamename = argv[1];

	printf("Reading archive...\n");
	game = fopen(gamename, "rb");
	if (!game) {
		perror(gamename);
		goto error;
	}

	index = gm_read_index(game);
	if (!index) {
		perror(gamename);
		goto error;
	}

	printf("Dumping files...\n");
	if (gm_dump_files(index, game, outdir) != 0) {
		perror(gamename);
		goto error;
	}

	printf("Successfully dumped all files.\n");

	goto end;

error:
	status = 1;

end:
	if (game) {
		fclose(game);
		game = NULL;
	}

	if (index) {
		gm_free_index(index);
		index = NULL;
	}

#ifdef GM_WINDOWS
	printf("Press ENTER to continue...");
	getchar();
#endif

	return status;
}
