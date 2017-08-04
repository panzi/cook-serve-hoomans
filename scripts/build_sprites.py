#!/usr/bin/env python3

import os
import sys
from PIL import Image
from io import BytesIO
from os.path import splitext, join as pjoin
from game_maker import *

def build_sprites(fp, spritedir, builddir):
	fp.seek(0, 2)
	file_size = fp.tell()
	fp.seek(0, 0)

	head = fp.read(8)
	magic, size = struct.unpack("<4sI", head)
	if magic != b'FORM':
		raise ValueError("illegal file magic: %r" % magic)

	expected = size + 8
	if expected < file_size:
		raise ValueError("file size underflow: file size = %d, read size = %d" % (
			file_size, expected))

	elif expected > file_size:
		raise ValueError("file size overflow: file size = %d, read size = %d" % (
			file_size, expected))

	end_offset = fp.tell() + size

	replacement_sprites = {}
	for prefix, dirs, files in os.walk(spritedir):
		for filename in files:
			sprite_name = splitext(filename)[0]
			replacement_sprites[sprite_name] = pjoin(prefix, filename)

	replacement_sprites_by_txtr = {}
	replacement_txtrs = {}
	while fp.tell() < end_offset:
		head = fp.read(8)
		magic, size = struct.unpack("<4sI", head)
		next_offset = fp.tell() + size

		if magic == b'SPRT':
			sprite_count, = struct.unpack("<I", fp.read(4))
			data = fp.read(4 * sprite_count)
			sprite_offsets = struct.unpack("<%dI" % sprite_count, data)

			for offset in sprite_offsets:
				fp.seek(offset, 0)
				data = fp.read(4 * 17)
				sprite_record = struct.unpack('<IIIIIIIIIIIIIIIII', data)

				strptr = sprite_record[0]
				fp.seek(strptr - 4, 0)
				strlen, = struct.unpack('<I', fp.read(4))
				sprite_name = fp.read(strlen).decode()

				if sprite_name in replacement_sprites:
					tpagptr = sprite_record[-2]
					fp.seek(tpagptr, 0)
					data = fp.read(22)
					tpag = struct.unpack('<HHHHHHHHHHH', data)

					txtr_index = tpag[-1]
					rect = tpag[:4]

					sprite_info = (sprite_name, rect)
					if txtr_index in replacement_sprites_by_txtr:
						replacement_sprites_by_txtr[txtr_index].append(sprite_info)
					else:
						replacement_sprites_by_txtr[txtr_index] = [sprite_info]

		elif magic == b'BGND':
			bgnd_count, = struct.unpack("<I", fp.read(4))
			data = fp.read(4 * bgnd_count)
			bgnd_offsets = struct.unpack("<%dI" % bgnd_count, data)

			for offset in bgnd_offsets:
				fp.seek(offset, 0)
				data = fp.read(4 * 5)
				bgnd_record = struct.unpack('<IIIII', data)

				strptr = bgnd_record[0]
				fp.seek(strptr - 4, 0)
				strlen, = struct.unpack('<I', fp.read(4))
				bgnd_name = fp.read(strlen).decode()

				if bgnd_name in replacement_sprites:
					tpagptr = bgnd_record[-1]
					fp.seek(tpagptr, 0)
					data = fp.read(22)
					tpag = struct.unpack('<HHHHHHHHHHH', data)

					txtr_index = tpag[-1]
					rect = tpag[:4]

					bgnd_info = (bgnd_name, rect)
					if txtr_index in replacement_sprites_by_txtr:
						replacement_sprites_by_txtr[txtr_index].append(bgnd_info)
					else:
						replacement_sprites_by_txtr[txtr_index] = [bgnd_info]

		elif magic == b'TXTR':
			start_offset = fp.tell()
			count, = struct.unpack("<I", fp.read(4))
			data = fp.read(4 * count)
			info_offsets = struct.unpack("<%dI" % count, data)
			file_infos = []

			for offset in info_offsets:
				if offset < start_offset or offset + 8 > end_offset:
					raise FileFormatError("illegal TXTR info offset: %d" % offset)

				fp.seek(offset, 0)
				data = fp.read(8)
				info = struct.unpack("<II",data)
				file_infos.append(info)

			seen_names = set()
			for index, (unknown, offset) in enumerate(file_infos):
				if offset < start_offset or offset > end_offset:
					raise FileFormatError("illegal TXTR data offset: %d" % offset)

				fp.seek(offset, 0)
				info = parse_png_info(fp)

				if offset + info.filesize > end_offset:
					raise FileFormatError("PNG file at offset %d overflows TXTR data section end" % offset)

				if index in replacement_sprites_by_txtr:
					sprites = replacement_sprites_by_txtr[index]
					fp.seek(offset, 0)
					data = fp.read(info.filesize)

					image = Image.open(BytesIO(data))

					for sprite_name, (x, y, width, height) in sprites:
						if sprite_name in seen_names:
							raise FileFormatError("Sprite double occurence: " + sprite_name)

						seen_names.add(sprite_name)

						sprite = Image.open(replacement_sprites[sprite_name])
						if sprite.size != (width, height):
							raise FileFormatError("Sprite %s has incompatible size. PNG size: %d x %d, size in game archive: %d x %d" %
								(sprite_name, sprite.size[0], sprite.size[1], width, height))

						image.paste(sprite, box=(x, y, x + width, y + height))

					# DEBUG:
					# image.save(pjoin(builddir, '%05d.png' % index))

					buf = BytesIO()
					image.save(buf, format='PNG')
					replacement_txtrs[index] = (image.size, buf.getvalue())

		fp.seek(next_offset, 0)

	patch_def = []
	patch_data_externs = []
	patch_data_c = []

	for index, ((width, height), data) in replacement_txtrs.items():
		patch_data_externs.append('extern const uint8_t csh_%05d_data[];' % index)
		patch_def.append("GM_PATCH_TXTR(%d, csh_%05d_data, %d, %d, %d)" % (index, index, len(data), width, height))
		data_filename = 'csh_%05d_data.c' % index

		hex_data = ',\n\t'.join(', '.join('0x%02x' % byte for byte in data[i:i+8]) for i in range(0,len(data),8))

		data_c = """\
#include <stdint.h>

const uint8_t csh_%05d_data[] = {
	%s
};
""" % (index, hex_data)

		out_filename = pjoin(builddir, data_filename)
		print(out_filename)
		with open(out_filename, 'w') as outfp:
			outfp.write(data_c)

	patch_def_h = """\
#ifndef CSH_PATCH_DEF_H
#define CSH_PATCH_DEF_H
#pragma once

#include <stdint.h>
#include "game_maker.h"

#ifdef __cplusplus
extern "C" {
#endif

%s
extern const struct gm_patch csh_patches[];

#ifdef __cplusplus
}
#endif

#endif
""" % '\n'.join(patch_data_externs)

	out_filename = pjoin(builddir, 'csh_patch_def.h')
	print(out_filename)
	with open(out_filename, 'w') as outfp:
		outfp.write(patch_def_h)

	patch_def_c = """\
#include "csh_patch_def.h"

const struct gm_patch csh_patches[] = {
	%s,
	GM_PATCH_END
};
""" % ',\n\t'.join(patch_def)

	out_filename = pjoin(builddir, 'csh_patch_def.c')
	print(out_filename)
	with open(out_filename, 'w') as outfp:
		outfp.write(patch_def_c)

if __name__ == '__main__':
	spritedir = sys.argv[1]
	builddir  = sys.argv[2]

	if len(sys.argv) > 3:
		archive = sys.argv[3]
	else:
		archive = find_archive()

	with open(archive, 'rb') as fp:
		build_sprites(fp, spritedir, builddir)
