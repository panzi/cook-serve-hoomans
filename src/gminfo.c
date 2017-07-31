#include "game_maker.h"

#include <stdio.h>

static void gm_print_info(const struct gm_index *index, FILE *out) {
	fprintf(out, "Offset       Size             Type      Index Info\n");
	for (; index->section != GM_END; ++ index) {
		fprintf(out, "0x%010" PRIXPTR " 0x%010" PRIXPTR " --- %-9s -----",
			(size_t)index->offset,
			index->size,
			gm_section_name(index->section));

		switch (index->section) {
			case GM_AUDO:
			case GM_TXTR:
				fprintf(out, " %" PRIuPTR " entries\n", index->entry_count);
				for (size_t entry_index = 0; entry_index < index->entry_count; ++ entry_index) {
					const struct gm_entry *entry = &index->entries[entry_index];
					fprintf(out, "0x%010" PRIXPTR " 0x%010" PRIXPTR "     %-9s %5" PRIuPTR,
						(size_t)entry->offset,
						entry->size,
						gm_typename(entry->type),
						entry_index);

					if (index->section == GM_TXTR) {
						fprintf(out, " %4" PRIuPTR " x %-4" PRIuPTR,
							entry->meta.txtr.width,
							entry->meta.txtr.height);
					}
					fprintf(out, "\n");
				}
				break;

			default:
				fprintf(out, "\n");
				break;
		}
	}
}

int main(int argc, char *argv[]) {
	int status = 0;
	FILE *game = NULL;
	struct gm_index *index = NULL;
	const char *gamename = NULL;

	if (argc < 2) {
		fprintf(stderr, "*** usage: %s archive\n", argc < 1 ? "gminfo" : argv[0]);
		goto error;
	}

	gamename = argv[1];

	game = fopen(gamename, "rb");
	if (!game) {
		perror(gamename);
		goto error;
	}

	index = gm_read_index(game);
	if (!index) {
		perror(gamename);
		goto error;
	}

	gm_print_info(index, stdout);

	goto end;

error:
	status = 1;

end:
	if (game) {
		fclose(game);
		game = NULL;
	}

	if (index) {
		gm_free_index(index);
		index = NULL;
	}

#ifdef GM_WINDOWS
	printf("Press ENTER to continue...");
	getchar();
#endif

	return status;
}
