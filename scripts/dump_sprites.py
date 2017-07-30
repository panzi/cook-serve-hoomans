#!/usr/bin/env python3

import struct
import collections
from PIL import Image
from io import BytesIO
from os.path import join as pjoin

PNG_SIG = b'\x89PNG\r\n\x1a\n'

class FileFormatError(ValueError):
	pass

class PNGInfo(collections.namedtuple("PNGInfo",[
			"filesize", "size", "magic", "width", "height", "bitdepth", "colortype",
			"compression", "filter", "interlace", "crc"
		])):
	@property
	def what(self):
		return 'PNG'

	@property
	def details(self):
		return "%dx%d" % (self.width, self.height)

def parse_png_info(fp):
	sig = fp.read(8)
	if sig != PNG_SIG:
		raise FileFormatError("not a PNG file: %r" % sig)
	hdr = fp.read(25)
	size, magic, width, height, bitdepth, colortype, compression, \
	filter, interlace, crc = struct.unpack(">I4sIIBBBBBI",hdr)

	if magic != b'IHDR':
		raise FileFormatError("expected IHDR chunk but got: %r" % magic)

	filesize = 8 + 25

	if bitdepth != 1 and bitdepth != 2 and bitdepth != 4 and bitdepth != 8 and bitdepth != 16:
		raise FileFormatError("unexpected bitdepth value: %d" % bitdepth)

	if colortype != 0 and colortype != 2 and colortype != 3 and colortype != 4 and colortype != 6:
		raise FileFormatError("unexpected colortype value: %d" % colortype)

	if compression != 0 and filter != 0:
		raise FileFormatError("filter and compression are both non-zero")

	if interlace != 0 and interlace != 1:
		raise FileFormatError("unexpected interlace value: %d" % interlace)

	while True:
		data = fp.read(8)
		chunk_size, chunk_magic = struct.unpack(">I4s",data)
		if not chunk_magic.isalpha():
			raise FileFormatError("unexpected chunk magic: %r" % chunk_magic)

		filesize += chunk_size + 12
		fp.seek(chunk_size + 4, 1)

		if chunk_magic == b'IEND':
			break

	return PNGInfo(filesize, size, magic, width, height, bitdepth, colortype,
		compression, filter, interlace, crc)

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

					for sprite_name, rect in sprites:
						if sprite_name in seen_names:
							raise FileFormatError("Sprite double occurence: " + sprite_name)

						seen_names.add(sprite_name)
						sprite_filename = pjoin(outdir, sprite_name + '.png')
						print(sprite_filename)
						x, y, width, height = rect
						sprite = image.crop((x, y, x + width, y + height))
						sprite.save(sprite_filename)

		fp.seek(next_offset, 0)

if __name__ == '__main__':
	import sys

	archive = sys.argv[1]
	outdir = sys.argv[2]
	with open(archive, 'rb') as fp:
		dump_sprites(fp, outdir)
