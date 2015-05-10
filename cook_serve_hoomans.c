#include "patch_game.h"
#include "cook_serve_hoomans.h"

#include <utime.h>
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

extern unsigned char hoomans_png[];
extern unsigned int hoomans_png_len;

extern unsigned char icons_png[];
extern unsigned int icons_png_len;

#if defined(_WIN16) || defined(_WIN32) || defined(_WIN64)
#	include <windows.h>

int find_archive(char *path, size_t pathlen) {
	HKEY hKey = 0;
	DWORD dwType = REG_SZ;
	DWORD dwSize = pathlen;

	if (pathlen < 46) {
		return -1;
	}

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Valve\\Steam", 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) {
		return -1;
	}

	if (RegQueryValueEx(hKey, TEXT("InstallPath"), NULL, &dwType, (LPBYTE)path, &dwSize) != ERROR_SUCCESS) {
		return -1;
	}

	if (dwType != REG_SZ || dwSize > pathlen - 46) {
		return -1;
	}

	strcat(path, "\\steamapps\\common\\CookServeDelicious\\data.win");

	return 0;
}
#else
int find_archive(char *path, size_t pathlen) {
	static const char *paths[] = {
		"/.local/share/Steam/SteamApps/common/CookServeDelicious/assets/game.unx",
		"/.local/share/Steam/steamapps/common/CookServeDelicious/assets/game.unx",
		"/.local/share/steam/SteamApps/common/CookServeDelicious/assets/game.unx",
		"/.local/share/steam/steamapps/common/CookServeDelicious/assets/game.unx",
		"/.steam/steam/SteamApps/common/CookServeDelicious/assets/game.unx",
		"/.steam/steam/steamapps/common/CookServeDelicious/assets/game.unx",
		"/.steam/Steam/SteamApps/common/CookServeDelicious/assets/game.unx",
		"/.steam/Steam/steamapps/common/CookServeDelicious/assets/game.unx",
		NULL
	};
	const char *home = getenv("HOME");
	struct stat info;

	if (!home) {
		return -1;
	}

	for (const char **ptr = paths; *ptr; ++ ptr) {
		if (snprintf(path, pathlen, "%s%s", home, *ptr) < 0) {
			return -1;
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

static const char *prompt(const char *msg, const char *defval, char *buf, size_t bufsize) {
	printf("%s", msg);

	if (!fgets(buf, bufsize, stdin)) {
		return NULL;
	}

	const char *ptr = buf;
	char *end = buf + strlen(buf);
	for (; ptr < end; ++ ptr) {
		if (!isspace(*ptr))
			break;
	}
	for (; ptr < end; -- end) {
		if (!isspace(*(end - 1)))
			break;
	}
	if (ptr == end) {
		return defval;
	}
	*end = '\0';
	return ptr;
}

static int prompt_yes_no(const char *msg, bool defval) {
	char buf[BUFSIZ];

	for (;;) {
		const char *val = prompt(msg, defval ? "Y" : "N", buf, BUFSIZ);

		if (!val) {
			return -1;
		}

		if (strcasecmp(val, "Y") == 0 || strcasecmp(val, "YES") == 0) {
			return 1;
		}
		else if (strcasecmp(val, "N") == 0 || strcasecmp(val, "NO") == 0) {
			return 0;
		}
		else {
			printf("Illegal input. Please enter 'Y' or 'N'.\n");
		}
	}
}

int main(int argc, char *argv[]) {
	char game_name_buf[PATH_MAX];
	FILE *game = NULL;
	int status = EXIT_SUCCESS;
	const char *game_name = NULL;
	char *backup_name = NULL;
	struct stat game_stat;
	struct stat backup_stat;
	size_t backup_name_len = 0;

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
		printf("Patching game archive: %s\n", game_name);
	}

	backup_name_len = strlen(game_name) + 8;
	backup_name = calloc(backup_name_len, 1);
	if (!backup_name) {
		perror("*** ERROR: allocating file name");
		goto error;
	}

	if (snprintf(backup_name, backup_name_len, "%s.backup", game_name) < 0) {
		perror("*** ERROR: creatig backup file name");
		goto error;
	}

	if (stat(game_name, &game_stat) < 0) {
		perror("*** ERROR: accessing game archive");
		goto error;
	}

	int create_backup = 0;
	bool backup_is_current = false;
	if (stat(backup_name, &backup_stat) < 0) {
		if (errno != ENOENT) {
			perror("*** ERROR: accessing backup file");
			goto error;
		}
		create_backup = 1;
	}
	else {
		int restore_backup = 1;
		if (game_stat.st_mtime <= backup_stat.st_mtime) {
			if (game_stat.st_size != backup_stat.st_size) {
				printf("Backup exists but has different size than the game archive.\n");
				restore_backup = prompt_yes_no("Use backup anyway? [y/N]: ", false);
			}
		}
		else {
			// seems like there was an update
			printf("Backup exists but is older than the game archive. Maybe there was a game update?\n");
			restore_backup = prompt_yes_no("Use backup anyway? [y/N]: ", false);
		}
		
		if (restore_backup < 0) {
			perror("*** ERROR: user interaction");
			goto error;
		}

		if (restore_backup) {
			if (copyfile(backup_name, game_name) < 0) {
				perror("*** ERROR: restoring backup");
				goto error;
			}
			printf("Restored backup of game archive.\n");
			backup_is_current = true;
		}
		else {
			printf("Should instead the backup be recreated?\n");
			create_backup = prompt_yes_no("Overwrite backup? [y/N]: ", false);
		}
	}

	if (create_backup < 0) {
		perror("*** ERROR: user interaction");
		goto error;
	}

	if (create_backup) {
		if (copyfile(game_name, backup_name) < 0) {
			perror("*** ERROR: creating backup");
			goto error;
		}
		printf("Created backup of game archive.\n");
		backup_is_current = true;
	}

	game = fopen(game_name, "r+b");
	if (!game) {
		fprintf(stderr, "*** ERROR: opening file: %s\n", strerror(errno));
		goto error;
	}

	if (patch_game(game, CSH_ICONS_OFFSET, icons_png, icons_png_len, CSH_ICONS_WIDTH, CSH_ICONS_HEIGHT) != 0) {
		goto error;
	}

	if (patch_game(game, CSH_HOOMANS_OFFSET, hoomans_png, hoomans_png_len, CSH_HOOMANS_WIDTH, CSH_HOOMANS_HEIGHT) != 0) {
		goto error;
	}

	if (fclose(game) != 0) {
		fprintf(stderr, "*** ERROR: closing file: %s\n", strerror(errno));
		game = NULL;
		goto error;
	}
	game = NULL;
	printf("Successfully replaced sprites.\n");

	if (backup_is_current && utime(backup_name, NULL) < 0) {
		perror("*** ERROR: marking backup as new");
		goto error;
	}

	goto end;

error:
	status = EXIT_FAILURE;
	if (game) {
		fclose(game);
		game = NULL;
	}

end:
	if (backup_name) {
		free(backup_name);
		backup_name = NULL;
	}

#if defined(_WIN16) || defined(_WIN32) || defined(_WIN64)
	printf("Press ENTER to continue...");
	getchar();
#endif

	return status;
}
