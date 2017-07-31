#!/usr/bin/env python3

import os
import struct
import collections
from PIL import Image
from io import BytesIO
from os.path import join as pjoin
from game_maker import *

# RECONSTRUCT = {
# 	17: 'images/catering.png',
# 	42: 'images/icons.png',
# 	47: 'images/hoomans.png',
# }

def dump_sprites(fp, outdir):
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
	txtrs = []
	sprts = {}
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

				tpagptr = sprite_record[-2]
				fp.seek(tpagptr, 0)
				data = fp.read(22)
				tpag = struct.unpack('<HHHHHHHHHHH', data)

				txtr_index = tpag[-1]
				rect = tpag[:4]

				sprite_info = (sprite_name, rect)
				if txtr_index in sprts:
					sprts[txtr_index].append(sprite_info)
				else:
					sprts[txtr_index] = [sprite_info]

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

				tpagptr = bgnd_record[-1]
				fp.seek(tpagptr, 0)
				data = fp.read(22)
				tpag = struct.unpack('<HHHHHHHHHHH', data)

				txtr_index = tpag[-1]
				rect = tpag[:4]

				bgnd_info = (bgnd_name, rect)
				if txtr_index in sprts:
					sprts[txtr_index].append(bgnd_info)
				else:
					sprts[txtr_index] = [bgnd_info]

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

				if index in sprts:
					sprites = sprts[index]
					fp.seek(offset, 0)
					data = fp.read(info.filesize)

					image = Image.open(BytesIO(data))

					sprite_dir = pjoin(outdir, '%05d' % index)
					os.makedirs(sprite_dir, exist_ok=True)

					txtr_filename = pjoin(outdir, '%05d.png' % index)
					print(txtr_filename)
					with open(txtr_filename, 'wb') as outfp:
						outfp.write(data)

					# if index in RECONSTRUCT:
					# 	img = Image.open(RECONSTRUCT[index])
					# 	os.makedirs(pjoin('sprites', str(index)), exist_ok=True)
					# else:
					# 	img = None

					for sprite_name, rect in sprites:
						if sprite_name in seen_names:
							raise FileFormatError("Sprite double occurence: " + sprite_name)

						seen_names.add(sprite_name)
						sprite_filename = pjoin(sprite_dir, sprite_name + '.png')
						print(sprite_filename)

						x, y, width, height = rect
						box = (x, y, x + width, y + height)
						sprite = image.crop(box)
						sprite.save(sprite_filename)

						# if img:
						# 	sprite = img.crop(box)
						# 	sprite.save(pjoin('sprites', str(index), sprite_name + '.png'))

		fp.seek(next_offset, 0)

if __name__ == '__main__':
	import sys

	archive = sys.argv[1]
	outdir = sys.argv[2]
	with open(archive, 'rb') as fp:
		dump_sprites(fp, outdir)
