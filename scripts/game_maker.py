#!/usr/bin/env python3

import os
import struct
import platform
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

class RIFFInfo(collections.namedtuple("RIFFInfo",["filesize","size","metatype","formtype"])):
	@property
	def what(self):
		return self.metatype

	@property
	def details(self):
		return self.formtype

class OGGInfo(collections.namedtuple("OGGInfo",["filesize"])):
	@property
	def what(self):
		return "Ogg"

	@property
	def details(self):
		return ""

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

def parse_riff_info(fp):
	data = fp.read(12)
	metatype, size, formtype = struct.unpack("<4sI4s",data)

	if metatype != b'RIFF':
		raise FileFormatError("not a RIFF file: %r" % metatype)

	if not formtype.isalnum():
		raise FileFormatError("illegal form type: %r" % formtype)

	return RIFFInfo(size + 8, size, metatype.decode(), formtype.decode())

def parse_ogg_info(fp):
	filesize = 0
	last_pageno = -1

	while True:
		offset = fp.tell()
		hdr = fp.read(27)
		magic, stream_version, type_flags, abs_g_pos, ser_strno, pageno, crc, nsegs = struct.unpack("<4sBBQIIIB",hdr)
		if magic != b'OggS':
			if filesize == 0:
				raise FileFormatError("not an Ogg file: %r" % magic)
			else:
				fp.seek(offset,0)
				break
		if pageno <= last_pageno:
			fp.seek(offset,0)
			break
		else:
			last_pageno = pageno
		segs = fp.read(nsegs)
		if len(segs) != nsegs:
			raise FileFormatError("truncated Ogg file")
		segs_end = 0
		for seg in segs:
			segs_end += seg
		filesize += 27 + nsegs + segs_end
		fp.seek(segs_end, 1)

	return OGGInfo(filesize)

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

if platform.system() == 'Linux':

	def find_archive():
		linux_paths = [
			[".local/share", "Steam", "SteamApps", "common", "CookServeDelicious" ,"assets", "game.unx"],
			[".steam", "Steam", "SteamApps", "common", "CookServeDelicious", "assets", "game.unx"]
		]

		home = os.getenv('HOME')
		for path in linux_paths:
			try:
				return find_path_ignore_case(home, path, 0)
			except FileNotFoundError:
				pass

		raise FileNotFoundError('game archive not found')

elif platform.system() == 'Darwin':

	def find_archive():
		path = pjoin(os.getenv("HOME"), "Library/Application Support/Steam/SteamApps/common/CookServeDelicious/Cook Serve Delicious.app/Contents/Resources/game.ios")
		if isfile(path):
			return path

		path = "/Applications/Cook Serve Delicious.app/Contents/Resources/game.ios"
		if isfile(path):
			return path

		raise FileNotFoundError('game archive not found')

elif platform.system() == 'Windows' or platform.system().startswith('CYGWIN'):

	from winreg import HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER, REG_SZ, KEY_QUERY_VALUE, OpenKeyEx, QueryValueEx, CloseKey

	def find_archive():
		reg_keys = [
			# Have confirmed sigthings of these keys:
			( HKEY_LOCAL_MACHINE, "Software\\Valve\\Steam",              "InstallPath" ),
			( HKEY_LOCAL_MACHINE, "Software\\Wow6432node\\Valve\\Steam", "InstallPath" ),
			( HKEY_CURRENT_USER,  "Software\\Valve\\Steam",              "SteamPath"   ),

			# All the other possible combination, just to try everything:
			( HKEY_CURRENT_USER,  "Software\\Wow6432node\\Valve\\Steam", "SteamPath"   ),
			( HKEY_LOCAL_MACHINE, "Software\\Valve\\Steam",              "SteamPath"   ),
			( HKEY_LOCAL_MACHINE, "Software\\Wow6432node\\Valve\\Steam", "SteamPath"   ),
			( HKEY_CURRENT_USER,  "Software\\Valve\\Steam",              "InstallPath" ),
			( HKEY_CURRENT_USER,  "Software\\Wow6432node\\Valve\\Steam", "InstallPath" ),
		]

		for key, subkey, valkey in reg_keys:
			try:
				hkey = OpenKeyEx(key, subkey, 0, KEY_QUERY_VALUE)
				try:
					value, valtype = QueryValueEx(hkey, valkey)
					if valtype == REG_SZ:
						return pjoin(value, "steamapps\\common\\CookServeDelicious\\data.win")
				finally:
					CloseKey(hkey)
			except FileNotFoundError:
				pass

		raise FileNotFoundError('game archive not found')

else:

	def find_archive():
		raise NotImplementedError('Auto find of the game archive is not supported on your system. Please pass the archive manually.')

if __name__ == '__main__':
	print(find_archive())
