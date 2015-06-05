#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "png_info.h"

#define LINE_LEN 8

int main(int argc, char *argv[]) {
	uint8_t buffer[LINE_LEN * 512];
	struct png_info info;
	FILE *res = NULL;
	FILE *hdr = NULL;
	FILE *src = NULL;
	int status = EXIT_SUCCESS;
	bool txtr = false;
	const char *type = NULL;
	char *macroname = NULL;
	size_t size = 0;

	if (argc != 5) {
		fprintf(stderr, "usage: %s symbol_name resource_file header_file source_file\n",
		        argc > 0 ? argv[0] : "make_resource");
		goto error;
	}

	const char *symname = argv[1];
	const char *resname = argv[2];
	const char *hdrname = argv[3];
	const char *srcname = argv[4];

	macroname = strdup(symname);
	if (!macroname) {
		perror(symname);
		goto error;
	}

	if (!isalpha(symname[0]) && symname[0] != '_') {
		fprintf(stderr, "illegal symbol name: '%s'\n", symname);
		goto error;
	}

	for (char *ptr = macroname; *ptr; ++ ptr) {
		if (!isalnum(*ptr) && *ptr != '_') {
			fprintf(stderr, "illegal symbol name: '%s'\n", symname);
			goto error;
		}
		*ptr = toupper(*ptr);
	}

	res = fopen(resname, "rb");
	if (!res) {
		perror(resname);
		goto error;
	}

	if (parse_png_info(res, &info) != 0) {
		if (errno != EINVAL) {
			perror(resname);
			goto error;
		}

		if (fseeko(res, 0, SEEK_SET) != 0) {
			perror(resname);
			goto error;
		}

		size_t count = fread(buffer, 1, 12, res);
		if (count >= 12 &&
			memcmp(buffer, "RIFF", 4) == 0 &&
			memcmp(buffer + 8, "WAVE", 4) == 0) {
			type = "GM_WAVE";
		}
		else if (count >= 4 && memcmp(buffer, "OggS", 4) == 0) {
			type = "GM_OGG";
		}
		else {
			type = "GM_UNKNOWN";
		}
	}
	else {
		txtr = true;
		type = "GM_PNG";
	}

	if (fseeko(res, 0, SEEK_SET) != 0) {
		perror(resname);
		goto error;
	}

	hdr = fopen(hdrname, "wb");
	if (!hdr) {
		perror(hdrname);
		goto error;
	}
	
	src = fopen(srcname, "wb");
	if (!src) {
		perror(srcname);
		goto error;
	}

	if (fprintf(src,
		"#include <stdint.h>\n"
		"\n"
		"const uint8_t %s[] = {\n\t",
		symname) < 0) {
		perror(srcname);
		goto error;
	}

	bool first = true;
	for (;;) {
		size_t count = fread(buffer, 1, sizeof(buffer), res);

		if (ferror(res)) {
			perror(resname);
			goto error;
		}

		if (count == 0) {
			break;
		}

		for (size_t lineoff = 0; lineoff < count; lineoff += LINE_LEN) {
			if (first) {
				first = false;
			}
			else if (fprintf(src, ",\n\t") < 0) {
				perror(srcname);
				goto error;
			}

			size_t lineend = count <= LINE_LEN || lineoff > count - LINE_LEN ?
				count : lineoff + LINE_LEN;
			if (fprintf(src, "0x%02x", buffer[lineoff]) < 0) {
				perror(srcname);
				goto error;
			}
			if (lineoff < lineend - 1) {
				for (size_t i = lineoff + 1; i < lineend; ++ i) {
					if (fprintf(src, ", 0x%02x", buffer[i]) < 0) {
						perror(srcname);
						goto error;
					}
				}
			}
		}

		size += count;
	}

	if (fprintf(src, "\n};\n") < 0) {
		perror(srcname);
		goto error;
	}

	if (fclose(src) != 0) {
		perror(srcname);
		src = NULL;
		goto error;
	}
	src = NULL;

	if (fprintf(hdr,
		"#ifndef %s_H\n"
		"#define %s_H\n"
		"#pragma once\n"
		"\n"
		"#include <stdint.h>\n"
		"\n"
		"#ifdef __cplusplus\n"
		"extern \"C\" {\n"
		"#endif\n"
		"\n"
		"#define %s_SIZE %" PRIuPTR "\n"
		"#define %s_TYPE %s\n",
		macroname, macroname, macroname, size, macroname, type) < 0) {
		perror(hdrname);
		goto error;
	}

	if (txtr && fprintf(hdr,
		"#define %s_WIDTH  %" PRIu32 "\n"
		"#define %s_HEIGHT %" PRIu32 "\n",
		macroname, info.width, macroname, info.height) < 0) {
		perror(hdrname);
		goto error;
	}

	if (fprintf(hdr,
		"extern const uint8_t %s[];\n"
//		"extern const size_t *%s_size;\n"
		"\n"
		"#ifdef __cplusplus\n"
		"}\n"
		"#endif\n"
		"\n"
		"#endif\n",
		symname) < 0) {
		perror(hdrname);
		goto error;
	}

	if (fclose(hdr) != 0) {
		perror(hdrname);
		hdr = NULL;
		goto error;
	}
	hdr = NULL;

	goto end;

error:
	status = EXIT_FAILURE;

end:

	if (macroname) {
		free(macroname);
		macroname = NULL;
	}

	if (res) {
		fclose(res);
		res = NULL;
	}

	if (hdr) {
		fclose(hdr);
		hdr = NULL;
	}

	if (src) {
		fclose(src);
		src = NULL;
	}

	return status;
}
