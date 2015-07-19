#include <math.h>
#ifndef PI
	#define PI (3.141592653589793f)
#endif

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "jpeg.h"
#include "helper.h"

static const int dc_base_code[12] = { 0x2, 0x3, 0x4, 0x0, 0x5, 0x6, 0xE, 0x1E, 0x3E, 0x7E, 0xFE, 0x1FE };
static const int dc_base_code_len[12] = { 3, 3, 3, 2, 3, 3, 4, 5, 6, 7, 8, 9 };

static const int ac_base_code[16][10] = {
	{ 0x0, 0x1, 0x4, 0xB, 0x1A, 0x38, 0x78, 0x3F6, 0xFF82, 0xFF83 },
	{ 0xC, 0x39, 0x79, 0x1F6, 0x7F6, 0xFF84, 0xFF85, 0xFF86, 0xFF87, 0xFF88 },
	{ 0x1B, 0xF8, 0x3F7, 0xFF89, 0xFF8A, 0xFF8B, 0xFF8C, 0xFF8D, 0xFF8E, 0xFF8F },
	{ 0x3A, 0x1F7, 0x7F7, 0xFF90, 0xFF91, 0xFF92, 0xFF93, 0xFF94, 0xFF95, 0xFF96 },
	{ 0x3B, 0x3F8, 0xFF97, 0xFF98, 0xFF99, 0xFF9A, 0xFF9B, 0xFF9C, 0xFF9D, 0xFF9E },
	{ 0x7A, 0x3F9, 0xFF0F, 0xFFA0, 0xFFA1, 0xFFA2, 0xFFA3, 0xFFA4, 0xFFA5, 0xFFA6 },
	{ 0x7B, 0x7F8, 0xFFA7, 0xFFA8, 0xFFA9, 0xFFAA, 0xFFAB, 0xFFAC, 0xFFAD, 0xFFAE },
	{ 0xF9, 0x7F9, 0xFFAF, 0xFFB0, 0xFFB1, 0xFFB2, 0xFFB3, 0xFFB4, 0xFFB5, 0xFFB6 },
	{ 0xFA, 0x7FC0, 0xFFB7, 0xFFB8, 0xFFB9, 0xFFBA, 0xFFBB, 0xFFBC, 0xFFBD, 0xFFBE },
	{ 0x1F8, 0xFFBF, 0xFFC0, 0xFFC1, 0xFFC2, 0xFFC3, 0xFFC4, 0xFFC5, 0xFFC6, 0xFFC7 },
	{ 0x1F9, 0xFFC8, 0xFFC9, 0xFFCA, 0xFFCB, 0xFFCC, 0xFFCD, 0xFFCE, 0xFFCF, 0xFFD0 },
	{ 0x1FA, 0xFFD1, 0xFFD2, 0xFFD3, 0xFFD4, 0xFFD5, 0xFFD6, 0xFFD7, 0xFFD8, 0xFFD9 },
	{ 0x3FA, 0xFFDA, 0xFFDB, 0xFFDC, 0xFFDD, 0xFFDE, 0xFFDF, 0xFFE0, 0xFFE1, 0xFFE2 },
	{ 0x7FA, 0xFFE3, 0xFFE4, 0xFFE5, 0xFFE6, 0xFFE7, 0xFFE8, 0xFFE9, 0xFFEA, 0xFFEB },
	{ 0xFF6, 0xFFEC, 0xFFED, 0xFFEE, 0xFFEF, 0xFFF0, 0xFFF1, 0xFFF2, 0xFFF3, 0xFFF4 },
	{ 0xFFF5, 0xFFF6, 0xFFF7, 0xFFF8, 0xFFF9, 0xFFFA, 0xFFFB, 0xFFFC, 0xFFFD, 0xFFFE }
};

static const int ac_base_code_len[16][10] = {
	{ 2, 2, 3, 4, 5, 6, 7, 10, 16, 16 },
	{ 4, 6, 7, 9, 11, 16, 16, 16, 16, 16 },
	{ 5, 8, 10, 16, 16, 16, 16, 16, 16, 16 },
	{ 6, 9, 11, 16, 16, 16, 16, 16, 16, 16 },
	{ 6, 10, 16, 16, 16, 16, 16, 16, 16, 16 },
	{ 7, 10, 16, 16, 16, 16, 16, 16, 16, 16 },
	{ 7, 11, 16, 16, 16, 16, 16, 16, 16, 16 },
	{ 8, 11, 16, 16, 16, 16, 16, 16, 16, 16 },
	{ 8, 15, 16, 16, 16, 16, 16, 16, 16, 16 },
	{ 9, 16, 16, 16, 16, 16, 16, 16, 16, 16 },
	{ 9, 16, 16, 16, 16, 16, 16, 16, 16, 16 },
	{ 9, 16, 16, 16, 16, 16, 16, 16, 16, 16 },
	{ 10, 16, 16, 16, 16, 16, 16, 16, 16, 16 },
	{ 11, 16, 16, 16, 16, 16, 16, 16, 16, 16 },
	{ 12, 16, 16, 16, 16, 16, 16, 16, 16, 16 }, 
	{ 16, 16, 16, 16, 16, 16, 16, 16, 16, 16 }
};

static const int mask_quant8x8[8][8] = {
	{ 16, 11, 10, 16, 24, 40, 51, 61 },
	{ 12, 12, 14, 19, 26, 58, 60, 55 },
	{ 14, 13, 16, 24, 40, 57, 69, 56 },
	{ 14, 17, 22, 29, 51, 87, 80, 62 },
	{ 18, 22, 37, 56, 68, 109, 103, 77 },
	{ 24, 35, 55, 64, 81, 104, 113, 92 },
	{ 49, 64, 78, 87, 103, 121, 120, 101 },
	{ 72, 92, 95, 98, 112, 100, 103, 99 }
};

static const int zigzag_indices8x8[8][8] = {
	{ 0,  1,  5,  6,  14, 15, 27, 28 },
	{ 2,  4,  7,  13, 16, 26, 29, 42 },
	{ 3,  8,  12, 17, 25, 30, 41, 43 },
	{ 9,  11, 18, 24, 31, 40, 44, 53 },
	{ 10, 19, 23, 32, 39, 45, 52, 54 },
	{ 20, 22, 33, 38, 46, 51, 55, 60 },
	{ 21, 34, 37, 47, 50, 56, 59, 61 },
	{ 35, 36, 48, 49, 57, 58, 62, 63 }
};

#define BETWEEN(x, L, H) (#x >= #L && #x <= #H)

static int get_dc_difference_category(short val)
{
	if (val != 0) {
		val = abs(val);
		int idx = ceilf(logf(val+1)/log(2));
		assert(idx < 12);
		return idx;
	}
	return 0;
}

int find_eob(short *x)
{
	int idx = 0;
	for (int i = 0; i < 64; ++i) {
		if (x[i] != 0) {
			idx = i + 1;
		}
	}
	return idx;
}

void zigzag_8x8(short *x, short *y)
{
	const int M = 8;
	const int N = 8;

	for (int i = 0; i < N; ++i) {
		for (int j = 0; j < M; ++j) {
			y[zigzag_indices8x8[i][j]] = x[i * M + j];
		}
	}
}

void dezigzag_8x8(short *y, short *x)
{
	const int M = 8;
	const int N = 8;

	for (int i = 0; i < N; ++i) {
		for (int j = 0; j < M; ++j) {
			x[i * M + j] = y[zigzag_indices8x8[i][j]];
		}
	}
}

void quant_8x8(short *x, int Q)
{
	if (Q <= 0) Q = 1;
	const int M = 8;
	const int N = 8;
	
	int S = Q < 50 ? floor(5000. / Q) : 200 - 2 * Q;

	for (int i = 0; i < N; ++i) {
		for (int j = 0; j < M; ++j) {
			int t = roundf((float)mask_quant8x8[i][j] * S / 100);

			if (t > 0) {
				x[i * M + j] = (short)roundf(x[i * M + j] / (float)t);
			}
		}
	}
}

void dequant_8x8(short *x, int Q)
{
	if (Q <= 0) Q = 1;
	const int M = 8;
	const int N = 8;
	
	int S = Q < 50 ? floor(5000. / Q) : 200 - 2 * Q;

	for (int i = 0; i < N; ++i) {
		for (int j = 0; j < M; ++j) {
			//int t = (S * mask_quant8x8[i][j] + 50) / 100;
			int t = round((float)mask_quant8x8[i][j] * S / 100);
			if (t > 0) {
				x[i * M + j] = x[i * M + j] * t;
			}
			
		}
	}
}


static float r_8x8(short *b, int x, int y, int u, int v)
{
	const float ap0 = 0.3535533906f;    // 1. / sqrt(M);
	const float aq0 = ap0;
	const float ap1 = 0.5f;             // sqrt(2./N)
	const float aq1 = 0.5f;

	return (u == 0 ? ap0 : ap1) * (v == 0 ? aq0 : aq1) * cosf(((2 * x + 1) * u * PI) / 16) * cosf(((2 * y + 1) * v * PI) / 16);
}

void dct_8x8(short *b)
{
	const int M = 8;
	const int N = 8;
	float T[8][8];

	for (int u = 0; u < N; u++) {
		for (int v = 0; v < M; v++) {
			float sum = 0.;

			for (int x = 0; x < N; x++) {
				for (int y = 0; y < M; y++) {
					sum += (float)b[x * M + y] * r_8x8(b, x, y, u, v);
				}
			}

			T[u][v] = sum;
		}
	}

	for (int u = 0; u < N; u++) {
		for (int v = 0; v < M; v++) {
			b[u * M + v] = (short)(T[u][v]);
		}
	}
}

void idct_8x8(short *b)
{
	float T[8][8];

	const int M = 8;
	const int N = 8;


	for (int x = 0; x < N; x++) {
		for (int y = 0; y < M; y++) {
			float sum = 0.;

			for (int u = 0; u < N; u++) {
				for (int v = 0; v < M; v++) {
					sum += (float)b[u * M + v] * r_8x8(b, x, y, u, v);
				}
			}

			T[x][y] = sum;
		}
	}

	for (int u = 0; u < N; u++) {
		for (int v = 0; v < M; v++) {
			b[u * M + v] = T[u][v];
		}
	}
}



#define STORE_FULL 1
#define NO_BITS_LEFT 2
#define EOB 0xA

static int write_bits(uint8_t *store, int *p, short val, int vlen, int *vp)
{
	if (*p >= 8) return STORE_FULL;
	//printf("write bits: %d [%s], p:%d\n", vlen, int2bin(val, NULL), *p);
	// write bits
	while (*vp < vlen) {
		int bit = (val >> (vlen - (*vp) - 1)) & 1;
		//printf("\twrite %d\n", bit);
		(*store) |= bit << (8 - (*p) - 1);
		(*vp)++;
		(*p)++;
		if (*p == 8) {
			return STORE_FULL;
		}
	}

	return NO_BITS_LEFT;
}

static void write_val(char *out, int *out_pos, uint8_t *store, int *p, short val, int vlen)
{
	int vp = 0;
	while (1) {
		//printf("OUT_POS: %d\n", *out_pos);
		int ret = write_bits(store, p, val, vlen, &vp);
		if (ret == STORE_FULL) {
			//printf("store full\n");
			out[*out_pos] = *store;
			(*out_pos)++;
			(*store) = 0;
			(*p) = 0;
		}
		else if (ret == NO_BITS_LEFT) {
			//printf("\tdone writing\n");
			break;
		}
	}
}

int huffman_enc_64(short *in, int in_len, uint8_t **out, int *out_length, short *last_block)
{
	// allocate a bit more space
	int out_pos = 0;
	uint8_t tmp[64];
	//*out = (char*)malloc(sizeof(char) * in_len);

	//for (int i = -255; i <= 255; i++) {
	//	short val = i;
	//	printf("diff category for val: %d -> %d\n", val, get_dc_difference_category(val));
	//}
	
	// get dc
	short dc = in[0];
	if (last_block != NULL) {
		dc = last_block[0] - dc;
	}
	//dc = 80;
	//dc = -1;
	//dc = -26;
	int cat = get_dc_difference_category(dc);

	uint8_t store = 0;
	int p = 0;
	int vp = 0;
	
	//printf("got dc: %d, category %d, vlen: %d, dc_basecodelen: %d\n", dc, cat, cat + dc_base_code_len[cat], dc_base_code_len[cat]);

	// write dc base code
	write_val(tmp, &out_pos, &store, &p, dc_base_code[cat], dc_base_code_len[cat]);
	// write lsb
	// no store reset
	int val = (dc & ((int)pow(2, cat + dc_base_code_len[cat]) - 1)) - (dc < 0 ? 1 : 0);
	int vlen = cat;
	write_val(tmp, &out_pos, &store, &p, val, vlen);

	//printf("write ac's\n");
	int run = 0;
	for (int i = 1; i < in_len; ++i) {
		if (in[i] == 0) {
			run++;
			if (run >= 0xF) {
				int ac = 0xFF7;
				write_val(tmp, &out_pos, &store, &p, ac, 12);
				run = 0;
			}
		}
		else {
			int ac = in[i];
			int ac_cat = get_dc_difference_category(ac);
			//printf("write ac: %d\tcat: %d\t run: %d\t\n", ac, ac_cat, run);
			//printf("write basecode: %s\n", int2bin(ac_base_code[run][ac_cat - 1], NULL));
			// write base code
			write_val(tmp, &out_pos, &store, &p, ac_base_code[run][ac_cat - 1], ac_base_code_len[run][ac_cat - 1]);
			// write lsb
			int ac_val = (ac & ((int)pow(2, ac_cat + ac_base_code_len[run][ac_cat - 1]) - 1)) - (ac < 0 ? 1 : 0);
			int ac_vlen = ac_cat;
			//printf("write ac_val [len: %d]: %s\n", ac_vlen, int2bin(ac_val, NULL));
			write_val(tmp, &out_pos, &store, &p, ac_val, ac_vlen);

			run = 0;
		}
	}

	//printf("write eob\n");
	int eob = EOB;
	int ac_vlen = 4;
	write_val(tmp, &out_pos, &store, &p, eob, ac_vlen);

	// flush rest
	//printf("p:%d\n", p);
	for (; p < 8; ++p) {
		//printf("pad\n");
		store |= 1 << (8 - p - 1);
	}
	(tmp)[out_pos++] = store;

	//for (int i = 0; i < out_pos; ++i) {
	//	printf("%d -> %s\n", i, uint2bin((tmp)[i], NULL));
	//}

	*out = (uint8_t*)malloc(sizeof(uint8_t) * out_pos);
	for (int i = 0; i < out_pos; ++i) {
		(*out)[i] = tmp[i];
	}
	*out_length = out_pos;
	
	return 0;
}

static huff_node_t *make_huff_node(enum HUFF_NODE_PAYLOAD_TYPE payload_type, int bit)
{
	huff_node_t *node = (huff_node_t*)malloc(sizeof(huff_node_t));;
	node->left = NULL;
	node->right = NULL;
	node->payload = NULL;
	node->payload_type = payload_type;
	node->bit = bit;
	return node;
}

static void delete_huff_node(huff_node_t *node)
{
	if (node) {
		if (node->left) {
			delete_huff_node(node->left);
		}
		if (node->right) {
			delete_huff_node(node->right);
		}
		if (node->payload) {
			if (node->payload_type == DC) {
				free((dc_payload_t*)node->payload);
			}
			else if (node->payload_type == AC) {
				free((ac_payload_t*)node->payload);
			}
			else {
				assert(0); // shouldt happen
			}
		}
		free(node);
	}
}

static void huff_tree_init(huff_tree_t *tree, enum HUFF_NODE_PAYLOAD_TYPE payload_type)
{
	tree->payload_type = payload_type;
	tree->root = make_huff_node(tree->payload_type, -1);
}

huff_node_t *huff_tree_insert(huff_node_t *cur, int bit)
{
	if (bit == 0) {
		if (cur->left) {
			//printf("follow left\n");
			return cur->left;
		}
		else {
			//printf("new left\n");
			huff_node_t *new_node = make_huff_node(cur->payload_type, bit);
			cur->left = new_node;
			return new_node;
		}
	}
	else if (bit == 1) {
		if (cur->right) {
			//printf("follow right\n");
			return cur->right;
		}
		else {
			//printf("new right\n");
			huff_node_t *new_node = make_huff_node(cur->payload_type, bit);
			cur->right = new_node;
			return new_node;
		}
	}
	return NULL;
}

typedef void(*update_callback)(huff_node_t *cur, int cat, int run);

void update_huff_tree(huff_tree_t *tree, int base_code, int base_code_len, int cat, int run, update_callback cb)
{
	huff_node_t *cur = tree->root;
	for (int k = 0; k < base_code_len; k++) {
		int bit = (base_code >> (base_code_len - k - 1)) & 1;
		//printf("%d ", bit);
		// insert into tree
		cur = huff_tree_insert(cur, bit);
		if (cur == NULL) {
			//printf("ERROR\n");
			return;
		}
		if (k == base_code_len - 1) {
			// insert category
			//huff_tree_insert_payload();
			//printf("done!\n");
			cb(cur, cat, run);
		}
	}
	//printf("\n");
	
}

static void update_huffman_tree_dc(huff_node_t *cur, int cat, int run)
{
	dc_payload_t *p = (dc_payload_t*)malloc(sizeof(dc_payload_t));
	p->cat = cat;
	//printf("insert DC payload with cat: %d and run: %d\n", p->cat, run);
	cur->payload = p;
}

static void update_huffman_tree_ac(huff_node_t *cur, int cat, int run)
{
	ac_payload_t *p = (ac_payload_t*)malloc(sizeof(ac_payload_t));
	p->cat = cat;
	p->run = run;
	//printf("insert AC payload with cat: %d and run: %d\n", p->cat, p->run);
	cur->payload = p;
}

void build_huffman_tree_dc(huff_tree_t *tree)
{
	huff_tree_init(tree, DC);
	for (int i = 0; i < 12; ++i) {
		int dc = dc_base_code[i];
		int dc_len = dc_base_code_len[i];
		update_huff_tree(tree, dc, dc_len, i, -1, update_huffman_tree_dc);
	}
}

void build_huffman_tree_ac(huff_tree_t *tree)
{
	huff_tree_init(tree, AC);
	//build_huffman_tree(tree, ac_base_code, ac_base_code_len, 160);
	for (int run = 0; run < 16; run++) {
		for (int cat = 0; cat < 10; cat++) {
			int ac = ac_base_code[run][cat];
			int ac_len = ac_base_code_len[run][cat];
			update_huff_tree(tree, ac, ac_len, cat + 1, run, update_huffman_tree_ac);
		}
	}

	// add EOB marker
	update_huff_tree(tree, EOB, 4, 0, 0, update_huffman_tree_ac);
	update_huff_tree(tree, 0xFF7, 12, 0, 0xF, update_huffman_tree_ac);
}

void destroy_huffman_tree(huff_tree_t *tree)
{
	if (tree->root) {
		delete_huff_node(tree->root);
	}
}

static int read_n_bits(uint8_t *in, uint8_t *store, int *p, int *in_pos, int n_bits)
{
	if (*p >= 8) {
		(*p) = 0;
		(*in_pos)++;
		(*store) = in[(*in_pos)];
	}
	//printf("read n bits: %d\n", n_bits);
	int val = 0;
	for (int i = 0; i < n_bits; ++i) {
		int bit = ((*store) >> (8 - (*p) - 1)) & 1;
		//printf("read bit: %d\n", bit);
		val |= (bit << (n_bits - i - 1));
		(*p)++;
		if (*p >= 8) {
			//printf("xxx p: %d in_pos: %d\n", *p, *in_pos);
			(*p) = 0;
			(*in_pos)++;
			(*store) = in[(*in_pos)];
		}
	}
	return val;
}

static int decode_val(int val, int vlen)
{
	// check if vlen'th bit is 0 or 1
	int dval = 0;
	int bit = (val >> (vlen - 1)) & 1;
	if (bit == 0) {
		// negative value
		dval = -(~val & ((int)powf(2, vlen) - 1));
	}
	else {
		dval = val;
	}
	return dval;
}

int huffman_dec_64(uint8_t *in, short *out, short *last_block, huff_tree_t *dc_tree, huff_tree_t *ac_tree)
{
	//printf("DECODE HUFF\n\n\n");

	memset(out, 0, sizeof(short) * 64);
	uint8_t store = in[0];
	int p = 0;
	int in_pos = 0;
	int out_pos = 0;
	// find dc
		// read bit from store

	huff_tree_t *tree = dc_tree;
	
	huff_node_t *cur = tree->root;

	int dc_found = 0;
	int dc_len = -1;
	while (dc_found == 0) {
		while (p < 8) {
			int bit = (store >> (8 - p - 1)) & 1;
			p++;
			//printf("read bit %d\n", bit);
			if (bit == 0) {
				//printf("go left\n");
				cur = cur->left;
			}
			else {
				//printf("go right\n");
				cur = cur->right;
			}
			if (cur->payload != NULL) {
				if (cur->payload_type == DC) {
					dc_payload_t *pl = (dc_payload_t*)cur->payload;
					//printf("got DC payload, cat: %d\n", pl->cat);
					dc_len = pl->cat;
					dc_found = 1;
					break;
				}
			}
		}
		if (p >= 8) {
			in_pos++;
			store = in[in_pos];
			p = 0;
		}
		// didn't find dc yet
	}

	//printf("p: %d, in_pos: %d\n", p, in_pos);
	int dc_val = read_n_bits(in, &store, &p, &in_pos, dc_len);
	int dval = decode_val(dc_val, dc_len);
	//printf("dc_val: %s [%d]\n", int2bin(dc_val, NULL), dval);
	//printf("p: %d, in_pos: %d\n", p, in_pos);
	if (last_block != NULL) {
		dval = last_block[0] - dval;
	} 
	out[out_pos] = dval;
	out_pos++;

	// decode ac's
	// should go until EOB
	// while (!not_found)
	cur = ac_tree->root;
	int eob_found = 0;
	while (eob_found == 0 && out_pos < 64) {
		while (p < 8 && out_pos < 64) {
			int bit = (store >> (8 - p - 1)) & 1;
			p++;
			//printf("read bit %d\n", bit);
			if (bit == 0) {
				//printf("go left\n");
				cur = cur->left;
			}
			else {
				//printf("go right\n");
				cur = cur->right;
			}
			if (cur == NULL) {
				goto err;
			}
			if (cur->payload != NULL) {
				if (cur->payload_type == AC) {
					ac_payload_t *pl = (ac_payload_t*)cur->payload;
					//printf("got AC payload, cat: %d, run: %d\n", pl->cat, pl->run);
					if (pl->cat == 0 && pl->run == 0) {
						eob_found = 1;
					}
					else if (pl->cat == 0 && pl->run == 0xF) {
						for (int z = 0; z < pl->run; z++) {
							out[out_pos++] = 0;
							if (out_pos >= 64) {
								//assert(0);
								goto done;
							}
						}
						cur = ac_tree->root;
					}
					else {
						// read ac_val
						int aclen = pl->cat;
						int acval = read_n_bits(in, &store, &p, &in_pos, aclen);
						int dval = decode_val(acval, aclen);
						for (int z = 0; z < pl->run; z++) {
							out[out_pos++] = 0;
							if (out_pos >= 64) {
								//assert(0);
								goto done;
							}
						}
						if (out_pos >= 64) 
							goto done;
						out[out_pos++] = dval;
						if (out_pos >= 64) {
							//assert(0);
							goto done;
						}

						// reset tree
						cur = ac_tree->root;
					}
					break;
				}
			}
		}
		if (p >= 8 && eob_found == 0) {
			//printf("extend\n");
			in_pos++;
			store = in[in_pos];
			p = 0;
		}
	}
done:


	//printf("DECODE HUFF DONE\n\n\n");

	return 0;
err:
	printf("ERROR DECODING\n");
	return 1;
}

void dc_shift_short(short *buf, int w, int h, short shift)
{
	for (int i = 0; i < h; i++) {
		for (int j = 0; j < w; j++) {
			buf[i * w + j] = buf[i * w + j] + shift;
		}
	}
}

void jpeg_encode_block(short *in, short *last_block, uint8_t **out, int *out_length, int quality)
{
	const int bwidth = 8;
	const int bheight = 8;
	short zz[64];

	dc_shift_short(in, bwidth, bheight, -128);
	dct_8x8(in);
	quant_8x8(in, quality);
	zigzag_8x8(in, zz);
	
	int zz_len = find_eob(zz);
	huffman_enc_64(zz, zz_len, out, out_length, last_block);
}

void jpeg_decode_block(uint8_t *in, short *last_block, short *out, int quality, huff_tree_t *dc_tree, huff_tree_t *ac_tree)
{
	const int bwidth = 8;
	const int bheight = 8;
	short tmp[64];
	memset(tmp, 0, sizeof(short) * 64);

	huffman_dec_64(in, tmp, last_block, dc_tree, ac_tree);
	dezigzag_8x8(tmp, out);
	dequant_8x8(out, quality);
	idct_8x8(out);
	dc_shift_short(out, bwidth, bheight, 128);
}