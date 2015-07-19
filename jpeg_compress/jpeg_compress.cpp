#include <string>
#include <time.h>
#include <fstream>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
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


	short tm[BLOCK_HEIGHT][BLOCK_WIDTH] = {
		{ 10, 10, 9, 8, 8, 9, 9, 10 },
		{ 9, 9, 9, 9, 8, 8, 9, 9 },
		{ 10, 9, 9, 9, 9, 8, 9, 10 },
		{ 10, 9, 9, 8, 8, 8, 8, 8 },
		{ 11, 10, 10, 9, 9, 9, 9, 9 },
		{ 42, 11, 10, 10, 10, 10, 10, 10 },
		{ 155, 43, 10, 9, 9, 10, 10, 10 },
		{ 175, 158, 57, 11, 9, 9, 9, 9 }
	};

	short *last_block = NULL;
	int f = 0;
	for (int i = 0; i < blocks_y; ++i) {
		for (int j = 0; j < blocks_x; ++j) {
			int cur_off = i * image.width * BLOCK_HEIGHT + j * BLOCK_WIDTH;
			//printf("i: %d, j: %d\ncur off: %d\n", i, j, cur_off);
			short cur_block[BLOCK_HEIGHT][BLOCK_WIDTH];
			for (int ii = 0; ii < BLOCK_HEIGHT; ++ii) {
				for (int jj = 0; jj < BLOCK_WIDTH; ++jj) {
					//printf("cur_off + ii * image.width + jj: %d\n", cur_off + ii * image.width + jj);
					int idx = cur_off + ii * image.width + jj;
					//assert(idx < image.width * image.height);
					//if (f == 453)
					//	printf("%d ", buf[idx]);
					cur_block[ii][jj] = buf[idx];// tm[ii][jj];// buf[idx];
				}
				//if (f == 453) printf("\n");
				//printf("\n");
			}
			//if (j == 2)
			//return 0;

			uint8_t *encoded_block;
			int enc_len;
			jpeg_encode_block(cur_block[0], NULL, &encoded_block, &enc_len, quality);

			//for (int h = 0; h < enc_len; h++) {
			//	printf("%d ", encoded_block[h]);
			//}
			//printf("\n");

			//f++;
			//printf("enc len %d\n", enc_len);

			enc_list_node_t *enc_node = make_enc_list_node(encoded_block, enc_len);
			enc_list_push(&enc_list, enc_node);

			total_len += enc_len;

			//last_block = cur_block;
		}
	}

	printf("encoding done\n");


	last_block = NULL;
	int x = 0;
	for (int i = 0; i < blocks_y; ++i) {
		for (int j = 0; j < blocks_x; ++j) {
			//printf("decode block %d/%d/%d\n", i, j, x);
			enc_list_node_t *n = enc_list_get(&enc_list, x);


			int cur_off = i * image.width * BLOCK_HEIGHT + j * BLOCK_WIDTH;


			//for (int h = 0; h < n->block_len; h++) {
			//	printf("%d ", n->block[h]);
			//}
			//printf("\n");

			short cur_block[BLOCK_HEIGHT][BLOCK_WIDTH];
			jpeg_decode_block(n->block, NULL, cur_block[0], quality, &dc_tree, &ac_tree);

			for (int ii = 0; ii < BLOCK_HEIGHT; ++ii) {
				for (int jj = 0; jj < BLOCK_WIDTH; ++jj) {
					//printf("cur_off + ii * image.width + jj: %d\n", cur_off + ii * image.width + jj);
					int idx = cur_off + ii * image.width + jj;
					//assert(idx < image.width * image.height);
					//printf("%d ", buf[idx]);
					dec_buf[idx] = cur_block[ii][jj];
				}
				//printf("\n");
			}

			//for (int i )

			//= &dec_buf[cur_off];

			x++;
			if (last_block == NULL) {
				//last_block = (short*)malloc(sizeof(short));
			}
			//last_block[0] = decoded_block[0][0];
		}
	}
	if (last_block) free(last_block);

	printf("decoding done\n");

	printf("check errors\n");
	double rmse = 0.;
	for (int i = 0; i < image.height; i++) {
		for (int j = 0; j < image.width; j++) {
			int e = dec_buf[i * image.width + j] - cmp_buf[i * image.width + j];
			//printf("%d ", e);
			//errbuf[i][j] = e;
			//printf("%d\t", errbuf[i][j]);
			rmse += e * e;
		}
		//printf("\n");
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

typedef struct {
	double rmse;
	double comp_ratio;
} test_t;

static test_t test_dct(int Q)
{
	huff_tree_t dc_tree;
	huff_tree_t ac_tree;

	printf("build huffman trees\n");
	build_huffman_tree_dc(&dc_tree);
	build_huffman_tree_ac(&ac_tree);
	printf("done\n");


	const int W = 8;
	const int H = 8;

	//short tm[H][W] = {
	//	{ 52, 55, 61, 66, 70, 61, 64, 73 },
	//	{ 63, 59, 66, 90, 109, 85, 69, 72 },
	//	{ 62, 59, 68, 113, 144, 104, 66, 73 },
	//	{ 63, 58, 71, 122, 154, 106, 70, 69 },
	//	{ 67, 61, 68, 104, 126, 88, 68, 70 },
	//	{ 79, 65, 60, 70, 77, 63, 58, 75 },
	//	{ 85, 71, 64, 59, 55, 61, 65, 83 },
	//	{ 87, 79, 69, 68, 65, 76, 78, 94 }
	//};


	short tm[H][W] = {
		{ 10, 10, 9, 8, 8, 9, 9, 10 },
		{ 9, 9, 9, 9, 8, 8, 9, 9 },
		{ 10, 9, 9, 9, 9, 8, 9, 10 },
		{ 10, 9, 9, 8, 8, 8, 8, 8 },
		{ 11, 10, 10, 9, 9, 9, 9, 9 },
		{ 42, 11, 10, 10, 10, 10, 10, 10 },
		{ 155, 43, 10, 9, 9, 10, 10, 10 },
		{ 175, 158, 57, 11, 9, 9, 9, 9 }
	};
	short fbuf[H][W];
	for (int i = 0; i < H; ++i) {
		for (int j = 0; j < W; ++j) {
			fbuf[i][j] = tm[i][j];
		}
	}

	//short last_block[1];
	//last_block[0] = -97;

	uint8_t *encoded;
	int enc_len = 0;
	jpeg_encode_block(fbuf[0], NULL, &encoded, &enc_len, Q);
	printf("quality: %d\n", Q);
	printf("ENCODING DONE\n\n");
	for (int i = 0; i < enc_len; ++i)
		printf("%d ", encoded[i]);
	printf("\n");

	printf("Reconstruct image\n");


	short decoded[H][W];
	jpeg_decode_block(encoded, NULL, decoded[0], Q, &dc_tree, &ac_tree);


	printf("allocate error buf\n");
	short errbuf[H][W];

	double rmse = 0.;
	for (int i = 0; i < H; i++) {
		for (int j = 0; j < W; j++) {
			int e = decoded[i][j] - tm[i][j];
			errbuf[i][j] = e;
			printf("%d\t", errbuf[i][j]);
			rmse += e * e;
		}
		printf("\n");
	}
	rmse /= (H * W);
	rmse = sqrt(rmse);
	printf("\n");

	double comp_ratio = W * H * 8. / (enc_len * 8.);
	printf("compression ratio: %1.02f\n", comp_ratio);
	printf("RSME: %f\n", rmse);

	printf("destroy huffman trees\n");
	destroy_huffman_tree(&dc_tree);
	destroy_huffman_tree(&ac_tree);
	free(encoded);

	test_t t;
	t.rmse = rmse;
	t.comp_ratio = comp_ratio;
	return t;
}

int main(int argc, char** argv)
{
	test_dct(50);// atoi(argv[1]));
	/*
	if (argc > 1) {
	test_dct(atoi(argv[1]));
	}
	else {
	}
	for (int i = 0; i <= 10; ++i) {
	//test_t t = test_dct_quality(i * 10);
	//printf("rmse Q [%d]:\t%f\t\tCR:\t%1.04f\n", i*10, t.rmse, t.comp_ratio);
	}


	return 0;

	*/

	//return 0;
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

