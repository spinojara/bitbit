#include "endgame.h"

static const int max_pieces[]     = { 0, 9, 11, 11, 11, 10 };
/* Initial call should have total = 1 to account for the king. */
static void generate_material(int (*pieces)[7], size_t *length, int *counts, int piece, int total, int pawns) {
	static const int initial_pieces[] = { 0, 0, 2, 2, 2, 1 };
	if (piece == KING) {
		for (int i = PAWN; i < KING; i++)
			pieces[*length][i] = counts[i];
		pieces[*length][KING] = 1;
		++*length;
		return;
	}

	for (int count = 0; count < max_pieces[piece]; count++) {
		int promoted = max(count - initial_pieces[piece], 0);
		if (total + count > 16 || pawns + promoted > 8)
			break;
		counts[piece] = count;
		generate_material(pieces, length, counts, piece + 1, total + count, pawns + promoted);
	}
}

static void material_position(struct position *pos, const int *white, const int *black) {
	const int *pieces[2]   = { white, black };
	pos->piece[WHITE][ALL] = pos->piece[BLACK][ALL] = 0;
	int total = 0;
	for (int color = 0; color < 2; color++) {
		for (int piece = PAWN; piece <= KING; piece++) {
			pos->piece[color][piece]   = (1 << pieces[color][piece]) - 1;
			/* Make sure the pieces are not overlapping each other, though this is not strictly necessary. */
			pos->piece[color][piece] <<= total;
			total                     += pieces[color][piece];
			pos->piece[color][ALL]    |= pos->piece[color][piece];
		}
	}
}

static void test_endgame_injectivity(void) {
	struct position pos = { 0 };
	size_t max_length = 1;
	for (int piece = PAWN; piece < KING; piece++)
		max_length *= max_pieces[piece];

	int (*pieces)[7] = malloc(max_length * sizeof(*pieces));
	size_t length = 0;
	int counts[7] = { 0 };
	generate_material(pieces, &length, counts, PAWN, 1, 0);

	size_t found = 0;

	for (size_t i = 0; i < length; i++) {
		for (size_t j = 0; j < length; j++) {
			material_position(&pos, pieces[i], pieces[j]);
			refresh_endgame_key(&pos);
			struct endgame *e = endgame_probe(&pos);
			/* Asserts verify_material. */
			if (e) {
				endgame_evaluate(e, &pos);
				found++;
			}
		}
	}

	free(pieces);

	/* Not included in KXK:
	 * KK
	 * KPK
	 * KNK
	 * KBK
	 *
	 * KPPK
	 * KNPK
	 * KBPK
	 * KNNK
	 * KBNK
	 *
	 * 14 assymetric endgames and 1 symmetric endgame (KPKP).
	 */
	size_t expected = 2 * (length - 9) + 2 * 14 + 1;
	CU_ASSERT_EQUAL(found, expected);
}
