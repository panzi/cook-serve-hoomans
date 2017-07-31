#!/usr/bin/env python3

import os
import sys
import struct
import collections
from os.path import isdir, isfile, join as pjoin

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

def find_path_ignore_case(prefix, path, index):
	next_index = index + 1
	is_last = next_index == len(path)
	current = path[index].lower()

	for entry in os.listdir(prefix):
		if entry.lower() == current:
			entry_path = pjoin(prefix, entry)
			if is_last:
				if isfile(entry_path):
					return entry_path

			elif isdir(entry_path):
				try:
					return find_path_ignore_case(entry_path, path, next_index)
				except FileNotFoundError:
					pass

	raise FileNotFoundError('game archive not found')

if sys.platform == 'linux':
	linux_paths = [
		[".local/share", "Steam", "SteamApps", "common", "CookServeDelicious" ,"assets", "game.unx"],
		[".steam", "Steam", "SteamApps", "common", "CookServeDelicious", "assets", "game.unx"]
	]

	def find_archive():
		home = os.getenv('HOME')
		for path in linux_paths:
			try:
				return find_path_ignore_case(home, path, 0)
			except FileNotFoundError:
				pass

		raise FileNotFoundError('game archive not found')
else:
	def find_archive():
		raise ValueError('Auto find of the game archive is currently not supported on your system. Please pass the archive manually.')
