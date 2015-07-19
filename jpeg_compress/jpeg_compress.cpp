#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "tiff.h"
#include "jpeg.h"
#include "helper.h"


typedef struct __enc_list_node {
	uint8_t *block;
	int block_len;
	struct __enc_list_node *next;
} enc_list_node_t;

typedef struct {
	enc_list_node_t *root;
	enc_list_node_t *last;
	int count;
} enc_list_t;

void enc_list_init(enc_list_t *l)
{
	l->root = NULL;
	l->last = NULL;
	l->count = 0;
}

void enc_list_node_destroy(enc_list_node_t *n)
{
	if (n) {
		if (n->next) {
			enc_list_node_destroy(n->next);
		}
		if (n->block) {
			free(n->block);
		}
		free(n);
	}
}

enc_list_node_t* make_enc_list_node(uint8_t *block, int block_len)
{
	enc_list_node_t *n = (enc_list_node_t*)malloc(sizeof(enc_list_node_t));
	n->next = NULL;
	n->block = block;
	n->block_len = block_len;
	return n;
}

void enc_list_destroy(enc_list_t *l)
{
	if (l->root) {
		enc_list_node_destroy(l->root);
	}
}

void enc_list_push(enc_list_t *l, enc_list_node_t *n)
{
	if (l->root == NULL) {
		l->root = l->last = n;
	}
	else {
		l->last->next = n;
		l->last = n;
	}
	l->count++;
}

enc_list_node_t *enc_list_get(enc_list_t *l, int n)
{
	int i = 0;
	enc_list_node_t *t = l->root;
	while (i < n) {
		t = t->next;
		i++;
	}
	return t;
}

static int do_jpeg(char *im, int quality)
{
	const int BLOCK_WIDTH = 8;
	const int BLOCK_HEIGHT = 8;
	short *buf = NULL;
	short *cmp_buf = NULL;
	short *dec_buf = NULL;
	enc_list_t enc_list;
	huff_tree_t dc_tree;
	huff_tree_t ac_tree;
	tiff_image_t image;
	int tiff_err = 0;
	int print_image = 0;

	memset(&dc_tree, 0, sizeof(dc_tree));
	memset(&ac_tree, 0, sizeof(ac_tree));
	enc_list_init(&enc_list);

	if ((tiff_err = read_tiff(im, &buf, &image)) < 0) {
		if (buf) {
			free(buf);
		}
		printf("error reading %s\n", im);
		return -1;
	}

	printf("got image:\n");
	printf("little endian : \t%d\nwidth x height : \t%d x %d\n", image.little_endian, image.width, image.height);
	if (image.n_components == 1) {
		printf("n_components:\t\t%d [%d]\n", image.n_components, image.bps[0]);
	}
	else {
		printf("n_components:\t\t%d [%d %d %d]\n", image.n_components, image.bps[0], image.bps[1], image.bps[2]);
		printf("unsupported yet\n");
		goto done;
	}

	if (image.width % BLOCK_WIDTH != 0 || image.height % BLOCK_HEIGHT != 0) {
		printf("images must be multiples of 8\n");
		goto done;
	}

	cmp_buf = (short*)malloc(sizeof(short) * image.width * image.height);
	dec_buf = (short*)malloc(sizeof(short) * image.width * image.height);
	for (int i = 0; i < image.height; i++) {
		for (int j = 0; j < image.width; j++) {
			cmp_buf[i * image.width + j] = buf[i * image.width + j];
		}
	}

	build_huffman_tree_dc(&dc_tree);
	build_huffman_tree_ac(&ac_tree);

	// split jpeg into blocks
	int blocks_y = (int)floor(image.height / 8.);
	int blocks_x = (int)floor(image.width / 8.);

	printf("compress with quality: %d\n", quality);
	printf("got blocks, y: %d, x: %d\n", blocks_y, blocks_x);

	int total_len = 0;

	short *last_block = NULL;
	int f = 0;
	for (int i = 0; i < blocks_y; ++i) {
		for (int j = 0; j < blocks_x; ++j) {
			short cur_block[BLOCK_HEIGHT][BLOCK_WIDTH];
			int cur_off = i * image.width * BLOCK_HEIGHT + j * BLOCK_WIDTH;
			for (int ii = 0; ii < BLOCK_HEIGHT; ++ii) {
				for (int jj = 0; jj < BLOCK_WIDTH; ++jj) {
					int idx = cur_off + ii * image.width + jj;
					cur_block[ii][jj] = buf[idx];// tm[ii][jj];// buf[idx];
				}
			}

			uint8_t *encoded_block;
			int enc_len;
			jpeg_encode_block(cur_block[0], NULL, &encoded_block, &enc_len, quality);

			enc_list_node_t *enc_node = make_enc_list_node(encoded_block, enc_len);
			enc_list_push(&enc_list, enc_node);

			total_len += enc_len;

			if (last_block == NULL) {
				last_block = (short*)malloc(sizeof(short));
			}
			last_block[0] = cur_block[0][0];
		}
	}
	if (last_block) free(last_block);

	printf("encoding done\n");


	last_block = NULL;
	int x = 0;
	for (int i = 0; i < blocks_y; ++i) {
		for (int j = 0; j < blocks_x; ++j) {
			enc_list_node_t *n = enc_list_get(&enc_list, x);

			int cur_off = i * image.width * BLOCK_HEIGHT + j * BLOCK_WIDTH;

			short cur_block[BLOCK_HEIGHT][BLOCK_WIDTH];
			jpeg_decode_block(n->block, NULL, cur_block[0], quality, &dc_tree, &ac_tree);

			for (int ii = 0; ii < BLOCK_HEIGHT; ++ii) {
				for (int jj = 0; jj < BLOCK_WIDTH; ++jj) {
					int idx = cur_off + ii * image.width + jj;
					dec_buf[idx] = cur_block[ii][jj];
				}
			}

			x++;
			if (last_block == NULL) {
				last_block = (short*)malloc(sizeof(short));
			}
			last_block[0] = cur_block[0][0];
		}
	}
	if (last_block) free(last_block);

	printf("decoding done\n");

	printf("check errors\n");
	double rmse = 0.;
	for (int i = 0; i < image.height; i++) {
		for (int j = 0; j < image.width; j++) {
			int e = dec_buf[i * image.width + j] - cmp_buf[i * image.width + j];
			rmse += e * e;
		}
	}
	rmse /= (image.width * image.height * image.n_components);
	rmse = sqrt(rmse);

	double comp_ratio = image.width * image.height * image.n_components * 8. / total_len * 8.;
	printf("compression ratio: %1.04f\n", comp_ratio);
	printf("RMSE: %1.04f\n", rmse);

done:
	if (cmp_buf)
		free(cmp_buf);
	if (buf)
		free(buf);
	if (dec_buf)
		free(dec_buf);

	destroy_huffman_tree(&dc_tree);
	destroy_huffman_tree(&ac_tree);

	enc_list_destroy(&enc_list);

	return 0;
}

int main(int argc, char** argv)
{
	if (argc > 1) {
		if (argc > 2) {
			do_jpeg(argv[1], atoi(argv[2]));
		}
		else {
			do_jpeg(argv[2], 50);
		}
	}
	else {
		printf("specify file please\n");
	}

	return 0;
}

