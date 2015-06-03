#include "game_maker.h"
#include "png_info.h"
#include "cook_serve_hoomans.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *filename(const char *path) {
	const char *ptr = path + strlen(path) - 1;

	for (; ptr != path; -- ptr) {
#ifdef GM_WINDOWS
		if (*ptr == '\\' || *ptr == '/') return ptr + 1;
#else
		if (*ptr == '/') return ptr + 1;
#endif
	}

	return path;
}

int main(int argc, char *argv[]) {
	int status = EXIT_SUCCESS;
	const char *game_filename    = NULL;
	const char *icons_filename   = NULL;
	const char *hoomans_filename = NULL;
	struct stat st;

	struct gm_patch patches[] = {
		GM_PATCH_END,
		GM_PATCH_END,
		GM_PATCH_END
	};
	struct gm_patch *patch = patches;

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

	if (icons_filename) {
		if (stat(icons_filename, &st) != 0) {
			perror(icons_filename);
			goto error;
		}
		patch->section      = GM_TXTR;
		patch->index        = CSH_ICONS_INDEX;
		patch->type         = GM_PNG;
		patch->patch_src    = GM_SRC_FILE;
		patch->src.filename = icons_filename;
		patch->size         = st.st_size;
		patch->meta.txtr.width  = CSH_ICONS_WIDTH;
		patch->meta.txtr.height = CSH_ICONS_HEIGHT;
		patch ++;
	}

	if (hoomans_filename) {
		if (stat(hoomans_filename, &st) != 0) {
			perror(hoomans_filename);
			goto error;
		}
		patch->section      = GM_TXTR;
		patch->index        = CSH_HOOMANS_INDEX;
		patch->type         = GM_PNG;
		patch->patch_src    = GM_SRC_FILE;
		patch->src.filename = hoomans_filename;
		patch->size         = st.st_size;
		patch->meta.txtr.width  = CSH_HOOMANS_WIDTH;
		patch->meta.txtr.height = CSH_HOOMANS_HEIGHT;
		patch ++;
	}

	if (gm_patch_archive(game_filename, patches) != 0) {
		fprintf(stderr, "*** ERROR: Error patching archive: %s\n", strerror(errno));
		goto error;
	}

	printf("Successfully patched game.\n");

	goto end;

error:
	status = EXIT_FAILURE;

end:

#ifdef GM_WINDOWS
	printf("Press ENTER to continue...");
	getchar();
#endif

	return status;
}
