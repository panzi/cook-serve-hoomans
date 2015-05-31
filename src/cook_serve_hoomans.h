#ifndef COOK_SERVE_HOOMANS_H
#define COOK_SERVE_HOOMANS_H
#pragma once

#if defined(_WIN16) || defined(_WIN32) || defined(_WIN64)
#	define CSH_GAME_ARCHIVE "data.win"
#else
#	define CSH_GAME_ARCHIVE "game.unx"
#endif

#define CSH_ICONS_OFFSET 0x035c3000
#define CSH_ICONS_WIDTH  2048
#define CSH_ICONS_HEIGHT 2048
#define CSH_ICONS_SIZE   1110948
#define CSH_ICONS_INDEX  41

#define CSH_HOOMANS_OFFSET 0x03f7da80
#define CSH_HOOMANS_WIDTH  2048
#define CSH_HOOMANS_HEIGHT 1024
#define CSH_HOOMANS_SIZE   1820099
#define CSH_HOOMANS_INDEX  46

#endif
