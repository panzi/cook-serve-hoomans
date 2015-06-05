#include "png_info.h"

#include <string.h>
#include <errno.h>

#if defined(__linux__) || defined(__CYGWIN__)

#	include <endian.h>

#elif defined(__APPLE__)

#	include <libkern/OSByteOrder.h>

#	define htobe16 OSSwapHostToBigInt16
#	define htole16 OSSwapHostToLittleInt16
#	define be16toh OSSwapBigToHostInt16
#	define le16toh OSSwapLittleToHostInt16

#	define htobe32 OSSwapHostToBigInt32
#	define htole32 OSSwapHostToLittleInt32
#	define be32toh OSSwapBigToHostInt32
#	define le32toh OSSwapLittleToHostInt32

#	define htobe64 OSSwapHostToBigInt64
#	define htole64 OSSwapHostToLittleInt64
#	define be64toh OSSwapBigToHostInt64
#	define le64toh OSSwapLittleToHostInt64

#	define __BYTE_ORDER    BYTE_ORDER
#	define __BIG_ENDIAN    BIG_ENDIAN
#	define __LITTLE_ENDIAN LITTLE_ENDIAN
#	define __PDP_ENDIAN    PDP_ENDIAN

#elif defined(__OpenBSD__)

#	include <sys/endian.h>

#elif defined(__NetBSD__) || defined(__FreeBSD__) || defined(__DragonFly__)

#	include <sys/endian.h>

#	define be16toh betoh16
#	define le16toh letoh16

#	define be32toh betoh32
#	define le32toh letoh32

#	define be64toh betoh64
#	define le64toh letoh64

#elif defined(_WIN16) || defined(_WIN32) || defined(_WIN64)

#	if BYTE_ORDER == LITTLE_ENDIAN

#		define htobe16(x) __builtin_bswap16(x)
#		define htole16(x) (x)
#		define be16toh(x) __builtin_bswap16(x)
#		define le16toh(x) (x)

#		define htobe32(x) __builtin_bswap32(x)
#		define htole32(x) (x)
#		define be32toh(x) __builtin_bswap32(x)
#		define le32toh(x) (x)

#		define htobe64(x) __builtin_bswap64(x)
#		define htole64(x) (x)
#		define be64toh(x) __builtin_bswap64(x)
#		define le64toh(x) (x)

#	elif BYTE_ORDER == BIG_ENDIAN

		/* that would be xbox 360 */
#		define htobe16(x) (x)
#		define htole16(x) __builtin_bswap16(x)
#		define be16toh(x) (x)
#		define le16toh(x) __builtin_bswap16(x)

#		define htobe32(x) (x)
#		define htole32(x) __builtin_bswap32(x)
#		define be32toh(x) (x)
#		define le32toh(x) __builtin_bswap32(x)

#		define htobe64(x) (x)
#		define htole64(x) __builtin_bswap64(x)
#		define be64toh(x) (x)
#		define le64toh(x) __builtin_bswap64(x)

#	else

#		error platform byte order not supported

#	endif

#	define __BYTE_ORDER    BYTE_ORDER
#	define __BIG_ENDIAN    BIG_ENDIAN
#	define __LITTLE_ENDIAN LITTLE_ENDIAN
#	define __PDP_ENDIAN    PDP_ENDIAN

#else

#	error platform not supported

#endif

#define PNG_SIGNATURE "\x89PNG\r\n\x1a\n"
#define PNG_SIGNATURE_SIZE 8
#define PNG_IHDR_SIZE 25
#define PNG_IEND_SIZE 12
#define PNG_CHUNK_HEADER_SIZE 8

// See: http://www.libpng.org/pub/png/spec/1.2/PNG-Structure.html

#define IS_PNG_MAGIC_CHAR(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#define IS_PNG_CHUNK_MAGIC(m) \
	(IS_PNG_MAGIC_CHAR((m)[0]) && \
	 IS_PNG_MAGIC_CHAR((m)[1]) && \
	 IS_PNG_MAGIC_CHAR((m)[2]) && \
	 IS_PNG_MAGIC_CHAR((m)[3]))

#pragma pack(push, 1)
struct png_chunk_header {
	uint32_t size;
	uint8_t  magic[4];
//	uint8_t  data[size];
//	uint32_t crc;
};

struct png_ihdr_chunk {
	uint32_t size;
	uint8_t  magic[4];
	uint32_t width;
	uint32_t height;
	uint8_t  bitdepth;
	uint8_t  colortype;
	uint8_t  compression;
	uint8_t  filter;
	uint8_t  interlace;
	uint32_t crc;
};
#pragma pack(pop)

int parse_png_info(FILE *file, struct png_info *info) {
	size_t filesize = 0;
	char signature[PNG_SIGNATURE_SIZE];

	if (fread(signature, PNG_SIGNATURE_SIZE, 1, file) != 1) {
		return -1;
	}

	if (memcmp(signature, PNG_SIGNATURE, PNG_SIGNATURE_SIZE) != 0) {
		errno = EINVAL;
		return -1;
	}

	struct png_ihdr_chunk ihdr;

	if (fread(&ihdr, PNG_IHDR_SIZE, 1, file) != 1) {
		return -1;
	}

	ihdr.size   = be32toh(ihdr.size);
	ihdr.width  = be32toh(ihdr.width);
	ihdr.height = be32toh(ihdr.height);
	ihdr.crc    = be32toh(ihdr.crc);

	if (ihdr.size != 13) {
		errno = EINVAL;
		return -1;
	}

	if (memcmp(ihdr.magic, "IHDR", 4) != 0) {
		errno = EINVAL;
		return -1;
	}

	filesize = PNG_SIGNATURE_SIZE + PNG_IHDR_SIZE;
	
	if (ihdr.width > INT32_MAX || ihdr.height > INT32_MAX) {
		errno = EINVAL;
		return -1;
	}
	
	if (ihdr.bitdepth != 1 && ihdr.bitdepth != 2 &&
		ihdr.bitdepth != 4 && ihdr.bitdepth != 8 &&
		ihdr.bitdepth != 16) {
		errno = EINVAL;
		return -1;
	}
	
	if (ihdr.colortype != 0 && ihdr.colortype != 2 &&
		ihdr.colortype != 3 && ihdr.colortype != 4 &&
		ihdr.colortype != 6) {
		errno = EINVAL;
		return -1;
	}

	if (ihdr.compression != 0 || ihdr.filter != 0) {
		errno = EINVAL;
		return -1;
	}
	
	if (ihdr.interlace != 0 && ihdr.interlace != 1) {
		errno = EINVAL;
		return -1;
	}

	for (;;) {
		struct png_chunk_header chunk_header;

		if (fread(&chunk_header, PNG_CHUNK_HEADER_SIZE, 1, file) != 1) {
			return -1;
		}

		chunk_header.size = be32toh(chunk_header.size);

		if (!IS_PNG_CHUNK_MAGIC(chunk_header.magic)) {
			errno = EINVAL;
			return -1;
		}

		// overflow check, 12 = sizeof(size + magic + crc)
		if (filesize          > (SIZE_MAX - 12) ||
			chunk_header.size > (SIZE_MAX - 12 - filesize)) {
			errno = EINVAL;
			return -1;
		}
		filesize += chunk_header.size + 12;

		if (fseeko(file, chunk_header.size + 4, SEEK_CUR) != 0) {
			return -1;
		}

		if (memcmp(chunk_header.magic, "IEND", 4) == 0) {
			errno = EINVAL;
			break;
		}
	}

	if (info) {
		info->filesize    = filesize;
		info->width       = ihdr.width;
		info->height      = ihdr.height;
		info->bitdepth    = ihdr.bitdepth;
		info->colortype   = ihdr.colortype;
		info->compression = ihdr.compression;
		info->filter      = ihdr.filter;
		info->interlace   = ihdr.interlace;
	}

	return 0;
}
