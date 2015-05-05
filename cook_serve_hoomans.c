#include "patch_game.h"
#include "cook_serve_hoomans.h"

#include <stdio.h>
#include <stdlib.h>

extern unsigned char hoomans_png[];
extern unsigned int hoomans_png_len;

extern unsigned char icons_png[];
extern unsigned int icons_png_len;

int main(int argc, char *argv[]) {
	FILE *game = NULL;
	int status = EXIT_SUCCESS;

	if (argc != 2) {
		fprintf(stderr, "*** ERROR: please pass the game.unx file to this program.\n");
		goto error;
	}

	game = fopen(argv[1], "r+b");

	if (!game) {
		perror(argv[1]);
		goto error;
	}

	if (patch_game(game, CSH_ICONS_OFFSET, icons_png, icons_png_len, CSH_ICONS_WIDTH, CSH_ICONS_HEIGHT) != 0) {
		goto error;
	}

	if (patch_game(game, CSH_HOOMANS_OFFSET, hoomans_png, hoomans_png_len, CSH_HOOMANS_WIDTH, CSH_HOOMANS_HEIGHT) != 0) {
		goto error;
	}

	if (fclose(game) != 0) {
		perror(argv[1]);
		game = NULL;
		goto error;
	}
	game = NULL;
	printf("Successfully replaced sprites.\n");

	goto end;

error:
	status = EXIT_FAILURE;
	if (game) {
		fclose(game);
	}

end:

#if defined(_WIN16) || defined(_WIN32) || defined(_WIN64)
	printf("Press ENTER to continue...");
	getchar();
#endif

	return status;
}
