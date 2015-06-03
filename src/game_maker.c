#include "game_maker.h"
#include "png_info.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <dirent.h>
#include <ctype.h>
#include <stdbool.h>

#define U32LE_FROM_BUF(BUF) ( \
	 (uint32_t)((BUF)[0])        | \
	((uint32_t)((BUF)[1]) <<  8) | \
	((uint32_t)((BUF)[2]) << 16) | \
	((uint32_t)((BUF)[3]) << 24))

#define WRITE_U32LE(BUF,N) { \
	(BUF)[0] =  (uint32_t)(N)        & 0xFF; \
	(BUF)[1] = ((uint32_t)(N) >>  8) & 0xFF; \
	(BUF)[2] = ((uint32_t)(N) >> 16) & 0xFF; \
	(BUF)[3] = ((uint32_t)(N) >> 24) & 0xFF; \
}

#define LOG_ERR(FMT, ...) fprintf(stderr, "*** ERROR: " FMT "\n", ## __VA_ARGS__)
#define LOG_ERR_MSG(MSG)  fprintf(stderr, "*** ERROR: " MSG "\n")

#if defined(GM_WINDOWS)
#	include <direct.h>
#	define mkdir(PATH,MODE) _mkdir(PATH)
#endif

static int gm_copydata(FILE *src, off_t srcoff, FILE *dst, off_t dstoff, size_t size) {
	uint8_t buf[BUFSIZ];

	if (src == dst) {
		errno = EINVAL;
		return -1;
	}

	if (fseeko(src, srcoff, SEEK_SET) != 0) {
		return -1;
	}
	
	if (fseeko(dst, dstoff, SEEK_SET) != 0) {
		return -1;
	}

	while (size > 0) {
		size_t chunk_size = size >= BUFSIZ ? BUFSIZ : size;
		if (fread(buf, chunk_size, 1, src) != 1) {
			if (!ferror(src)) {
				LOG_ERR_MSG("unexpected end of file while copying file data");
				errno = EINVAL;
			}
			return -1;
		}
		if (fwrite(buf, chunk_size, 1, dst) != 1) {
			return -1;
		}
		size -= chunk_size;
	}

	return 0;
}

static int gm_mkpath(const char *pathname) {
	char buf[PATH_MAX];
	struct stat st;

	if (!pathname) {
		LOG_ERR_MSG("pathname cannot be NULL");

		errno = EINVAL;
		return -1;
	}
	
	if (!*pathname) {
		LOG_ERR_MSG("pathname cannot be empty");

		errno = EINVAL;
		return -1;
	}

	if (strlen(pathname) >= sizeof(buf)) {
		LOG_ERR("pathname too long: %s", pathname);

		errno = ENAMETOOLONG;
		return -1;
	}

	strncpy(buf, pathname, sizeof(buf));

	char *ptr = buf;

#if defined(GM_WINDOWS)
	// skip UNC prefix or drive letter
	if (// UNC path
		(ptr[0] == '\\' && ptr[1] == '\\') ||

		// absolute short path
		(isalpha(ptr[0]) && ptr[1] == ':'))
	{
		ptr += 2;
	}
#endif

	for (;; ++ ptr) {
		// skip redundant path seperators
#if defined(GM_WINDOWS)
		while (*ptr == '\\' || *ptr == '/') ++ ptr;
#else
		while (*ptr == '/') ++ ptr;
#endif

		// find end of current path component
		for (; *ptr; ++ ptr) {
#if defined(GM_WINDOWS)
			if (*ptr == '\\' || *ptr == '/') break;
#else
			if (*ptr == '/') break;
#endif
		}

		char ch = *ptr;
		*ptr = '\0';

		if (stat(buf, &st) == 0) {
			if (!S_ISDIR(st.st_mode)) {
				LOG_ERR("exists but is not a directory: %s", buf);

				errno = ENOTDIR;
				return -1;
			}
		}
		else if (errno != ENOENT) {
			return -1;
		}
		else if (mkdir(buf, S_IRWXU) != 0) {
			return -1;
		}

		*ptr = ch;
		if (!ch) break;
	}

	return 0;
}

static int gm_write_patch_data(FILE *fp, const struct gm_patch *patch) {
	int status = 0;

	switch (patch->patch_src) {
	case GM_SRC_MEM:
		if (fwrite(patch->src.data, patch->size, 1, fp) != 1) {
			status = -1;
		}
		break;

	case GM_SRC_FILE:
		{
			FILE *infile = fopen(patch->src.filename, "rb");
			if (infile) {
				status = gm_copydata(infile, 0, fp, ftello(fp), patch->size);
				fclose(infile);
			}
			else {
				status = -1;
			}
		}
		break;
	
	default:
		errno = EINVAL;
		status = -1;
		break;
	}

	return status;
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
			LOG_ERR("can't move %s section (not implemented)", gm_section_name(index->section));

			errno = ENOSYS;
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
		LOG_ERR("can't patch %s section (not implemented)", gm_section_name(index->section));

		errno = ENOSYS;
		return -1;
	}

	if (patch->index >= index->entry_count) {
		LOG_ERR("patch index out of range: section = %s, patch index = %" PRIuPTR ", entry count = %" PRIuPTR,
		        gm_section_name(index->section), patch->index, index->entry_count);

		errno = EINVAL;
		return -1;
	}

	struct gm_patched_entry *entry = &index->entries[patch->index];
	if (entry->patch) {
		LOG_ERR("section %s, entry %" PRIuPTR " is already patched", gm_section_name(index->section), patch->index);

		errno = EINVAL;
		return -1;
	}

	if (entry->entry->type != patch->type) {
		LOG_ERR("section %s, entry %" PRIuPTR " type missmatch: entry type = %s, patch type = %s",
			gm_section_name(index->section), patch->index, gm_typename(entry->entry->type), gm_typename(patch->type));

		errno = EINVAL;
		return -1;
	}

	if (index->section == GM_TXTR) {
		// validate replacement sprite dimensions
		if (entry->entry->meta.txtr.width  != patch->meta.txtr.width ||
		    entry->entry->meta.txtr.height != patch->meta.txtr.height) {
			LOG_ERR("section %s, entry %" PRIuPTR " sprite dimensions missmatch: entry dimensions = %" PRIuPTR "x%" PRIuPTR
			        ", patch dimensions = %" PRIuPTR "x%" PRIuPTR,
				gm_section_name(index->section), patch->index, entry->entry->meta.txtr.width, entry->entry->meta.txtr.height,
				patch->meta.txtr.width, patch->meta.txtr.height);

			errno = EINVAL;
			return -1;
		}
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

enum gm_section gm_parse_section(const uint8_t *name) {
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
	uint8_t buffer[8];
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
		if (offset > INT32_MAX) {
			LOG_ERR("offset too big: offset = %" PRIu32 ", max. allowed = %" PRIu32, offset, INT32_MAX);

			errno = ERANGE;
			goto error;
		}
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

		const uint32_t value = U32LE_FROM_BUF(buffer);
		if (value != 1) {
			LOG_ERR("at offset %" PRIi64 ", section %s, entry %" PRIuPTR ": unexpected value of non-reverse engineered field: value = %" PRIu32,
				(int64_t)info_offsets[index], gm_section_name(section->section), index, value);

			errno = ENOSYS;
			goto error;
		}

		uint32_t offset = U32LE_FROM_BUF(buffer + 4);
		if (offset > INT32_MAX) {
			LOG_ERR("offset too big: offset = %" PRIu32 ", max. allowed = %" PRIu32, offset, INT32_MAX);

			errno = ERANGE;
			goto error;
		}
		entry->offset = (off_t)offset;
		if (fseeko(game, (off_t)offset, SEEK_SET) != 0) {
			goto error;
		}

		struct png_info meta;
		if (parse_png_info(game, &meta) != 0) {
			LOG_ERR("section %s, entry %" PRIuPTR ": error parsing sprite file",
				gm_section_name(section->section), index);

			goto error;
		}

		entry->size = meta.filesize;
		entry->type = GM_PNG;
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
	uint8_t buffer[12];
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
		if (offset > INT32_MAX) {
			LOG_ERR("offset too big: offset = %" PRIu32 ", max. allowed = %" PRIu32, offset, INT32_MAX);

			errno = ERANGE;
			goto error;
		}
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

		uint32_t size = U32LE_FROM_BUF(buffer);
		uint32_t hdrsize = size < 12 ? size : 12;
		if (fread(buffer, hdrsize, 1, game) != 1) {
			goto error;
		}
		if (hdrsize >= 12 &&
		    memcmp(buffer, "RIFF", 4) == 0 &&
		    memcmp(buffer + 8, "WAVE", 4) == 0) {
			entry->type = GM_WAVE;
		}
		else if (hdrsize >= 4 && memcmp(buffer, "OggS", 4) == 0) {
			entry->type = GM_OGG;
		}
		else {
			entry->type = GM_UNKNOWN;
		}
		entry->offset = offsets[index] + 4;
		entry->size   = size;
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

	uint8_t buffer[8];
	if (fread(buffer, 8, 1, game) != 1) {
		goto error;
	}

	if (memcmp(buffer, "FORM", 4) != 0) {
		LOG_ERR("unsupported file magic: '%c%c%c%c' (0x%0x 0x%0x 0x%0x 0x%0x)",
			buffer[0], buffer[1], buffer[2], buffer[3],
			buffer[0], buffer[1], buffer[2], buffer[3]);

		errno = ENOSYS;
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
			LOG_ERR("at offset %" PRIi64 ": unsupported section magic: '%c%c%c%c' (0x%0x 0x%0x 0x%0x 0x%0x)",
				(int64_t)offset,
				buffer[0], buffer[1], buffer[2], buffer[3],
				buffer[0], buffer[1], buffer[2], buffer[3]);

			errno = ENOSYS;
			goto error;
		}

		size_t section_size = U32LE_FROM_BUF(buffer + 4);
		if (section_size > INT32_MAX - 8 || offset > INT32_MAX - 8 - (int32_t)section_size) {
			LOG_ERR("section size big: section size = %" PRIuPTR ", max. allowed = %" PRIu32, section_size, INT32_MAX);

			errno = ERANGE;
			goto error;
		}

		if ((size_t)offset + section_size + 8 > (size_t)end_offset) {
			LOG_ERR("%s section overflows file: section offset = %" PRIi64 ", section size = %" PRIuPTR ", file size = %" PRIi64,
				gm_section_name(section_type), (int64_t)offset, section_size + 8, (int64_t)end_offset);

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

		offset += section_size + 8;
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

int gm_write_hdr(FILE *fp, const uint8_t *magic, size_t size) {
	if (!magic) {
		LOG_ERR_MSG("magic may not be NULL");

		errno = EINVAL;
		return -1;
	}

	if (size > UINT32_MAX) {
		LOG_ERR("section size out of range: size = %" PRIuPTR ", max size = %" PRIu32, size, UINT32_MAX);

		errno = EINVAL;
		return -1;
	}

	uint8_t buffer[8];
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

int gm_patch_archive(const char *filename, const struct gm_patch *patches) {
	char tmpname[PATH_MAX];
	FILE *game = NULL;
	FILE *tmp  = NULL;
	struct gm_index *index           = NULL;
	struct gm_patched_index *patched = NULL;
	int status = 0;

	memset(tmpname, 0, sizeof(tmpname));
	if (GM_CONCAT(tmpname, sizeof(tmpname), filename, ".tmp") != 0) {
		errno = ENAMETOOLONG;
		goto error;
	}

	game = fopen(filename, "rb");
	if (!game) {
		LOG_ERR("Failed to open archive: %s", filename);
		goto error;
	}

	index = gm_read_index(game);
	if (!index) {
		goto error;
	}

	// build patch index
	const size_t count = gm_index_length(index);
	patched = calloc(count + 1, sizeof(struct gm_patched_index));
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
	patched[count].section = GM_END;

	// adjust patch index
	for (const struct gm_patch *patch = patches; patch->section != GM_END; ++ patch) {
		struct gm_patched_index *section = gm_get_section(patched, patch->section);
		if (!section) {
			LOG_ERR("archive contains no %s section", gm_section_name(patch->section));

			errno = EINVAL;
			goto error;
		}

		if (gm_patch_entry(section, patch) != 0) {
			LOG_ERR("applying patch for section %s, entry %" PRIuPTR " failed",
				gm_section_name(patch->section), patch->index);
			goto error;
		}
	}

	// write new archive
	tmp = fopen(tmpname, "wb");
	if (!tmp) {
		LOG_ERR("Failed to open temp file: %s", tmpname);
		goto error;
	}

	size_t form_size = gm_form_size(patched);
	if (gm_write_hdr(tmp, (const uint8_t*)"FORM", form_size) != 0) {
		goto error;
	}

	for (struct gm_patched_index *ptr = patched; ptr->section != GM_END; ++ ptr) {
		uint8_t buffer[8];

		if (fseeko(tmp, ptr->offset, SEEK_SET) != 0) {
			goto error;
		}

		if (gm_write_hdr(tmp, (const uint8_t*)gm_section_name(ptr->section), ptr->size) != 0) {
			goto error;
		}

		switch (ptr->section) {
		case GM_TXTR:
		{
			WRITE_U32LE(buffer, ptr->entry_count);
			if (fwrite(buffer, 4, 1, tmp) != 1) {
				goto error;
			}
			const uint32_t fileinfo_offset = (uint32_t)ptr->offset + 12 + 4 * ptr->entry_count;
			for (size_t i = 0; i < ptr->entry_count; ++ i) {
				WRITE_U32LE(buffer, fileinfo_offset + i * 8);
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
					if (gm_write_patch_data(tmp, entry->patch) != 0) {
						goto error;
					}
				}
				else if (gm_copydata(game, entry->entry->offset, tmp, entry->offset, entry->size) != 0) {
					goto error;
				}
			}
			break;
		}

		case GM_AUDO:
			WRITE_U32LE(buffer, ptr->entry_count);
			if (fwrite(buffer, 4, 1, tmp) != 1) {
				goto error;
			}
			for (size_t i = 0; i < ptr->entry_count; ++ i) {
				uint32_t offset = ptr->entries[i].offset - 4;
				WRITE_U32LE(buffer, offset);
				if (fwrite(buffer, 4, 1, tmp) != 1) {
					goto error;
				}
			}
			for (size_t i = 0; i < ptr->entry_count; ++ i) {
				struct gm_patched_entry *entry = &ptr->entries[i];
				if (entry->patch) {
					if (fseeko(tmp, entry->offset - 4, SEEK_SET) != 0) {
						goto error;
					}
					WRITE_U32LE(buffer, entry->patch->size);
					if (fwrite(buffer, 4, 1, tmp) != 1) {
						goto error;
					}
					if (gm_write_patch_data(tmp, entry->patch) != 0) {
						goto error;
					}
				}
				else if (gm_copydata(game, entry->entry->offset - 4, tmp, entry->offset - 4, entry->size + 4) != 0) {
					goto error;
				}
			}
			break;

		default:
			if (gm_copydata(game, ptr->index->offset, tmp, ptr->offset, ptr->size + 8) != 0) {
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

	// delete target mainly to make it work on windows:
	if (unlink(filename) != 0) {
		LOG_ERR("Failed to remove original game archive: %s", filename);
		goto error;
	}

	if (rename(tmpname, filename) != 0) {
		LOG_ERR("Failed to rename temp file to: %s", filename);
		goto error;
	}

	goto end;

error:
	status = -1;
	int errnum = errno;

	if (game) {
		fclose(game);
		game = NULL;
	}
	
	if (tmp) {
		fclose(tmp);
		tmp = NULL;
	}

	if (tmpname[0]) {
		unlink(tmpname);
	}

	// keep the original error
	errno = errnum;

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

int gm_patch_archive_from_dir(const char *filename, const char *dirname) {
	size_t capacity = 256;
	struct gm_patch *patches = calloc(capacity, sizeof(struct gm_patch));
	size_t i = 0;
	char *endptr = NULL;
	DIR *dir = NULL;
	FILE *fp = NULL;
	struct dirent *entry = NULL;
	char namebuf[PATH_MAX];
	long int index = 0;
	int status = 0;

	if (!patches) {
		perror("listing files");
		goto error;
	}

	if (GM_JOIN_PATH(namebuf, sizeof(namebuf), dirname, "txtr") != 0) {
		perror("listing files");
		goto error;
	}

	dir = opendir(namebuf);
	if (dir) {
		struct png_info info;

		for (;;) {
			if (i + 1 == capacity) {
				if (SIZE_MAX / (2 * sizeof(struct gm_patch)) < capacity) {
					errno = ENOMEM;
					perror("listing files");
					goto error;
				}
				capacity *= 2;
				struct gm_patch *new_patches = realloc(patches, capacity * sizeof(struct gm_patch));
				if (!new_patches) {
					perror("listing files");
					goto error;
				}
				memset(new_patches + i, 0, (capacity - i) * sizeof(struct gm_patch));
				patches = new_patches;
			}

			errno = 0;
			entry = readdir(dir);
			if (!entry) {
				if (errno != 0) {
					perror("listing files");
					goto error;
				}
				break;
			}

			if (GM_JOIN_PATH(namebuf, sizeof(namebuf), dirname, "txtr", entry->d_name) != 0) {
				perror("listing files");
				goto error;
			}
			
			index = strtol(entry->d_name, &endptr, 10);
			if ((strcasecmp(endptr, ".png") != 0 && strcasecmp(endptr, ".dat") != 0) ||
				endptr == entry->d_name || index < 0 || (unsigned long int)index > UINT32_MAX) {
				// ignore file
				continue;
			}

			fp = fopen(namebuf, "rb");
			if (!fp) {
				perror(namebuf);
				goto error;
			}

			if (parse_png_info(fp, &info) != 0) {
				perror(namebuf);
				goto error;
			}
			fclose(fp);
			fp = NULL;

			char *filename = strdup(namebuf);
			if (!filename) {
				perror(namebuf);
				goto error;
			}

			struct gm_patch *ptr = &patches[i];
			ptr->section      = GM_TXTR;
			ptr->index        = index;
			ptr->type         = GM_PNG;
			ptr->patch_src    = GM_SRC_FILE;
			ptr->size         = info.filesize;
			ptr->src.filename = filename;
			ptr->meta.txtr.width  = info.width;
			ptr->meta.txtr.height = info.height;

			++ i;
		}
	}
	else if (errno != ENOENT) {
		perror("listing files");
		goto error;
	}

	if (GM_JOIN_PATH(namebuf, sizeof(namebuf), dirname, "audo") != 0) {
		perror("listing files");
		goto error;
	}

	dir = opendir(namebuf);
	if (dir) {
		struct stat st;
		char buffer[12];

		for (;;) {
			if (i + 1 == capacity) {
				if (SIZE_MAX / (2 * sizeof(struct gm_patch)) < capacity) {
					errno = ENOMEM;
					perror("listing files");
					goto error;
				}
				capacity *= 2;
				struct gm_patch *new_patches = realloc(patches, capacity * sizeof(struct gm_patch));
				if (!new_patches) {
					perror("listing files");
					goto error;
				}
				memset(new_patches + i, 0, (capacity - i) * sizeof(struct gm_patch));
				patches = new_patches;
			}

			errno = 0;
			entry = readdir(dir);
			if (!entry) {
				if (errno != 0) {
					perror("listing files");
					goto error;
				}
				break;
			}

			if (GM_JOIN_PATH(namebuf, sizeof(namebuf), dirname, "audo", entry->d_name) != 0) {
				perror("listing files");
				goto error;
			}
			
			index = strtol(entry->d_name, &endptr, 10);
			if ((strcasecmp(endptr, ".wav") != 0 && strcasecmp(endptr, ".ogg") != 0 && strcasecmp(endptr, ".dat") != 0) ||
				endptr == entry->d_name || index < 0 || (unsigned long int)index > UINT32_MAX) {
				// ignore file
				continue;
			}

			if (stat(namebuf, &st) != 0) {
				perror(namebuf);
				goto error;
			}

			fp = fopen(namebuf, "rb");
			if (!fp) {
				perror(namebuf);
				goto error;
			}

			const size_t count = fread(buffer, 1, sizeof(buffer), fp);
			if (ferror(fp)) {
				perror(namebuf);
				goto error;
			}
			fclose(fp);
			fp = NULL;

			char *filename = strdup(namebuf);
			if (!filename) {
				perror(namebuf);
				goto error;
			}

			struct gm_patch *ptr = &patches[i];
			if (count >= 12 && memcmp(buffer, "RIFF", 4) == 0 && memcmp(buffer + 8, "WAVE", 4) == 0) {
				ptr->type = GM_WAVE;
			}
			else if (count >= 4 && memcmp(buffer, "OggS", 4) == 0) {
				ptr->type = GM_OGG;
			}
			else {
				ptr->type = GM_UNKNOWN;
			}

			ptr->section      = GM_AUDO;
			ptr->index        = index;
			ptr->patch_src    = GM_SRC_FILE;
			ptr->size         = st.st_size;
			ptr->src.filename = filename;

			++ i;
		}
	}
	else if (errno != ENOENT) {
		perror("listing files");
		goto error;
	}

	patches[i].section = GM_END;

	if (gm_patch_archive(filename, patches) != 0) {
		goto error;
	}

	goto end;

error:
	status = -1;

end:
	if (fp) {
		fclose(fp);
		fp = NULL;
	}

	if (dir) {
		closedir(dir);
		dir = NULL;
	}

	if (patches) {
		for (struct gm_patch *patch = patches; patch->section != GM_END; ++ patch) {
			if (patch->src.filename) {
				free((void*)patch->src.filename);
				patch->src.filename = NULL;
			}
		}
		free(patches);
		patches = NULL;
	}

	return status;
}

const char *gm_extension(enum gm_filetype type) {
	switch (type) {
		case GM_PNG:  return ".png";
		case GM_WAVE: return ".wav";
		case GM_OGG:  return ".ogg";
		default:      return ".bin";
	}
}

const char *gm_typename(enum gm_filetype type) {
	switch (type) {
		case GM_PNG:  return "PNG";
		case GM_WAVE: return "WAVE";
		case GM_OGG:  return "Ogg";
		default:      return "(Unknown)";
	}
}

int gm_dump_files(const struct gm_index *index, FILE *game, const char *outdir) {
	char buf[PATH_MAX];

	for (; index->section != GM_END; ++ index) {
		const char *dir = NULL;

		switch (index->section) {
		case GM_TXTR:
			dir = "txtr";
			break;

		case GM_AUDO:
			dir = "audo";
			break;

		default:
			continue;
		}

		if (GM_JOIN_PATH(buf, sizeof(buf), outdir, dir) != 0) {
			return -1;
		}

		if (gm_mkpath(buf) != 0) {
			return -1;
		}

		for (size_t i = 0; i < index->entry_count; ++ i) {
			const struct gm_entry *entry = &index->entries[i];
			const char *ext = gm_extension(entry->type);
			int count = snprintf(buf, sizeof(buf), "%s%c%s%c%04" PRIuPTR "%s",
			                     outdir, GM_PATH_SEP, dir, GM_PATH_SEP, i, ext);

			if (count < 0) {
				return -1;
			}
			else if ((size_t)count >= sizeof(buf)) {
				LOG_ERR("Name too long: %s%c%s%c%04" PRIuPTR "%s",
				        outdir, GM_PATH_SEP, dir, GM_PATH_SEP, i, ext);

				errno = ENAMETOOLONG;
				return -1;
			}

			puts(buf);

			FILE *fp = fopen(buf, "wb");
			if (!fp) {
				return -1;
			}

			if (gm_copydata(game, entry->offset, fp, 0, entry->size) != 0) {
				fclose(fp);
				return -1;
			}

			if (fclose(fp) != 0) {
				return -1;
			}
		}
	}

	return 0;
}

int gm_concat(char *buf, size_t size, const char *strs[], size_t nstrs) {
	size_t ch_index = 0;

	for (size_t strs_index = 0; strs_index < nstrs; ++ strs_index) {
		const char *str = strs[strs_index];

		size_t strsize = strlen(str);
		if (strsize >= size || ch_index >= size - strsize) {
			errno = EINVAL;
			return -1;
		}

		memcpy(buf + ch_index, str, strsize);
		ch_index += strsize;
	}

	if (ch_index >= size) {
		errno = EINVAL;
		return -1;
	}

	buf[ch_index] = '\0';

	return 0;
}

int gm_join_path(char *buf, size_t size, const char *comps[], size_t ncomps) {
	size_t ch_index = 0;
	bool first = true;

	for (size_t comp_index = 0; comp_index < ncomps; ++ comp_index) {
		const char *comp = comps[comp_index];

		if (first) {
			first = false;
		}
		else if (ch_index >= size) {
			errno = ENAMETOOLONG;
			return -1;
		}
		else {
			buf[ch_index] = GM_PATH_SEP;
			++ ch_index;
		}

		size_t complen = strlen(comp);
		if (complen >= size || ch_index >= size - complen) {
			errno = ENAMETOOLONG;
			return -1;
		}

		memcpy(buf + ch_index, comp, complen);
		ch_index += complen;
	}

	if (ch_index >= size) {
		errno = ENAMETOOLONG;
		return -1;
	}

	buf[ch_index] = '\0';

	return 0;
}
