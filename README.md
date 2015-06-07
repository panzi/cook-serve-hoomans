Cook, Serve, Hoomans!
=====================

In [one of her streams](http://www.twitch.tv/feliciaday/v/4517425?t=02h19m22s)
Felicia Day said she wishes she could put the faces of her fans on the customers
in [Cook, Serve, Delicious!](http://store.steampowered.com/app/247020/), so I
made this "mod" that does just that.

I still need more pictures of hoomans (fans), so send them in! They don't need
to be photos if you rather just want your avatar or a drawing to be used. In any
case don't let your elbows stick out to the side so the picture will fit into
one of the available slots. Help from someone who is better with Photoshop/GIMP
would also be nice.

Setup
-----

Download the latest release from the [releases page](https://github.com/panzi/cook-serve-hoomans/releases)
and double click `cook_serve_hoomans.exe`. It should automatically find the
installation path of Cook, Serve, Delicious! and patch the game archive. If
everything went well you should see something like this:

![](http://panzi.github.io/cook-serve-hoomans/img/patch_success.png)

Just press enter and you are done.

To be on the safe side this patch creates a backup of the game archive (if none
exist already). The backup will be placed in the same folder as the game archive
(`data.win` on Windows and `game.unx` on Linux) and will be called
`data.win.backup` on Windows and `game.unx.backup` on Linux. If you want to
remove the patch simply delete `data.win`/`game.unx` and rename the backup file.

### In case that didn't work

In case `cook_serve_hoomans.exe` couldn't automatically find the game archive
file you can pass it manually. First find `data.win` from your Cook, Serve,
Delicious! installation. Under Windows `data.win` can usually be found at one of
these or similar locations:

```
C:\Program Files\Steam\steamapps\common\CookServeDelicious\data.win
C:\Program Files (x86)\Steam\steamapps\common\CookServeDelicious\data.win
```

Under Linux it would be:

```
~/.local/share/Steam/SteamApps/common/CookServeDelicious/assets/game.unx
~/.steam/steam/SteamApps/common/CookServeDelicious/assets/game.unx
```

I don't have a Mac so I don't know where it's there and I can't provide a binary
for Mac anyway. (I don't use Windows either, but it is easily possible to cross
compile a Windows binary under Linux.)

Then simply drag and drop `data.win` onto `cook_serve_hoomans.exe`:

![](http://panzi.github.io/cook-serve-hoomans/img/open_with_cook_serve_hoomans.png)

If everything went well you should see the same dialog as above. Just press enter
and you are done.

Advanced Usage
--------------

For advanced users there is a second binary called `quick_patch.exe`. You can
use this to patch `data.win` with your own customer and/or icon sprites. Just
drop `data.win`, `hoomans.png` and `icons.png` all together onto
`quick_patch.exe`. It is important that the files are named like this so the
program knows what to do with which file.

If you can handle the shell there are even more binaries: `gmdump.exe` and
`gmupdate.exe`. Use the first to dump all sprites and sound files from an
Game Maker archive into a directory. After you edited those files you can
use the second program to update the archive.

Build From Source
-----------------

(For advanced users and software developers only.)

In case you want to build this patching tool yourself download the source and
simply run these commands in the source folder:

```
make setup
make -j`nproc`
```

If you want to cross-compile for another platform you can run one of these
commands:

```
make TARGET=linux32
make TARGET=linux64
make TARGET=win32
make TARGET=win64
```

Always make sure that the folder `build/$TARGET` exists before you run `make`.
You can do this simply by running `make TARGET=$TARGET setup`.

Finally you can run the patch by typing:

```
make patch
```

How It Works
------------

Cook, Serve, Delicious! uses [Game Maker](http://www.yoyogames.com/studio) from
YoYo Games. At first I didn't bother reverse engineering the archive file format
of this game engine, but because the file size of the replacement sprite got
bigger than the file size of the original I had to reverse engineer at least a
bit so I could rewrite the archive properly (instead of just overwriting the
proportion of the archive containing the sprite).

This program understands the overall structure of Game Maker archives and the
detailed structure of the TXTR and AUDO sections. These are the last two
sections in the archive, so when the TXTR section needs resizing absolute
offsets in this section and the following section (AUDO) need to be adjusted.
There don't seem to be any offsets to parts of those sections in other places.
All I know about this file format is documented in [fileformat.txt](fileformat.txt).

I don't know what will happen when there is a game update. Will the updater
corrupt the archive, bail because the file isn't what it expects it to be or
simply revert the patch? Your guess is a sgood as mine. In any case this program
creates a `data.win.backup` file which you can use in case the game stops
working. Just remove `data.win` and rename `data.win.backup` to `data.win`.

Because Felicia uses Windows and one cannot assume the availability of any sane
scripting language (like Python) on an arbitrary Windows installation I wrote
this patch as a self contained C program. Cross compiling simple C/C++ programs
for Windows under Linux is easy.

However, it is very very cumbersome to cross compile stuff for Mac under Linux
(the setup still requires access to a Mac and an Apple developer account), so I
can't provide a Mac binary. I could rewrite everything in Python (Mac, just like
most Linux distributions, actually comes with Python pre-installed), but I only
do this if there is any demand for it.
