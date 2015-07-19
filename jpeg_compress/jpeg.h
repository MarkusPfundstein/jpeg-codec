#ifndef __JPEG_H__
#define __JPEG_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

	enum HUFF_NODE_PAYLOAD_TYPE {
		DC = 1,
		AC = 3
	};

	typedef struct {
		int cat;
	} dc_payload_t;

	typedef struct {
		int cat;
		int run;
	} ac_payload_t;

	typedef struct _huff_node {
		struct _huff_node *left;
		struct _huff_node *right;
		int bit;
		enum HUFF_NODE_PAYLOAD_TYPE payload_type;
		void *payload;
	} huff_node_t;

	typedef struct {
		enum HUFF_NODE_PAYLOAD_TYPE payload_type;
		huff_node_t *root;
	} huff_tree_t;

	extern void jpeg_encode_block(short *in, short *last_block, uint8_t **out, int *out_length, int quality);
	extern void jpeg_decode_block(uint8_t *in, short *last_block, short *out, int quality, huff_tree_t *dc_tree, huff_tree_t *ac_tree);

	extern void dc_shift_short(short *buf, int w, int h, short shift);

	extern void dct_8x8(short *x);
	extern void quant_8x8(short *x, int quality);
	extern void zigzag_8x8(short *x, short *y);
	extern int huffman_enc_64(short *in, int in_length, uint8_t **out, int *out_length, short *last_block);
	extern int find_eob(short *y);

	extern void idct_8x8(short *x);
	extern void dequant_8x8(short *x, int quality);
	extern void dezigzag_8x8(short *y, short *x);
	extern void build_huffman_tree_dc(huff_tree_t *tree);
	extern void build_huffman_tree_ac(huff_tree_t *tree);
	extern void destroy_huffman_tree(huff_tree_t *tree);
	extern int huffman_dec_64(uint8_t *in, short *out, short *last_block, huff_tree_t *dc_tree, huff_tree_t *ac_tree);

#ifdef __cplusplus
}
#endif

#endif