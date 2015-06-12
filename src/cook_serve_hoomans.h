#ifndef COOK_SERVE_HOOMANS_H
#define COOK_SERVE_HOOMANS_H
#pragma once

#if defined(_WIN16) || defined(_WIN32) || defined(_WIN64)
#	define CSH_GAME_ARCHIVE "data.win"
#else
#	define CSH_GAME_ARCHIVE "game.unx"
#endif

#define CSH_CATERING_INDEX 16
#define CSH_ICONS_INDEX    41
#define CSH_HOOMANS_INDEX  46

#endif
