#include "patch_game.h"
#include "png_info.h"

#include <stdint.h>
#include <string.h>

int patch_game(FILE *file, off_t offset, const unsigned char *data, size_t size, size_t width, size_t height) {
	if (fseeko(file, offset, SEEK_SET) != 0) {
		perror("*** ERROR: seeking to offset of image");
		return -1;
	}

	struct png_info info;
	if (parse_png_info(file, &info) != 0) {
		fprintf(stderr, "*** ERROR: No PNG image found at expected offset.\n");
		return -1;
	}

	if (info.filesize < size) {
		fprintf(stderr,
			"*** ERROR: Not enough space to embed the file.\n"
			"           Need %" PRIuPTR " bytes but only have %" PRIuPTR ".\n",
			size, info.filesize);
		return -1;
	}

	if (info.width != width || info.height != height) {
		fprintf(stderr,
			"*** ERROR: Image to be replaced has wrong dimensions.\n"
			"           Existing image size: %" PRIu32 "x%" PRIu32 "\n"
			"           New image size: %" PRIuPTR "x%" PRIuPTR "\n",
			info.width, info.height, width, height);
		return -1;
	}

	// parse_png changed file position
	if (fseeko(file, offset, SEEK_SET) != 0) {
		perror("*** ERROR: seeking to offset of image");
		return -1;
	}

	if (fwrite(data, size, 1, file) != 1) {
		perror("*** ERROR: patching game");
		return -1;
	}

	// zero padding
	for (; size < info.filesize; ++ size) {
		if (fwrite("", 1, 1, file) != 1) {
			perror("*** ERROR: patching game");
			return -1;
		}
	}

	return 0;
}
