#include "patch_game.h"
#include "cook_serve_hoomans.h"
#include "hoomans_png.h"
#include "icons_png.h"

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
#include <assert.h>

#ifndef static_assert
#	define static_assert _Static_assert
#endif

#define _STR(X) #X
#define STR(X) _STR(X)

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

#if 0
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
	static_assert(ICONS_PNG_LEN <= CSH_ICONS_SIZE,
		"icons.png must be <= " STR(CSH_ICONS_SIZE) " bytes but is "
		STR(ICONS_PNG_LEN) " bytes");

	static_assert(HOOMANS_PNG_LEN <= CSH_HOOMANS_SIZE,
		"hoomans.png must be <= " STR(CSH_HOOMANS_SIZE) " bytes but is "
		STR(HOOMANS_PNG_LEN) " bytes");

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
#else

enum gm_section {
	GM_END = 0,

	GM_GEN8,
	GM_OPTN,
	GM_EXTN,
	GM_SOND,
	GM_SPRT,
	GM_BGND,
	GM_PATH,
	GM_SCPT,
	GM_FONT,
	GM_TMLN,
	GM_OBJT,
	GM_ROOM,
	GM_DAFL,
	GM_TPAG,
	GM_CODE,
	GM_VARI,
	GM_FUNC,
	GM_STRG,
	GM_TXTR,
	GM_AUDO
};

struct {
	enum gm_section section;
	size_t index;
	const unsigned char *data;
	size_t size;
	union {
		struct {
			size_t width;
			size_t height;
		} txtr;
	} meta;
} gm_patch;

struct {
	off_t  offset;
	size_t size;
	union {
		struct {
			size_t width;
			size_t height;
		} txtr;
	} meta;
} gm_entry;

struct {
	enum gm_section section;
	
	off_t  offset;
	size_t size;

	size_t entry_count;
	struct gm_entry *entries;
} gm_index;

#define U32LE_FROM_BUF(BUF) ((uint32_t)((BUF)[0]) | (uint32_t)((BUF)[1]) << 8 | (uint32_t)((BUF)[2]) << 16 | (uint32_t)((BUF)[3]) << 24)

void gm_free_index(struct gm_index *index) {
	if (index) {
		for (struct gm_index *ptr = index; ptr->section != GM_END; ++ ptr) {
			if (ptr->entries) {
				free(ptr->entries);
				ptr->entries = NULL;
			}
		}
		free(index);
	}
}

enum gm_section gm_parse_section(const char *buffer) {
	if      (memcmp("GEN8", name, 4) == 0) { return GM_GEN8; }
	else if (strcmp("OPTN", name, 4) == 0) { return GM_OPTN; }
	else if (strcmp("EXTN", name, 4) == 0) { return GM_EXTN; }
	else if (strcmp("SOND", name, 4) == 0) { return GM_SOND; }
	else if (strcmp("SPRT", name, 4) == 0) { return GM_SPRT; }
	else if (strcmp("BGND", name, 4) == 0) { return GM_BGND; }
	else if (strcmp("PATH", name, 4) == 0) { return GM_PATH; }
	else if (strcmp("SCPT", name, 4) == 0) { return GM_SCPT; }
	else if (strcmp("SHDR", name, 4) == 0) { return GM_SHDR; }
	else if (strcmp("FONT", name, 4) == 0) { return GM_FONT; }
	else if (strcmp("TMLN", name, 4) == 0) { return GM_TMLN; }
	else if (strcmp("OBJT", name, 4) == 0) { return GM_OBJT; }
	else if (strcmp("ROOM", name, 4) == 0) { return GM_ROOM; }
	else if (strcmp("DAFL", name, 4) == 0) { return GM_DAFL; }
	else if (strcmp("TPAG", name, 4) == 0) { return GM_TPAG; }
	else if (strcmp("CODE", name, 4) == 0) { return GM_CODE; }
	else if (strcmp("VARI", name, 4) == 0) { return GM_VARI; }
	else if (strcmp("FUNC", name, 4) == 0) { return GM_FUNC; }
	else if (strcmp("STRG", name, 4) == 0) { return GM_STRG; }
	else if (strcmp("TXTR", name, 4) == 0) { return GM_TXTR; }
	else if (strcmp("AUDO", name, 4) == 0) { return GM_AUDO; }
	else return GM_END;
}

int gm_read_index_txtr(FILE *game, struct gm_index *section) {
	char buffer[8];
	size_t count = 0;
	off_t *info_offsets = NULL;
	struct gm_entry *entries = NULL;
	int status = 0;

	if (fread(buffer, 4, 1, game) != 1) {
		goto error;
	}

	count = U32LE_FROM_BUF(buffer);
	entries = calloc(count, sizeof(struct gm_entry));
	if (!entries) {
		goto error;
	}

	info_offsets = calloc(count, sizeof(off_t));
	if (!info_offsets) {
		goto error;
	}

	for (size_t index = 0; index < count; ++ index) {
		if (fread(buffer, 4, 1, game) != 1) {
			goto error;
		}

		uint32_t offset = U32LE_FROM_BUF(buffer);
		// XXX: possible integer overflow on 32bit systems
		info_offsets[index] = (off_t)offset;
	}
	
	for (size_t index = 0; index < count; ++ index) {
		struct gm_entry *entry = &entries[index];
		if (fseeko(game, info_offsets[index], SEEK_SET) != 0) {
			goto error;
		}

		if (fread(buffer, 8, 1, game) != 1) {
			goto error;
		}

		if (U32LE_FROM_BUF(buffer) != 1) {
			errno = EINVAL;
			goto error;
		}

		uint32_t offset = U32LE_FROM_BUF(buffer + 4);
		// XXX: possible integer overflow on 32bit systems
		entry->offset = (off_t)offset;
		if (fseeko(game, (off_t)offset, SEEK_SET) != 0) {
			goto error;
		}

		struct png_info meta;
		if (parse_png_info(game, &meta) != 0) {
			goto error;
		}

		entry->size = meta.filesize;
		entry->meta.txtr.width  = meta.width;
		entry->meta.txtr.height = meta.height;
	}

	section->entry_count = count;
	section->entries     = entries;

	goto end;

error:
	status = -1;

	if (entries) {
		free(entries);
		entries = NULL;
	}

end:

	if (info_offsets) {
		free(info_offsets);
		info_offsets = NULL;
	}

	return status;
}

int gm_read_index_audo(FILE *game, struct gm_index *index) {
	char buffer[4];
	size_t count = 0;
	off_t *offsets = NULL;
	struct gm_entry *entries = NULL;
	int status = 0;

	if (fread(buffer, 4, 1, game) != 1) {
		goto error;
	}

	count = U32LE_FROM_BUF(buffer);
	entries = calloc(count, sizeof(struct gm_entry));
	if (!entries) {
		goto error;
	}

	offsets = calloc(count, sizeof(off_t));
	if (!offsets) {
		goto error;
	}

	for (size_t index = 0; index < count; ++ index) {
		if (fread(buffer, 4, 1, game) != 1) {
			goto error;
		}

		uint32_t offset = U32LE_FROM_BUF(buffer);
		// XXX: possible integer overflow on 32bit systems
		offsets[index] = (off_t)offset;
	}
	
	for (size_t index = 0; index < count; ++ index) {
		struct gm_entry *entry = &entries[index];
		if (fseeko(game, offsets[index], SEEK_SET) != 0) {
			goto error;
		}

		if (fread(buffer, 4, 1, game) != 1) {
			goto error;
		}

		entry->offset = offsets[index] + 4;
		entry->size   = U32LE_FROM_BUF(buffer);
	}

	section->entry_count = count;
	section->entries     = entries;

	goto end;

error:
	status = -1;

	if (entries) {
		free(entries);
		entries = NULL;
	}

end:

	if (offsets) {
		free(offsets);
		offsets = NULL;
	}

	return status;
}

struct gm_index *gm_read_index(FILE *game) {
	size_t capacity = 32;
	size_t count = 0;
	struct gm_index *index = calloc(capacity, sizeof(struct gm_index));

	if (!index) {
		goto error;
	}

	char buffer[8];
	if (fread(buffer, 8, 1, game) != 1) {
		goto error;
	}

	if (memcmp(buffer, "FORM", 4) != 0) {
		errno = EINVAL;
		goto error;
	}

	const size_t form_size = U32LE_FROM_BUF(buffer + 4);
	const off_t end_offset = form_size + 8;
	off_t offset = 8;
	
	while (offset < end_offset) {
		if (fread(buffer, 8, 1, game) != 1) {
			goto error;
		}

		enum gm_section section = gm_parse_section(buffer);
		if (gm_section == GM_END) {
			errno = EINVAL;
			goto error;
		}

		size_t section_size = U32LE_FROM_BUF(buffer + 4);
		if (offset + section_size > end_offset) { // XXX: possible integer overflow
			errno = EINVAL;
			goto error;
		}

		if (count >= capacity) {
			capacity *= 2;
			struct gm_index *new_index = realloc(index, capacity * sizeof(struct gm_index));
			if (!new_index) {
				goto error;
			}
			memset(new_index + count * sizeof(struct gm_index), 0, (capacity - count) * sizeof(struct gm_index));
			index = new_index;
		}

		struct gm_index *section = &index[count];
		section->section = section;
		section->offset  = offset;
		section->size    = section_size;

		switch (section) {
		case GM_TXTR:
			if (gm_read_index_txtr(game, section_size, section) != 0) {
				goto error;
			}
			break;

		case GM_AUDO:
			if (gm_read_index_audo(game, section_size, section) != 0) {
				goto error;
			}
			break;

		default:
			break;
		}

		offset += section_size;
		if (fseeko(game, offset, SEEK_SET) != 0) {
			goto error;
		}

		++ count;
	}

	goto end;

error:
	if (index) {
		gm_free_index(index);
		index = NULL;
	}

end:
	return index;
}

size_t gm_index_length(const struct gm_index *index) {
	size_t len = 0;
	while (index->section != GM_END) {
		++ len;
		++ index;
	}
	return len;
}

struct gm_index *gm_clone_index(const struct gm_index *index) {
	size_t len = gm_index_length(index);
	struct gm_index *clone = calloc(len + 1, sizeof(struct gm_index));
	if (!clone) {
		goto error;
	}

	for (size_t i = 0; i < len; ++ i) {
		size_t entry_count = index[i].entry_count;
		clone[i].section     = index[i].section;
		clone[i].offset      = index[i].offset;
		clone[i].size        = index[i].size;
		clone[i].entry_count = entry_count;

		struct gm_entry *entries = calloc(entry_count, sizeof(struct gm_entry));
		if (!entries) {
			goto error;
		}

		clone[i].entries = memcpy(entries, index[i].entries, entry_count * sizeof(struct gm_entry));
	}

	goto end;

error:
	if (clone) {
		gm_free_index(clone);
		clone = NULL;
	}

end:
	return clone;
}

int gm_patch_archive(FILE *game, struct gm_patch *patches) {
	int status = 0;
	struct gm_index *index   = gm_read_index(game);
	struct gm_index *patched = NULL;

	if (!index) {
		goto error;
	}

	patched = gm_clone_index(index);
	if (!patched) {
		goto error;
	}

	for (struct gm_patch *ptr = patches; ptr->section != GM_END; ++ ptr) {
		// TODO
	}

	goto end;

error:
	status = -1;

end:

	if (index) {
		gm_free_index(index);
		index = NULL;
	}

	if (patched) {
		gm_free_index(patched);
		patched = NULL;
	}

	return status;
}

int main(int argc, char *argv[]) {
	char game_name_buf[PATH_MAX];
	FILE *game = NULL;
	int status = EXIT_SUCCESS;
	const char *game_name = NULL;

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

	

	goto end;

error:
	status = EXIT_FAILURE;
	if (game) {
		fclose(game);
		game = NULL;
	}

end:

#if defined(_WIN16) || defined(_WIN32) || defined(_WIN64)
	printf("Press ENTER to continue...");
	getchar();
#endif

	return status;
}

#endif
