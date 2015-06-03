#include "cook_serve_hoomans.h"
#include "game_maker.h"
#include "hoomans_png.h"
#include "icons_png.h"

#include <strings.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#ifndef static_assert
#	define static_assert _Static_assert
#endif

#if defined(GM_WINDOWS)
#	include <windows.h>

#define CSH_DATA_WIN_PATH "\\steamapps\\common\\CookServeDelicious\\data.win"

int find_archive(char *path, size_t pathlen) {
	HKEY hKey = 0;
	DWORD dwType = REG_SZ;
	DWORD dwSize = pathlen;

	if (pathlen < sizeof(CSH_DATA_WIN_PATH)) {
		errno = ENAMETOOLONG;
		return -1;
	}

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Valve\\Steam", 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) {
		errno = EINVAL;
		return -1;
	}

	if (RegQueryValueEx(hKey, TEXT("InstallPath"), NULL, &dwType, (LPBYTE)path, &dwSize) != ERROR_SUCCESS) {
		errno = EINVAL;
		return -1;
	}

	if (dwType != REG_SZ) {
		errno = EINVAL;
		return -1;
	}
	else if (dwSize > pathlen - sizeof(CSH_DATA_WIN_PATH)) {
		errno = ENAMETOOLONG;
		return -1;
	}

	strcat(path, CSH_DATA_WIN_PATH);

	return 0;
}
#else
int find_archive(char *path, size_t pathlen) {
	static const char *paths[] = {
		".local/share/Steam/SteamApps/common/CookServeDelicious/assets/game.unx",
		".local/share/Steam/steamapps/common/CookServeDelicious/assets/game.unx",
		".local/share/steam/SteamApps/common/CookServeDelicious/assets/game.unx",
		".local/share/steam/steamapps/common/CookServeDelicious/assets/game.unx",
		".steam/steam/SteamApps/common/CookServeDelicious/assets/game.unx",
		".steam/steam/steamapps/common/CookServeDelicious/assets/game.unx",
		".steam/Steam/SteamApps/common/CookServeDelicious/assets/game.unx",
		".steam/Steam/steamapps/common/CookServeDelicious/assets/game.unx",
		NULL
	};
	const char *home = getenv("HOME");
	struct stat info;

	if (!home) {
		return -1;
	}

	for (const char **ptr = paths; *ptr; ++ ptr) {
		if (GM_JOIN_PATH(path, pathlen, home, *ptr) != 0) {
			// ignore too long paths
			continue;
		}

		if (stat(path, &info) < 0) {
			if (errno != ENOENT) {
				perror(path);
			}
		}
		else if (S_ISREG(info.st_mode)) {
			return 0;
		}
	}

	return -1;
}
#endif

static int copyfile(const char *src, const char *dst) {
	char buf[BUFSIZ];
	FILE *fsrc = NULL;
	FILE *fdst = NULL;
	int status = 0;

	fsrc = fopen(src, "rb");
	if (!fsrc) {
		goto error;
	}

	fdst = fopen(dst, "wb");
	if (!fdst) {
		goto error;
	}

	for (;;) {
		size_t count = fread(buf, 1, BUFSIZ, fsrc);

		if (count < BUFSIZ && ferror(fsrc)) {
			goto error;
		}

		if (count > 0 && fwrite(buf, 1, count, fdst) != count) {
			goto error;
		}

		if (count < BUFSIZ) {
			break;
		}
	}

	goto end;

error:
	status = -1;

end:

	if (fsrc) {
		fclose(fsrc);
		fsrc = NULL;
	}

	if (fdst) {
		fclose(fdst);
		fdst = NULL;
	}

	return status;
}

int main(int argc, char *argv[]) {
	const struct gm_patch patches[] = {
		GM_PATCH_TXTR(CSH_ICONS_INDEX,   icons_png,   icons_png_len,   CSH_ICONS_WIDTH,   CSH_ICONS_HEIGHT),
		GM_PATCH_TXTR(CSH_HOOMANS_INDEX, hoomans_png, hoomans_png_len, CSH_HOOMANS_WIDTH, CSH_HOOMANS_HEIGHT),
		GM_PATCH_END
	};

	char game_name_buf[PATH_MAX];
	char backup_name[PATH_MAX];
	int status = EXIT_SUCCESS;
	const char *game_name = NULL;
	struct stat st;

	if (argc > 2) {
		fprintf(stderr, "*** ERROR: Please pass the %s file to this program.\n", CSH_GAME_ARCHIVE);
		goto error;
	}
	else if (argc == 2) {
		game_name = argv[1];
	}
	else {
		if (find_archive(game_name_buf, PATH_MAX) < 0) {
			fprintf(stderr, "*** ERROR: Couldn't find %s file.\n", CSH_GAME_ARCHIVE);
			goto error;
		}
		game_name = game_name_buf;
	}

	printf("Patching game archive: %s\n", game_name);

	// create backup if there isn't one
	if (GM_CONCAT(backup_name, sizeof(backup_name), game_name, ".backup") != 0) {
		errno = ENAMETOOLONG;
		perror("*** ERROR: creatig backup file name");
		goto error;
	}

	if (stat(backup_name, &st) == 0) {
		if (!S_ISREG(st.st_mode)) {
			fprintf(stderr, "*** ERROR: Backup file is not a regular file.\n");
			goto error;
		}
	}
	else if (errno == ENOENT) {
		if (copyfile(game_name, backup_name) != 0) {
			perror("*** ERROR: Creatig backup");
			goto error;
		}
		printf("Created backup of game archive: %s\n", backup_name);
	}
	else {
		perror("*** ERROR: Error accessing backup file");
		goto error;
	}

	// patch the archive
	if (gm_patch_archive(game_name, patches) != 0) {
		fprintf(stderr, "*** ERROR: Error patching archive: %s\n", strerror(errno));
		goto error;
	}
	
	printf("Successfully pached game.\n");

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
