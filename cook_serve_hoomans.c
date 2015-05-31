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

int copydata(FILE *src, off_t srcoff, FILE *dst, off_t dstoff, size_t size) {
	char buf[BUFSIZ];

	if (fseeko(src, srcoff, SEEK_SET) != 0) {
		return -1;
	}
	
	if (fseeko(dst, dstoff, SEEK_SET) != 0) {
		return -1;
	}

	while (size > 0) {
		size_t chunk_size = size >= BUFSIZ ? BUFSIZ : size;
		if (fread(buf, chunk_size, 1, src) != 1) {
			return -1;
		}
		if (fwrite(buf, chunk_size, 1, dst) != 1) {
			return -1;
		}
		size -= chunk_size;
	}

	return 0;
}

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
	GM_SHDR,
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

struct gm_patch {
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
};

struct gm_entry {
	off_t  offset;
	size_t size;
	union {
		struct {
			size_t width;
			size_t height;
		} txtr;
	} meta;
};

struct gm_index {
	enum gm_section section;
	
	off_t  offset;
	size_t size;

	size_t entry_count;
	struct gm_entry *entries;
};

struct gm_patched_entry {
	off_t  offset;
	size_t size;

	const struct gm_patch *patch;
	const struct gm_entry *entry;
};

struct gm_patched_index {
	enum gm_section section;

	off_t  offset;
	size_t size;

	size_t entry_count;
	struct gm_patched_entry *entries;

	const struct gm_index *index;
};

#define U32LE_FROM_BUF(BUF) ((uint32_t)((BUF)[0]) | (uint32_t)((BUF)[1]) << 8 | (uint32_t)((BUF)[2]) << 16 | (uint32_t)((BUF)[3]) << 24)
#define WRITE_U32LE(BUF,N) { \
	(BUF)[0] =  (uint32_t)(N)        & 0xFF; \
	(BUF)[1] = ((uint32_t)(N) >>  8) & 0xFF; \
	(BUF)[2] = ((uint32_t)(N) >> 16) & 0xFF; \
	(BUF)[3] = ((uint32_t)(N) >> 24) & 0xFF; \
}

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

struct gm_patched_index *gm_get_section(struct gm_patched_index *patched, enum gm_section section) {
	for (; patched->section != GM_END; ++ patched) {
		if (patched->section == section) {
			return patched;
		}
	}
	return NULL;
}

int gm_shift_tail(struct gm_patched_index *index, off_t offset) {
	for (; index->section != GM_END; ++ index) {
		switch (index->section) {
		// only know how to move these sections so far:
		case GM_TXTR:
		case GM_AUDO:			
			break;

		default:
			errno = EINVAL;
			return -1;
		}

		index->offset += offset;

		for (size_t i = 0; i < index->entry_count; ++ i) {
			index->entries[i].offset += offset;
		}
	}

	return 0;
}

int gm_patch_entry(struct gm_patched_index *index, const struct gm_patch *patch) {
	switch (index->section) {
	// only know how to patch these sections so far:
	case GM_TXTR:
	case GM_AUDO:			
		break;

	default:
		errno = EINVAL;
		return -1;
	}

	if (patch->index >= index->entry_count) {
		errno = EINVAL;
		return -1;
	}

	struct gm_patched_entry *entry = &index->entries[patch->index];
	if (entry->patch) {
		errno = EINVAL;
		return -1;
	}

	off_t offset = patch->size - entry->entry->size;
	index->size += offset;
	entry->size  = patch->size;
	entry->patch = patch;

	// also look at previous siblings, just in case
	for (size_t i = 0; i < index->entry_count; ++ i) {
		struct gm_patched_entry *other = &index->entries[i];
		if (other->offset > entry->offset) {
			other->offset += offset;
		}
	}

	return gm_shift_tail(index + 1, offset);
}

void gm_free_patched_index(struct gm_patched_index *index) {
	if (index) {
		for (struct gm_patched_index *ptr = index; ptr->section != GM_END; ++ ptr) {
			if (ptr->entries) {
				free(ptr->entries);
				ptr->entries = NULL;
			}
		}
		free(index);
	}
}

const char *gm_section_name(enum gm_section section) {
	switch (section) {
	case GM_GEN8: return "GEN8";
	case GM_OPTN: return "OPTN";
	case GM_EXTN: return "EXTN";
	case GM_SOND: return "SOND";
	case GM_SPRT: return "SPRT";
	case GM_BGND: return "BGND";
	case GM_PATH: return "PATH";
	case GM_SCPT: return "SCPT";
	case GM_SHDR: return "SHDR";
	case GM_FONT: return "FONT";
	case GM_TMLN: return "TMLN";
	case GM_OBJT: return "OBJT";
	case GM_ROOM: return "ROOM";
	case GM_DAFL: return "DAFL";
	case GM_TPAG: return "TPAG";
	case GM_CODE: return "CODE";
	case GM_VARI: return "VARI";
	case GM_FUNC: return "FUNC";
	case GM_STRG: return "STRG";
	case GM_TXTR: return "TXTR";
	case GM_AUDO: return "AUDO";
	default: return NULL;
	}
}

enum gm_section gm_parse_section(const char *name) {
	if      (memcmp("GEN8", name, 4) == 0) { return GM_GEN8; }
	else if (memcmp("OPTN", name, 4) == 0) { return GM_OPTN; }
	else if (memcmp("EXTN", name, 4) == 0) { return GM_EXTN; }
	else if (memcmp("SOND", name, 4) == 0) { return GM_SOND; }
	else if (memcmp("SPRT", name, 4) == 0) { return GM_SPRT; }
	else if (memcmp("BGND", name, 4) == 0) { return GM_BGND; }
	else if (memcmp("PATH", name, 4) == 0) { return GM_PATH; }
	else if (memcmp("SCPT", name, 4) == 0) { return GM_SCPT; }
	else if (memcmp("SHDR", name, 4) == 0) { return GM_SHDR; }
	else if (memcmp("FONT", name, 4) == 0) { return GM_FONT; }
	else if (memcmp("TMLN", name, 4) == 0) { return GM_TMLN; }
	else if (memcmp("OBJT", name, 4) == 0) { return GM_OBJT; }
	else if (memcmp("ROOM", name, 4) == 0) { return GM_ROOM; }
	else if (memcmp("DAFL", name, 4) == 0) { return GM_DAFL; }
	else if (memcmp("TPAG", name, 4) == 0) { return GM_TPAG; }
	else if (memcmp("CODE", name, 4) == 0) { return GM_CODE; }
	else if (memcmp("VARI", name, 4) == 0) { return GM_VARI; }
	else if (memcmp("FUNC", name, 4) == 0) { return GM_FUNC; }
	else if (memcmp("STRG", name, 4) == 0) { return GM_STRG; }
	else if (memcmp("TXTR", name, 4) == 0) { return GM_TXTR; }
	else if (memcmp("AUDO", name, 4) == 0) { return GM_AUDO; }
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

int gm_read_index_audo(FILE *game, struct gm_index *section) {
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

		entry->offset = offsets[index];
		entry->size   = U32LE_FROM_BUF(buffer) + 4;
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

		enum gm_section section_type = gm_parse_section(buffer);
		if (section_type == GM_END) {
			errno = EINVAL;
			goto error;
		}

		size_t section_size = U32LE_FROM_BUF(buffer + 4);
		if ((size_t)offset + section_size > (size_t)end_offset) { // XXX: possible integer overflow
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
		section->section = section_type;
		section->offset  = offset;
		section->size    = section_size;

		switch (section_type) {
		case GM_TXTR:
			if (gm_read_index_txtr(game, section) != 0) {
				goto error;
			}
			break;

		case GM_AUDO:
			if (gm_read_index_audo(game, section) != 0) {
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

size_t gm_form_size(const struct gm_patched_index *index) {
	size_t size = 0;
	while (index->section != GM_END) {
		size += index->size + 8;
		++ index;
	}
	return size;
}

int gm_write_hdr(FILE *fp, const char *magic, size_t size) {
	if (!magic) {
		errno = EINVAL;
		return -1;
	}

	if (size > UINT32_MAX) {
		errno = ERANGE;
		return -1;
	}

	char buffer[8];
	memcpy(buffer, magic, 4);
	WRITE_U32LE(buffer + 4, (uint32_t)size);

	if (fwrite(buffer, 8, 1, fp) != 1) {
		return -1;
	}

	return 0;
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

int gm_patch_archive(const char *filename, struct gm_patch *patches) {
	char tmpname[PATH_MAX];
	FILE *game = NULL;
	FILE *tmp  = NULL;
	struct gm_index *index           = NULL;
	struct gm_patched_index *patched = NULL;
	int status = 0;

	if (snprintf(tmpname, PATH_MAX, "%s.tmp", filename) < 0) {
		goto error;
	}

	game = fopen(filename, "rb");
	if (!game) {
		goto error;
	}

	index = gm_read_index(game);
	if (!index) {
		goto error;
	}

	// build patch index
	const size_t count = gm_index_length(index);
	patched = calloc(count, sizeof(struct gm_patched_index));
	if (!patched) {
		goto error;
	}

	for (size_t i = 0; i < count; ++ i) {
		size_t entry_count = index[i].entry_count;
		struct gm_patched_entry *entries = calloc(entry_count, sizeof(struct gm_patched_entry));
		if (!entries) {
			goto error;
		}

		struct gm_entry *index_entries = index[i].entries;
		for (size_t j = 0; j < entry_count; ++ j) {
			entries[j].offset = index_entries[j].offset;
			entries[j].size   = index_entries[j].size;
			entries[j].entry  = &index_entries[j];
		}

		patched[i].section     = index[i].section;
		patched[i].offset      = index[i].offset;
		patched[i].size        = index[i].size;
		patched[i].entry_count = entry_count;
		patched[i].entries     = entries;
		patched[i].index       = &index[i];
	}

	// adjust patch index
	for (struct gm_patch *ptr = patches; ptr->section != GM_END; ++ ptr) {
		struct gm_patched_index *section = gm_get_section(patched, ptr->section);
		if (!section) {
			errno = EINVAL;
			goto error;
		}

		if (gm_patch_entry(section, ptr) != 0) {
			goto error;
		}
	}

	// write new archive
	tmp = fopen(tmpname, "wb");
	if (!tmp) {
		goto error;
	}

	size_t form_size = gm_form_size(patched);
	if (gm_write_hdr(tmp, "FORM", form_size) != 0) {
		goto error;
	}
	
	for (struct gm_patched_index *ptr = patched; ptr->section != GM_END; ++ ptr) {
		char buffer[8];

		if (fseeko(tmp, ptr->offset, SEEK_SET) != 0) {
			goto error;
		}

		if (gm_write_hdr(tmp, gm_section_name(ptr->section), ptr->size) != 0) {
			goto error;
		}

		switch (ptr->section) {
		case GM_TXTR:
			WRITE_U32LE(buffer, ptr->entry_count);
			if (fwrite(buffer, 4, 1, tmp) != 1) {
				goto error;
			}
			{
				const uint32_t fileinfo_offset = (uint32_t)ptr->offset + 12 + 4 * ptr->entry_count;
				for (size_t i = 0; i < ptr->entry_count; ++ i) {
					WRITE_U32LE(buffer, fileinfo_offset + i * 4);
					if (fwrite(buffer, 4, 1, tmp) != 1) {
						goto error;
					}
				}
				for (size_t i = 0; i < ptr->entry_count; ++ i) {
					WRITE_U32LE(buffer, 1);
					WRITE_U32LE(buffer + 4, ptr->entries[i].offset);
					if (fwrite(buffer, 8, 1, tmp) != 1) {
						goto error;
					}
				}
				for (size_t i = 0; i < ptr->entry_count; ++ i) {
					struct gm_patched_entry *entry = &ptr->entries[i];
					if (entry->patch) {
						if (fseeko(tmp, entry->offset, SEEK_SET) != 0) {
							goto error;
						}
						if (fwrite(entry->patch->data, entry->patch->size, 1, tmp) != 1) {
							goto error;
						}
					}
					else if (copydata(game, entry->entry->offset, tmp, entry->offset, entry->size) != 0) {
						goto error;
					}
				}			
			}
			break;

		case GM_AUDO:
			WRITE_U32LE(buffer, ptr->entry_count);
			if (fwrite(buffer, 4, 1, tmp) != 1) {
				goto error;
			}
			for (size_t i = 0; i < ptr->entry_count; ++ i) {
				WRITE_U32LE(buffer, ptr->entries[i].offset);
				if (fwrite(buffer, 4, 1, tmp) != 1) {
					goto error;
				}
			}
			for (size_t i = 0; i < ptr->entry_count; ++ i) {
				struct gm_patched_entry *entry = &ptr->entries[i];
				if (entry->patch) {
					if (fseeko(tmp, entry->offset, SEEK_SET) != 0) {
						goto error;
					}
					// this means patch->data must already include the file size
					if (fwrite(entry->patch->data, entry->patch->size, 1, tmp) != 1) {
						goto error;
					}
				}
				else if (copydata(game, entry->entry->offset, tmp, entry->offset, entry->size) != 0) {
					goto error;
				}
			}
			break;

		default:
			if (copydata(game, ptr->index->offset, tmp, ptr->offset, ptr->size) != 0) {
				goto error;
			}
		}
	}

	if (fclose(game) != 0) {
		game = NULL;
		goto error;
	}
	
	if (fclose(tmp) != 0) {
		tmp = NULL;
		goto error;
	}

	if (rename(tmpname, filename) != 0) {
		goto error;
	}

	goto end;

error:
	status = -1;

	if (game) {
		fclose(game);
		game = NULL;
	}
	
	if (tmp) {
		fclose(tmp);
		tmp = NULL;
	}

end:

	if (index) {
		gm_free_index(index);
		index = NULL;
	}

	if (patched) {
		gm_free_patched_index(patched);
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
