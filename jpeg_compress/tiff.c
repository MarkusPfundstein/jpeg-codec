#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tiff.h"


enum TIFF_ENTRY_DATA_TYPE {
	BYTE = 1,
	ASCII = 2,
	SHORT = 3,
	LONG = 4,
	RATIONAL = 5
};

typedef struct {
	char endian_tags[2];
	char magic[2];
	uint32_t ifd_offset;
} tiff_header_t;

typedef struct {
	uint16_t tag_id;
	uint16_t data_type;
	uint32_t n_values;
	uint32_t n_offset;
} ifd_entry_t;

static int read_and_reset(FILE *f, uint32_t pos, short **values, int count)
{
	long int savedPos = ftell(f);
	if (fseek(f, pos, SEEK_SET) != 0) {
		return -1;
	}

	int read_err = fread(values[0], sizeof(short), count, f);

	if (fseek(f, savedPos, SEEK_SET) != 0) {
		return read_err;
	}

	return read_err;
}

static int read_bps(FILE *f, ifd_entry_t *entry, short bitsPerSample[3])
{
	short *bps = (short*)malloc(sizeof(short) * 3);
	int err = read_and_reset(f, entry->n_offset, &bps, 3);
	bitsPerSample[0] = bps[0], bitsPerSample[1] = bps[1], bitsPerSample[2] = bps[2];
	free(bps);
	return err;
}

int read_tiff(const char *filepath, short **buf, tiff_image_t *image)
{
	int err = 0;
	long int im_pos;

	memset(image, -1, sizeof(tiff_image_t));

	*buf = NULL;
	FILE *f = fopen(filepath, "rb");
	if (f == NULL) {
		return -1;
	}

	tiff_header_t hdr;
	memset(&hdr, 0, sizeof(tiff_header_t));

	if (fread((void*)&hdr, sizeof(tiff_header_t), 1, f) < 1) {
		err = -1;
		goto cleanup;
	}

	image->little_endian = hdr.endian_tags[0] == 'I' && hdr.endian_tags[1] == 'I';

	if (!image->little_endian) {
		err = -1;
		goto cleanup;
	}

	im_pos = ftell(f);

	if (fseek(f, hdr.ifd_offset, SEEK_SET) != 0) {
		err = -1;
		goto cleanup;
	}

	uint16_t number_entries;

	if (fread((void*)&number_entries, sizeof(number_entries), 1, f) < 1) {
		err = -1;
		goto cleanup;
	}

	//printf("number entries: %d\n", number_entries);

	/* http://www.fileformat.info/format/tiff/egff.htm */
	for (int i = 0; i < number_entries; ++i) {
		ifd_entry_t entry;
		// read width & height
		if (fread((void*)&entry, sizeof(ifd_entry_t), 1, f) < 1) {
			err = -1;
			goto cleanup;
		}
		//printf("read entry %d, tag_id: %d, data_type: %d, values: %d offset: %d\n", i, entry.tag_id, entry.data_type, entry.n_values, entry.n_offset);
		switch (entry.tag_id) {
		case 256: // width
			image->width = entry.n_offset;
			break;
		case 257: // height
			image->height = entry.n_offset;
			break;
		case 258: // bits per sample
			if (entry.n_values == 3) {
				read_bps(f, &entry, image->bps);
			}
			else if (entry.n_values == 1) {
				image->bps[0] = entry.n_offset;
				image->bps[1] = image->bps[2] = -1;
			}
			break;
		case 277: // samples per pixel
			image->n_components = entry.n_offset;
		default:
			break;
		}
	}

	if (image->n_components > 1) {
		fprintf(stderr, "rgb images not yet supported\n");
		err = -1;
		goto cleanup;
	}

	if (image->n_components == -1) {
		fprintf(stderr, "couldn't find number of components in ifd entry\n");
		err = -1;
		goto cleanup;
	}

	unsigned long next_image_offset;
	fread(&next_image_offset, sizeof(unsigned long), 1, f);
	if (next_image_offset != 0) {
		err = -1;
		goto cleanup;
	}

	*buf = (short*)malloc(sizeof(short) * image->width * image->height * image->n_components);

	if (fseek(f, im_pos, SEEK_SET) != 0) {
		err = -1;
		goto cleanup;
	}

	for (int y = 0; y < image->height; y++) {
		for (int x = 0; x < image->width; x++) {
			for (int j = 0; j < image->n_components; ++j) {
				uint8_t val;
				if (fread(&val, sizeof(uint8_t), 1, f) != 1) {
					;
					err = -1;
					goto cleanup;
				}
				//printf("%d\n", val);
				(*buf)[y * image->width + x + j] = val;
			}
		}
	}

cleanup:
	fclose(f);

	return err;
}