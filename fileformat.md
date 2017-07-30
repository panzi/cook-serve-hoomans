Game Maker Archive File Format
==============================

This document describes parts of the Game Maker archive format as reverse
engineered from the game data shipped with Cook, Serve, Delicious! All of this
was reverse engineered using a hex editor and Python. No code was analyzed.

Because this is all from one game I don't actually know if there are any game
specific parts or if this applies generally to all Game Maker games.

Overall Structure
-----------------

Byte-order: Little-Endian

    ┌──────────────────────────────────────────┐
    │                                          │
    │  Root Chunk Magic: "FORM"                │
    │  Root Chunk Content Size                 │
    │                                          │
    │  ┌────────────────────────────────────┐  │
    │  │                                    │  │
    │  │  Chunk Magic                       │  │
    │  │  Chunk Content Size                │  │
    │  │                                    │  │
    │  │  ┌──────────────────────────────┐  │  │
    │  │  │                              │  │  │
    │  │  │  (Content)                   │  │  │
    │  │  │                              │  │  │
    │  │  └──────────────────────────────┘  │  │
    │  ├────────────────────────────────────┤  │
    │  │                                    │  │
    │  │  ...                               │  │
    │  │                                    │  │
    │  └────────────────────────────────────┘  │
    │                                          │
    └──────────────────────────────────────────┘

This looks like each chunk where self-contained, but there are absolute offsets
in many chunks, some even pointing into other chunks.

Obviously this file format is somewhat based on IFF, except it uses
little-endian encoding of integers instead big-endian.

Chunks
------

Basic chunk layout:

    Offset  Size  Type        Description
         0     4  char[4]     chunk magic
         4     4  uint32_t    chunk content size (N)
         8     N  uint8_t[N]  content

This means the overall size of a chuck is N + 8.
         
Chunk hierarchy as found in the Cook, Serve, Delicious! archive (data.win or
game.unx).
         
 * FORM
   - GEN8
   - OPTN
   - EXTN
   - SOND
   - SPRT
   - BGND
   - PATH
   - SCPT
   - SHDR
   - FONT
   - TMLN
   - OBJT
   - ROOM
   - DAFL
   - TPAG
   - CODE
   - VARI
   - FUNC
   - STRG
   - TXTR
   - AUDO
   - LANG
   - GLOB

### FORM

Main chunk spanning the whole file, containing all the other chunks. None of the
other chunks seem to contain sub-chunks.

### GEN8

Some meta data about the game archive. These are the values from game.unx. Some
of the values are obviously absolute offsets pointing into the string table
(STRG).

     Offset  Size  Type               Value
          0     4  ?                   3329
          4     4  uint32_t        15029373 -> "CSDSteamBuild"
          8     4  uint32_t        15029391 -> "Default"
         12     4  ?                 100006
         16     4  ?               10000000
         20     4  ?              586952641
         24     4  ?                      0
         28     4  ?                      0
         32     4  ?                      0
         36     4  ?                      0
         40     4  uint32_t        15029373 -> "CSDSteamBuild"
         44     4  ?                      1
         48     4  ?                      0
         52     4  ?                      0
         56     4  ?                   1443
         60     4  uint32_t     width (1280)
         64     4  uint32_t     height (720)
         68     4  ?                   2558
         72     4  ?             1557395764
         76     4  ?              526147823
         80     4  ?             2425561599
         84     4  ?              640346564
         88     4  ?              231427924
         92     4  ?             1429203362
         96     4  ?                      0
        100     4  uint32_t        15029403 -> "Cook, Serve, Delicious!"
        104     4  ?                      0
        108     4  ?                      0
        112     4  ?                      4
        116     4  ?                      0
        120     4  ?             4294720276
        124     4  ?                      5
        128     4  ?                      0
        132     4  ?                      1
        136     4  ?                      2
        140     4  ?                      3
        144     4  ?                      4

### STRG

String table. Other chunks point right into this one whenever they refer to a
simple string (so whole scripts aren't stored in this, only identifier names and
such).

     Offset  Size  Type         Description
          0     4  char[4]      chunk magic: 'STRG'
          4     4  uint32_t     chunk content size
          8     4  uint32_t     number of strings (N)
         12   4*N  uint32_t[N]  absolute offsets of strings
     12+4*N     ?  String[N]    strings

Because of the absolute offsets I wouldn't expect the strings to actually start
right after the offset table.
    
#### String

Absolute offsets to strings in other chunks point directly to the characters
(not to the string length).

     Offset  Size  Type         Description
          0     4  uint32_t     string length (N) excluding the terminating NULL
          4     N  char[N]      characters
        4+N     1  char         '\0'

### SPRT

Sprite info.

    Offset  Size  Type         Description
         0     4  char[4]      chunk magic: 'SPRT'
         4     4  uint32_t     chunk body size
         8     4  uint32_t     number of sprites (N)
        12   4*N  uint32_t[N]  sprite offsets

#### Sprite

    Offset  Size  Type         Description
         0     4  uint32_t     sprite name (offset into STRG)
         4    60  ?            ?
        64     4  uint32_t     offset into TPAG
        68     4  uint32_t     always(?) 1
        71     ?  ?            some junk that might be a bitmap? alpha map?
                               lots of 0xFF, some 0x00, a few other values

### TPAG

Contains coordinates for sprites in textures files.

    Offset  Size  Type         Description
         0     4  char[4]      chunk magic: 'TPAG'
         4     4  uint32_t     chunk body size
         8     4  uint32_t     number of records (N)
        12   4*N  uint32_t[N]  record offsets

#### Record

    Offset  Size  Type         Description
         0     2  uint16_t     x
         2     2  uint16_t     y
         4     2  uint16_t     width
         6     2  uint16_t     height
         8     2  uint16_t     ? (mostly 0)
        10     2  uint16_t     ? (mostly 0)
        12     2  uint16_t     width again?
        14     2  uint16_t     height again?
        16     2  uint16_t     width again??
        18     2  uint16_t     height again??
        20     2  uint16_t     TXTR index

### TXTR

I brute-force searched all the other chunks and haven't found any values that
are offsets to any file data chunks in this chunk.

     Offset  Size  Type         Description
          0     4  char[4]      chunk magic: 'TXTR'
          4     4  uint32_t     chunk content size
          8     4  uint32_t     number of textures (N)
         12   4*N  uint32_t[N]  absolute FileInfo offsets
     12+8*N   8*N  FileInfo[N]  FileInfos
    12+12*N     ?  N/A          zero padding (35 bytes in this case)
          ?     ?  uint8_t[?]   file data

#### FileInfo

     Offset  Size  Type         Description
          0     4  uint32_t     always 1 in Cook, Serve, Delicious!
                                saw 0 in another game
                                (maybe a file type? flags?)
          4     4  uint32_t     absolute offset of file data

There is no information about the size of the texture files anywhere in the
archive file format. But all of the textures in Cook, Serve, Delicious! are PNG
files and thus it is easy to parse them out of the archive. A PNG file has a
certain signature and then consists of chunks, each containing a chunk-magic and
the last having the magic "IEND". See more about the PNG file format here:
http://www.w3.org/TR/PNG-Structure.html
http://www.w3.org/TR/PNG/

### AUDO

I brute-force searched all the other chunks and haven't found any values that
are offsets to any file data chunks in this chunk.

     Offset  Size  Type         Description
          0     4  char[4]      chunk magic: 'AUDO'
          4     4  uint32_t     chunk content size
          8     4  uint32_t     number of audio files (N)
         12   4*N  uint32_t[N]  absolute AudioFile offsets
     12+4*N     ?  AudioFile[N] AudioFiles

#### AudioFile

     Offset  Size  Type         Description
          0     4  uint32_t     size (N)
          4     N  uint8_t[N]   file data

The only audio file formats found in the Cook, Serve, Delicious! game archive
where RIFF WAVE and Ogg Vorbis.
