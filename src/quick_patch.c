#include "patch_game.h"
#include "png_info.h"
#include "cook_serve_hoomans.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

const char *filename(const char *path) {
	const char *ptr = path + strlen(path) - 1;

	for (; ptr != path; -- ptr) {
#if defined(_WIN16) || defined(_WIN32) || defined(_WIN64)
		if (*ptr == '\\' || *ptr == '/') return ptr + 1;
#else
		if (*ptr == '/') return ptr + 1;
#endif
	}

	return path;
}

unsigned char *load_file(const char *filename, uint32_t width, uint32_t height, size_t *sizeptr) {
	struct png_info info;
	FILE *file = fopen(filename, "rb");
	unsigned char *data = NULL;
	off_t size = -1;

	if (!file) {
		goto error;
	}

	if (parse_png_info(file, &info) != 0) {
		fprintf(stderr, "%s: is not a valid PNG file\n", filename);
		goto error;
	}

	if (info.width != width || info.height != height) {
		fprintf(stderr, "%s: Image has invalid dimensions. Expected %" PRIu32 "x%" PRIu32
		                ", but image is %" PRIu32 "x%" PRIu32 ".",
			filename, width, height, info.width, info.height);
		goto error;
	}

	if (fseeko(file, 0, SEEK_END) != 0) {
		perror(filename);
		goto error;
	}

	size = ftello(file);
	if (size < 0) {
		perror(filename);
		goto error;
	}
	
	data = malloc((size_t)size);
	if (!data) {
		perror(filename);
		goto error;
	}

	if (fseeko(file, 0, SEEK_SET) != 0) {
		perror(filename);
		goto error;
	}

	if (fread(data, (size_t)size, 1, file) != 1) {
		perror(filename);
		goto error;
	}

	if (sizeptr) {
		*sizeptr = (size_t)size;
	}

	goto cleanup;

error:
	if (data) {
		free(data);
		data = NULL;
	}

cleanup:
	if (file) {
		fclose(file);
		file = NULL;
	}

	return data;
}

int main(int argc, char *argv[]) {
	int status = EXIT_SUCCESS;
	FILE *game = NULL;
	const char *game_filename    = NULL;
	const char *icons_filename   = NULL;
	const char *hoomans_filename = NULL;
	unsigned char *icons_data   = NULL;
	unsigned char *hoomans_data = NULL;
	size_t icons_len   = 0;
	size_t hoomans_len = 0;

	if (argc < 3) {
		fprintf(stderr, "*** ERROR: Please pass %s, hoomans.png and/or icons.png to this program.\n", CSH_GAME_ARCHIVE);
		goto error;
	}

	for (int i = 1; i < argc; ++ i) {
		const char *path = argv[i];
		const char *name = filename(path);

		if (strcasecmp(name, "game.unx") == 0 || strcasecmp(name, "data.win") == 0) {
			game_filename = path;
		}
		else if (strcasecmp(name, "hoomans.png") == 0) {
			hoomans_filename = path;
		}
		else if (strcasecmp(name, "icons.png") == 0) {
			icons_filename = path;
		}
		else {
			fprintf(stderr, "*** ERROR: Don't know what to do with a file named '%s'.\n"
							"           Please pass files named %s, hoomans.png and/or icons.png to this program.\n",
							name, CSH_GAME_ARCHIVE);
			goto error;
		}
	}

	if (!game_filename || (!icons_filename && !hoomans_filename)) {
		fprintf(stderr, "*** ERROR: please pass %s, hoomans.png and/or icons.png to this program.\n", CSH_GAME_ARCHIVE);
		goto error;
	}

	if (icons_filename && !(icons_data = load_file(icons_filename, CSH_ICONS_WIDTH, CSH_ICONS_HEIGHT, &icons_len))) {
		goto error;
	}

	if (hoomans_filename && !(hoomans_data = load_file(hoomans_filename, CSH_HOOMANS_WIDTH, CSH_HOOMANS_HEIGHT, &hoomans_len))) {
		goto error;
	}

	game = fopen(game_filename, "r+b");
	if (!game) {
		perror(game_filename);
		goto error;
	}

	if (icons_data && patch_game(game, CSH_ICONS_OFFSET, icons_data, icons_len, CSH_ICONS_WIDTH, CSH_ICONS_HEIGHT) != 0) {
		goto error;
	}

	if (hoomans_data && patch_game(game, CSH_HOOMANS_OFFSET, hoomans_data, hoomans_len, CSH_HOOMANS_WIDTH, CSH_HOOMANS_HEIGHT) != 0) {
		goto error;
	}

	printf("Successfully replaced sprites.\n");

	goto cleanup;

error:
	status = EXIT_FAILURE;

cleanup:
	
	if (game) {
		fclose(game);
		game = NULL;
	}

	if (hoomans_data) {
		free(hoomans_data);
		hoomans_data = NULL;
	}
	
	if (icons_data) {
		free(icons_data);
		icons_data = NULL;
	}

#if defined(_WIN16) || defined(_WIN32) || defined(_WIN64)
	printf("Press ENTER to continue...");
	getchar();
#endif

	return status;
}
