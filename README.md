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

Download the latest release from the
[releases page](https://github.com/panzi/cook-serve-hoomans/releases). Find
`game.unx` from your Cook, Serve, Delicious! installation. Under Windows
`game.unx` can usually be found at one of these or similar locations:

```
C:\Program Files\Steam\steamapps\common\CookServeDelicious\assets\game.unx
C:\Program Files (x86)\Steam\steamapps\common\CookServeDelicious\assets\game.unx
```

Under Linux it would be:

```
~/.local/share/Steam/SteamApps/common/CookServeDelicious/assets/game.unx
~/.steam/steam/SteamApps/common/CookServeDelicious/assets/game.unx
```

I don't have a Mac so I don't know where it's there and I can't provide a binary
for Mac anyway. (I don't use Windows either, but it is easily possible to cross
compile for Windows under Linux.)

**Make a backup of `game.unx`!** `cook_serve_hoomans.exe` will patch `game.unx`.
It might (but shouldn't) corrupt the game, so please make a backup first! If
you have patched `game.unx` once already copy the backup back into the assets
folder before patching. Otherwise patching might fail if the new replacement
sprite image is bigger than the old (the replacement sprite should still be
smaller than the original image, that's why restoring the backup is needed).

Then simply drag and drop `game.unx` onto `cook_serve_hoomans.exe`:

![](http://i.imgur.com/6FMEtPd.png)

If everything went well you should see this:

![](http://i.imgur.com/KJ1bFIg.png)

Just press enter and you are done.

Advanced Usage
--------------

For advanced users there is a second binary called `quick_patch.exe`. You can
use this to patch `game.unx` witch your own customer and/or icon sprites. Just
do the same as under [Setup](#setup), but drop `game.unx`, `hoomans.png` and
`icons.png` all together onto `quick_patch.exe`. It is important that the files
are named like this so the program knows what to do with which file.

Build From Source
-----------------

(For advanced users and software developers only.)

In case you want to build this patching tool yourself download the source and
simply run these commands in the source folder:

```
mkdir -p "build/`uname|tr '[A-Z]' '[a-z]'``getconf LONG_BIT`"
make
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

How It Works
------------

Cook, Serve, Delicious! uses [Game Maker](http://www.yoyogames.com/studio) from
YoYo Games. I didn't bother to reverse engineer the archive file format of this
game engine, but instead I just used [another program](https://github.com/panzi/mediaextract)
I once wrote to extract all the PNG images contained in the `game.unx` archive
file. I just guessed that this game uses PNGs and I guessed right. I quickly
found the image containing all the customer sprites.

Now how do I replace this image without understanding the archive format? I just
edited the image (inserted a bad picture of myself as a test, a better picture
will follow) and wrote it over the existing image in the archive file. This is
only possible if the new image has the same or a smaller file size than the old
one. Otherwise other proportions of the archive would be overwritten and the
archive would be corrupted. Luckily the original image seems to be not very well
compressed and I could generate a replacement image that is smaller even though
it contains more detail (in general photos have more details than cartoon
characters and thus compress not so well).

This means that this patch will stop working if there is an update that changes
the offset of the image or reduces it's size too much. Then I will have to
search for the sprite image again and adapt my patch.

However, I built some safety mechanisms into the program to prevent archive
corruption: Before it writes the new image into the archive it first checks if
there is already a PNG file at the expected offset and if that file is big
enough (file size) and has the right dimensions (pixel width and height).

Because Felicia uses Windows and one cannot assume the availability of any sane
scripting language (like Python) on an arbitrary Windows installation I simply
wrote a very simple self contained C program that writes the image into the
file it gets passed as its first argument. Cross compiling simple C/C++ programs
for Windows under Linux is easy.

However, it is very very cumbersome to cross compile stuff for Mac under Linux
(the setup still requires access to a Mac and an Apple developer account), so I
can't provide a Mac binary. I could rewrite everything in Python (Mac, just like
most Linux distributions, actually comes with Python pre-installed), but I only
do this if there is any demand for it.
