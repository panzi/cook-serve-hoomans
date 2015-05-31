#ifndef GAME_MAKER_H
#define GAME_MAKER_H
#pragma once

#include <stdio.h>
#include <inttypes.h>

#if defined(_WIN16) || defined(_WIN32) || defined(_WIN64)
#	define GM_WINDOWS
#	define GM_PATH_SEP '\\'
#else
#	define GM_PATH_SEP '/'
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum gm_filetype {
	GM_UNKNOWN = 0,
	GM_PNG,
	GM_WAVE,
	GM_OGG
};

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
	enum gm_section      section;
	size_t               index;
	enum gm_filetype     type;
	const unsigned char *data;
	size_t               size;

	union {
		struct {
			size_t width;
			size_t height;
		} txtr;
	} meta;
};

struct gm_entry {
	off_t            offset;
	size_t           size;
	enum gm_filetype type;

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

struct gm_patched_index *gm_get_section(struct gm_patched_index *patched, enum gm_section section);
int                      gm_shift_tail(struct gm_patched_index *index, off_t offset);
int                      gm_patch_entry(struct gm_patched_index *index, const struct gm_patch *patch);
void                     gm_free_index(struct gm_index *index);
void                     gm_free_patched_index(struct gm_patched_index *index);
const char              *gm_section_name(enum gm_section section);
const char              *gm_extension(enum gm_filetype type);
enum gm_section          gm_parse_section(const char *name);
int                      gm_read_index_txtr(FILE *game, struct gm_index *section);
int                      gm_read_index_audo(FILE *game, struct gm_index *section);
struct gm_index         *gm_read_index(FILE *game);
size_t                   gm_form_size(const struct gm_patched_index *index);
int                      gm_write_hdr(FILE *fp, const char *magic, size_t size);
size_t                   gm_index_length(const struct gm_index *index);
int                      gm_patch_archive(const char *filename, struct gm_patch *patches);
int                      gm_dump_files(const struct gm_index *index, FILE *game, const char *outdir);

#ifdef __cplusplus
}
#endif
#endif
