#include "cook_serve_hoomans.h"
#include "game_maker.h"
#include "csh_patch_def.h"

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

struct reg_path {
	HKEY    hKey;
	LPCTSTR lpSubKey;
	LPCTSTR lpValueName;
};

static int get_path_from_registry(HKEY hKey, LPCTSTR lpSubKey, LPCTSTR lpValueName, char *path, size_t pathlen) {
	HKEY hSubKey = 0;
	DWORD dwType = REG_SZ;
	DWORD dwSize = pathlen;

	if (pathlen < sizeof(CSH_DATA_WIN_PATH)) {
		return ENAMETOOLONG;
	}

	if (RegOpenKeyEx(hKey, lpSubKey, 0, KEY_QUERY_VALUE, &hSubKey) != ERROR_SUCCESS) {
		return ENOENT;
	}

	if (RegQueryValueEx(hSubKey, lpValueName, NULL, &dwType, (LPBYTE)path, &dwSize) != ERROR_SUCCESS) {
		RegCloseKey(hSubKey);
		return ENOENT;
	}

	RegCloseKey(hSubKey);

	if (dwType != REG_SZ) {
		return ENOENT;
	}
	else if (dwSize > pathlen - sizeof(CSH_DATA_WIN_PATH)) {
		return ENAMETOOLONG;
	}

	strcat(path, CSH_DATA_WIN_PATH);

	return 0;
}

static int find_archive(char *path, size_t pathlen) {
	static const struct reg_path reg_paths[] = {
		// Have confirmed sigthing of these keys:
		{ HKEY_LOCAL_MACHINE, TEXT("Software\\Valve\\Steam"),              TEXT("InstallPath") },
		{ HKEY_LOCAL_MACHINE, TEXT("Software\\Wow6432node\\Valve\\Steam"), TEXT("InstallPath") },
		{ HKEY_CURRENT_USER,  TEXT("Software\\Valve\\Steam"),              TEXT("SteamPath")   },

		// All the other possible combination, just to to try everything:
		{ HKEY_CURRENT_USER,  TEXT("Software\\Wow6432node\\Valve\\Steam"), TEXT("SteamPath")   },
		{ HKEY_LOCAL_MACHINE, TEXT("Software\\Valve\\Steam"),              TEXT("SteamPath")   },
		{ HKEY_LOCAL_MACHINE, TEXT("Software\\Wow6432node\\Valve\\Steam"), TEXT("SteamPath")   },
		{ HKEY_CURRENT_USER,  TEXT("Software\\Valve\\Steam"),              TEXT("InstallPath") },
		{ HKEY_CURRENT_USER,  TEXT("Software\\Wow6432node\\Valve\\Steam"), TEXT("InstallPath") },
		{ 0,                  0,                                           0                   }
	};

	for (const struct reg_path* reg_path = reg_paths; reg_path->lpSubKey; ++ reg_path) {
		int errnum = get_path_from_registry(reg_path->hKey, reg_path->lpSubKey, reg_path->lpValueName, path, pathlen);
		if (errnum == 0) {
			return 0;
		}
		else if (errnum != ENOENT) {
			errno = errnum;
			return -1;
		}
	}

	errno = ENOENT;
	return -1;
}
#elif defined(__APPLE__)

#define CSD_STEAM_ARCHIVE "Library/Application Support/Steam/SteamApps/common/CookServeDelicious/Cook Serve Delicious.app/Contents/Resources/game.ios"
#define CSD_APP_ARCHIVE   "/Applications/Cook Serve Delicious.app/Contents/Resources/game.ios"

static int find_archive(char *path, size_t pathlen) {
	const char *home = getenv("HOME");
	struct stat info;

	if (!home) {
		return -1;
	}

	if (GM_JOIN_PATH(path, pathlen, home, CSD_STEAM_ARCHIVE) == 0) {
		if (stat(path, &info) < 0) {
			if (errno != ENOENT) {
				perror(path);
			}
		}
		else if (S_ISREG(info.st_mode)) {
			return 0;
		}
	}

	if (stat(CSD_APP_ARCHIVE, &info) < 0) {
		if (errno != ENOENT) {
			perror(path);
		}
		return -1;
	}
	else if (S_ISREG(info.st_mode)) {
		if (strlen(CSD_APP_ARCHIVE) + 1 > pathlen) {
			errno = ENAMETOOLONG;
			return -1;
		}
		strcpy(path, CSD_APP_ARCHIVE);
		return 0;
	}

	errno = ENOENT;
	return -1;
}
#else // default: Linux
#include <dirent.h>

static int find_path_ignore_case(const char *home, const char *prefix, const char* const path[], char buf[], size_t size) {
	int count = snprintf(buf, size, "%s/%s", home, prefix);
	if (count < 0) {
		return -1;
	}
	else if ((size_t)count >= size) {
		errno = ENAMETOOLONG;
		return -1;
	}

	for (const char* const* nameptr = path; *nameptr; ++ nameptr) {
		const char* realname = NULL;
		DIR *dir = opendir(buf);

		if (!dir) {
			if (errno != ENOENT) {
				perror(buf);
			}
			return -1;
		}

		for (;;) {
			errno = 0;
			struct dirent *entry = readdir(dir);
			if (entry) {
				if (strcasecmp(entry->d_name, *nameptr) == 0) {
					realname = entry->d_name;
					break;
				}
			}
			else if (errno == 0) {
				break; // end of dir
			}
			else {
				perror(buf);
				return -1;
			}
		}

		if (!realname) {
			closedir(dir);
			errno = ENOENT;
			return -1;
		}

		if (strlen(buf) + strlen(realname) + 2 > size) {
			errno = ENAMETOOLONG;
			return -1;
		}

		strcat(buf, "/");
		strcat(buf, realname);

		closedir(dir);
	}

	return 0;
}

static int find_archive(char *path, size_t pathlen) {
	// Steam was developed for Windows, which has case insenstive file names.
	// Therefore I can't assume a certain case and because I don't want to write
	// a parser for registry.vdf I scan the filesystem for certain names in a case
	// insensitive manner.
	static const char* const path1[] = {".local/share", "Steam", "SteamApps", "common", "CookServeDelicious" ,"assets", "game.unx", NULL};
	static const char* const path2[] = {".steam", "Steam", "SteamApps", "common", "CookServeDelicious", "assets", "game.unx", NULL};
	static const char const* const* paths[] = {path1, path2, NULL};

	const char *home = getenv("HOME");

	if (!home) {
		return -1;
	}

	for (const char* const* const* ptr = paths; ptr; ++ ptr) {
		const char* const* path_spec = *ptr;
		if (find_path_ignore_case(home, path_spec[0], path_spec + 1, path, pathlen) == 0) {
			struct stat info;

			if (stat(path, &info) < 0) {
				if (errno != ENOENT) {
					perror(path);
				}
			}
			else if (S_ISREG(info.st_mode)) {
				return 0;
			}
		}
	}

	errno = ENOENT;
	return -1;
}
#endif

#ifdef __linux__

// use sendfile() under Linux for zero-context switch file copy
#include <fcntl.h>
#include <sys/sendfile.h>

static int copyfile(const char *src, const char *dst) {
	int status =  0;
	int infd   = -1;
	int outfd  = -1;
	struct stat info;

	infd = open(src, O_RDONLY);
	if (infd < 0) {
		goto error;
	}

	outfd = open(dst, O_CREAT | O_WRONLY);
	if (outfd < 0) {
		goto error;
	}

	if (fstat(infd, &info) < 0) {
		goto error;
	}

	if (sendfile(outfd, infd, NULL, (size_t)info.st_size) < (ssize_t)info.st_size) {
		goto error;
	}

	goto end;

error:
	status = -1;

end:
	if (infd >= 0) {
		close(infd);
		infd = -1;
	}

	if (outfd >= 0) {
		close(outfd);
		outfd = -1;
	}

	return status;
}
#else
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
#endif

int main(int argc, char *argv[]) {
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
	if (gm_patch_archive(game_name, csh_patches) != 0) {
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
