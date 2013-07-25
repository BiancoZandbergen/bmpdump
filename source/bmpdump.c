/* bmpdump is an utility to convert a 24 bit uncompressed BMP image
 * to other formats.
 * 
 * Currently supported formats:
 *      1. raw (8, 12, 16, 24 bits)
 *      2. C array (8, 12, 16, 24 bits)
 *      
 * This application uses only ANSI C functions and
 * can be compiled with most (if not all) C compilers.
 * Modifications might be needed to use this application
 * on big endian machines.
 * 
 * Usage instructions: see 'bmpdump -help'
 * 
 * Changes:
 *      December 3, 2009        Initial release
 *      
 * Author: Bianco Zandbergen <zandbergenb[_AT_]gmail.com>
 */          
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* constant macro's */
#define UNSET               0
#define FORMAT_RAW          1
#define FORMAT_CARRAY       2
#define APPEND              1
#define VERBOSE             1
#define EXISTS              1

/* used to save the commandline options */ 
struct options {
    char *input_file;
    char *output_file;
    int format;
    int bpp;
    int append;
    char *arrayname;
    int verbose;
} opts;

/* used to save the BMP image header */
struct bmp_header {
    unsigned short identifier;
    unsigned int file_size;
    unsigned int data_offset;
    unsigned int header_size;
    unsigned int width;
    unsigned int height;
    unsigned short planes;
    unsigned short bpp;
    unsigned int compression;
    unsigned int data_size;
    unsigned int hresolution;
    unsigned int vresolution;
    unsigned int colors;
    unsigned int important_colors;
} header;

/* we use an array of pixel structures to buffer the pixel data */
struct pixel {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

/* forward declarations */
int parse_opts(int argc, char* argv[], struct options * opts);
int get_header(FILE *fp, struct bmp_header *h);
void get_data(FILE *fp, struct pixel *pixbuf, struct bmp_header *h);
int create_c_array_8bit(struct pixel *buf, int pixels, struct options *o, struct bmp_header *h);
int create_c_array_12bit(struct pixel *buf, int pixels, struct options *o, struct bmp_header *h);
int create_c_array_16bit(struct pixel *buf, int pixels, struct options *o, struct bmp_header *h);
int create_c_array_24bit(struct pixel *buf, int pixels, struct options *o, struct bmp_header *h);
int create_raw_8bit(struct pixel *buf, int pixels, struct options *o);
int create_raw_12bit(struct pixel *buf, int pixels, struct options *o);
int create_raw_16bit(struct pixel *buf, int pixels, struct options *o);
int create_raw_24bit(struct pixel *buf, int pixels, struct options *o);
void print_options(struct options *o);
void print_header(struct bmp_header *h);
void print_help(void);

int main(int argc, char *argv[])
{
    FILE *fp;
    int i, pixels;
    struct pixel * pixbuf;
    int pixbufsize;
    
    /* parse command line options */
    if (! parse_opts(argc, argv, &opts)) {
        return 1;
    }
    
    if (opts.verbose == VERBOSE) print_options(&opts);
    
    /* open BMP image file */   
    fp = fopen(opts.input_file, "rb+");
    
    if (fp == NULL) {
        printf("Failed to open file...\n");
        return 1;
    }
    
    /* get the BMP image header */
    if (! get_header(fp, &header)) return 1;
    if (opts.verbose == VERBOSE) print_header(&header);
    
    /* calculate number of pixels in the image */
    pixels = header.width * header.height;
    
    /* allocate memory for pixel buffer */
    pixbufsize = header.width * header.height * sizeof(struct pixel);
    pixbuf = malloc(pixbufsize);
    
    if (pixbuf == NULL) {
        printf("Memory allocation failed (1)");
        return 1;
    }
    
    /* get pixel data from BMP image */
    get_data(fp, pixbuf, &header);
    
    /* we got the data, so now we can close the file */
    fclose(fp); 
    
    /* create output file in the right format */
    if (opts.format == FORMAT_CARRAY) {
        
        switch (opts.bpp) {
            case 8:
                create_c_array_8bit(pixbuf, pixels, &opts, &header);
                break;
            case 12:
                create_c_array_12bit(pixbuf, pixels, &opts, &header);
                break;
            case 16:
                create_c_array_16bit(pixbuf, pixels, &opts, &header);
                break;
            case 24:
                create_c_array_24bit(pixbuf, pixels, &opts, &header);
                break;
            default:
                printf("C array output %d bits per pixel is not supported\n", opts.bpp);
        }
        
    } else if (opts.format == FORMAT_RAW) {
        
        switch (opts.bpp) {
            case 8:
                create_raw_8bit(pixbuf, pixels, &opts);
                break;
            case 12:
                create_raw_12bit(pixbuf, pixels, &opts);
                break;
            case 16:
                create_raw_16bit(pixbuf, pixels, &opts);
                break;
            case 24:
                create_raw_24bit(pixbuf, pixels, &opts);
                break;
            default:
                printf("RAW output %d bits per pixel is not supported\n", opts.bpp);
        }
    }
       
    return 0;   
}

/* Get BMP header and save it in a bmp_header structure
 * 
 * Arguments:   fp:     pointer to file descriptor
 *              h:      pointer to bmp_header structure 
 * 
 * Return:      1 if succesfully read the header and the BMP is
 *              in a valid format (24bpp, uncompressed)    
 */
int get_header(FILE *fp, struct bmp_header *h)
{
    /* get and check identifier. offset: 0x00 size: 2 bytes */
    fread(&h->identifier,sizeof(unsigned short), 1, fp);
    
    if (h->identifier != 0x4D42) {
        printf("Unknown identifier.\n");
        return 0;
    }
    
    /* get file size. offset: 0x02 size: 4 bytes */
    fread(&h->file_size,sizeof(unsigned int), 1, fp);
    
    /* get bitmap data offset. offset: 0x0A size: 4 bytes */
    if (fseek(fp, 4, SEEK_CUR) != 0) {
        printf("fseek failed.\n");
        return 0;
    }
    
    fread(&h->data_offset,sizeof(unsigned int), 1, fp);
    
    /* get header size. offset: 0x0E size: 4 bytes */
    fread(&h->header_size,sizeof(unsigned int), 1, fp);
    
    /* get and check image width. offset: 0x12 size: 4 bytes */
    fread(&h->width,sizeof(unsigned int), 1, fp);
    
    /* get and check image height. offset: 0x16 size: 4 bytes */
    fread(&h->height,sizeof(unsigned int), 1, fp);
        
    /* get and check planes. offset: 0x1A size: 2 bytes */
    fread(&h->planes,sizeof(unsigned short), 1, fp);
    
    if (h->planes != 1) {
        printf("planes should be 1\n");
        return 0;
    }
    
    /* get and check bpp. offset: 0x1c size: 2 bytes */
    fread(&h->bpp ,sizeof(unsigned short), 1, fp);
    
    if (h->bpp != 24) {
        printf("image should be 24 bits per pixel\n");
        return 0;
    }
    
    /* get and check compression. offset: 0x1E size: 4 bytes */
    fread(&h->compression ,sizeof(unsigned int), 1, fp);
    
    if (h->compression != 0) {
        printf("bmp file should be not compressed\n");
        return 0;
    }
    
    /* get bitmap data size. offset: 0x22 size: 4 bytes */
    fread(&h->data_size ,sizeof(unsigned int), 1, fp);
    
    /* get horizontal resolution. offset: 0x26 size: 4 bytes */
    fread(&h->hresolution ,sizeof(unsigned int), 1, fp);
    
    /* get vertical resolution. offset: 0x2A size: 4 bytes */
    fread(&h->vresolution ,sizeof(unsigned int), 1, fp);
    
    /* get colors. offset: 0x2E size: 4 bytes */
    fread(&h->colors ,sizeof(unsigned int), 1, fp);
    
    /* get important colors. offset: 0x32 size: 4 bytes */
    fread(&h->important_colors ,sizeof(unsigned int), 1, fp);
    
    return 1;    
}


/* Get the pixel data from the BMP image
 * and save it in a buffer.
 * 
 * Arguments:   fp:     pointer to file descriptor
 *              pixbuf: pointer to array of pixel structures
 *              h:      pointer to bmp_header structure  
 * 
 * Return:      nothing
 */
void get_data(FILE *fp, struct pixel *pixbuf, struct bmp_header *h) {

    int padding;
    unsigned int line, linebyte;
    struct pixel *pix = pixbuf;
    
    /* calculate padding bytes, every line starts at a 32bit boundary */
    padding = ((header.width * 3) % 4);
    
    fseek(fp, header.data_offset, SEEK_SET);
    
    /* for each line */
    for (line = 0; line< h->height; line++) {
    
        /* for each pixel in the line */
        for (linebyte = 0; linebyte < h->width; linebyte++, pix++) {
            fread(&pix->b ,sizeof(unsigned char), 1, fp);
            fread(&pix->g ,sizeof(unsigned char), 1, fp);
            fread(&pix->r ,sizeof(unsigned char), 1, fp);
            //printf("Line: %u r: %u g: %u b: %u\n", line, pix->r, pix->g, pix->b);
        }
        
        if (padding != 0) {
            /* skip padding bytes after each line */
            fseek(fp, padding, SEEK_CUR);
        }
    }
                   
}

/* Save the contents of the pixel buffer to a file
 * as an C array with 8 bits per pixel.
 * Pixel format: RRRGGGBB 
 * 
 * Arguments:   buf:     pointer to array of pixel structures
 *              pixels:  number of pixels in buffer
 *              o:       pointer to options structure
 *              h:       pointer to bmp_header structure     
 * 
 * Return:      0 if failed to create output file
 */
int create_c_array_8bit(struct pixel *buf, int pixels, struct options *o, struct bmp_header *h) {
    
    FILE *fp;
    int i, file_exists;
    unsigned char b;
    struct pixel *pix = buf;
    
    /* check if file exists */
    if (fopen(o->output_file, "r") == NULL) {
        file_exists = 0;
    } else {
        file_exists = EXISTS;
    }
    
    /* check if we append or overwrite if file exists */
    if (o->append == APPEND) {
        fp = fopen(o->output_file, "a+");
    } else {
        fp = fopen(o->output_file, "w+");
    }
    
    /* succesfully opened? */
    if (fp == NULL) {
        printf("Failed open output file %s\n", o->output_file);
        return 0;
    }
    
    if (o->append == APPEND && file_exists) {
        fprintf(fp, "\n\n");
    } else {
        fprintf(fp, "/* This is an auto-generated file generated by bmpdump */\n\n");
    }
    
    fprintf(fp, "/* Array with bitmap containing data of a %ux%u (%u pixels) image.\n", h->width, h->height, pixels);
    fprintf(fp, " * Each pixel has 8 bits (RRRGGGBB).\n");
    fprintf(fp, " */\n");
    fprintf(fp, "unsigned char %s[] = {\n\t", o->arrayname);
    
    /* write array data */
    for (i=0; i< pixels; i++) {
        
        b = pix->r;
        b &= ~(0xFF >> 3);
        b |= (pix->g >> 3);
        b &= ~(0xFF >> 6);
        b |= (pix->b >> 6);
        
        fprintf(fp, "0x%.2x, ", b);
          
        pix++;
        
        /* a new line after each 12 pixels (12 bytes).
         * We have written i+1 pixels.
         */         
        if (((i+1) % 12) == 0) {
            fprintf(fp, "\n\t");
        }
             
        
    }
    fprintf(fp, "\n};");
    fclose(fp);
    
    return 1;
}

/* Save the contents of the pixel buffer to a file
 * as an C array with 12 bits per pixel.
 * Pixel format: RRRRGGGG BBBB[RRRR GGGGBBBB] (two pixels) 
 * 
 * Arguments:   buf:     pointer to array of pixel structures
 *              pixels:  number of pixels in buffer
 *              o:       pointer to options structure
 *              h:       pointer to bmp_header structure     
 * 
 * Return:      0 if failed to create output file
 */
int create_c_array_12bit(struct pixel *buf, int pixels, struct options *o, struct bmp_header *h) {
    
    FILE *fp;
    int i, file_exists;
    unsigned char b1, b2, b3;
    struct pixel *pix = buf;
    
    /* check if file exists */
    if (fopen(o->output_file, "r") == NULL) {
        file_exists = 0;
    } else {
        file_exists = EXISTS;
    }
    
    /* check if we append or overwrite if file exists */
    if (o->append == APPEND) {
        fp = fopen(o->output_file, "a+");
    } else {
        fp = fopen(o->output_file, "w+");
    }
    
    /* succesfully opened? */
    if (fp == NULL) {
        printf("Failed open output file %s\n", o->output_file);
        return 0;
    }
    
    if (o->append == APPEND && file_exists) {
        fprintf(fp, "\n\n");
    } else {
        fprintf(fp, "/* This is an auto-generated file generated by bmpdump */\n\n");
    }
    
    fprintf(fp, "/* Array with bitmap containing data of a %ux%u (%u pixels) image.\n", h->width, h->height, pixels);
    fprintf(fp, " * Each pixel has 12 bits, two pixels share three bytes (RRRRGGGG BBBBRRRR GGGGBBBB).\n");
    fprintf(fp, " */\n");
    fprintf(fp, "unsigned char %s[] = {\n\t", o->arrayname);
    
    /* write array data */
    for (i=0; i< pixels; i+=2) {
        
        b1 = pix->r;
        b1 &= ~0x0F;
        b1 |= (pix->g >> 4);
        
        b2 = pix->b;
        b2 &= ~0x0F;
           
        /* don't write last byte for last pixel of an uneven number of pixels */
        if ((i+2) <= pixels) {
            pix++;
            b2 |= (pix->r >> 4);
            
            b3 = pix->g;
            b3 &= ~0x0F;
            b3 |= (pix->b >> 4);
            
            fprintf(fp, "0x%.2x, 0x%.2x, 0x%.2x, ", b1, b2, b3);
        } else {
            fprintf(fp, "0x%.2x, 0x%.2x, ", b1, b2);
        }
        
        pix++;
        
        /* a new line after each 8 pixels (12 bytes).
         * We have written i+2 pixels.
         */         
        if (((i+2) % 8) == 0) {
            fprintf(fp, "\n\t");
        }
             
        
    }
    fprintf(fp, "\n};");
    fclose(fp);
    
    return 1;
}

/* Save the contents of the pixel buffer to a file
 * as an C array with 16 bits per pixel.
 * Pixel format: RRRRRGGG GGGBBBBB 
 * 
 * Arguments:   buf:     pointer to array of pixel structures
 *              pixels:  number of pixels in buffer
 *              o:       pointer to options structure
 *              h:       pointer to bmp_header structure     
 * 
 * Return:      0 if failed to create output file
 */
int create_c_array_16bit(struct pixel *buf, int pixels, struct options *o, struct bmp_header *h) {
    
    FILE *fp;
    int i, file_exists;
    unsigned char b1, b2;
    struct pixel *pix = buf;
    
    /* check if file exists */
    if (fopen(o->output_file, "r") == NULL) {
        file_exists = 0;
    } else {
        file_exists = EXISTS;
    }
    
    /* check if we append or overwrite if file exists */
    if (o->append == APPEND) {
        fp = fopen(o->output_file, "a+");
    } else {
        fp = fopen(o->output_file, "w+");
    }
    
    /* succesfully opened? */
    if (fp == NULL) {
        printf("Failed open output file %s\n", o->output_file);
        return 0;
    }
    
    if (o->append == APPEND && file_exists) {
        fprintf(fp, "\n\n");
    } else {
        fprintf(fp, "/* This is an auto-generated file generated by bmpdump */\n\n");
    }
    
    fprintf(fp, "/* Array with bitmap containing data of a %ux%u (%u pixels) image.\n", h->width, h->height, pixels);
    fprintf(fp, " * Each pixel has 16 bits (RRRRRGGG GGGBBBBB).\n");
    fprintf(fp, " */\n");
    fprintf(fp, "unsigned char %s[] = {\n\t", o->arrayname);
    
    /* write array data */
    for (i=0; i< pixels; i++) {
        
        b1 = pix->r;        
        b1 &= ~(0xFF >> 5); 
        b1 |= (pix->g >> 5);
        
        b2 |= (pix->g << 3);
        b2 &= ~(0xFF >> 3);
        b2 |= (pix->b >> 3);
        
        fprintf(fp, "0x%.2x, 0x%.2x, ", b1, b2);
          
        pix++;
        
        /* a new line after each 6 pixels (12 bytes).
         * We have written i+1 pixels.
         */         
        if (((i+1) % 6) == 0) {
            fprintf(fp, "\n\t");
        }
             
        
    }
    fprintf(fp, "\n};");
    fclose(fp);
    
    return 1;
}

/* Save the contents of the pixel buffer to a file
 * as an C array with 24 bits per pixel.
 * Pixel format: RRRRRRRR GGGGGGGG BBBBBBBB 
 * 
 * Arguments:   buf:     pointer to array of pixel structures
 *              pixels:  number of pixels in buffer
 *              o:       pointer to options structure
 *              h:       pointer to bmp_header structure     
 * 
 * Return:      0 if failed to create output file
 */
int create_c_array_24bit(struct pixel *buf, int pixels, struct options *o, struct bmp_header *h) {
    
    FILE *fp;
    int i, file_exists;
    struct pixel *pix = buf;
    
    /* check if file exists */
    if (fopen(o->output_file, "r") == NULL) {
        file_exists = 0;
    } else {
        file_exists = EXISTS;
    }
    
    /* check if we append or overwrite if file exists */
    if (o->append == APPEND) {
        fp = fopen(o->output_file, "a+");
    } else {
        fp = fopen(o->output_file, "w+");
    }
    
    /* succesfully opened? */
    if (fp == NULL) {
        printf("Failed open output file %s\n", o->output_file);
        return 0;
    }
    
    if (o->append == APPEND && file_exists) {
        fprintf(fp, "\n\n");
    } else {
        fprintf(fp, "/* This is an auto-generated file generated by bmpdump */\n\n");
    }
    
    fprintf(fp, "/* Array with bitmap containing data of a %ux%u (%u pixels) image.\n", h->width, h->height, pixels);
    fprintf(fp, " * Each pixel has 24 bits (RRRRRRRR GGGGGGGG BBBBBBBB).\n");
    fprintf(fp, " */\n");
    fprintf(fp, "unsigned char %s[] = {\n\t", o->arrayname);
    
    /* write array data */
    for (i=0; i< pixels; i++) {
                
        fprintf(fp, "0x%.2x, 0x%.2x, 0x%.2x, ", pix->r, pix->g, pix->b);
          
        pix++;
        
        /* a new line after each 4 pixels (12 bytes).
         * We have written i+1 pixels.
         */         
        if (((i+1) % 4) == 0) {
            fprintf(fp, "\n\t");
        }
             
        
    }
    fprintf(fp, "\n};");
    fclose(fp);
    
    return 1;
}

/* Save the contents of the pixel buffer to a file
 * as RAW with 8 bits per pixel.
 * Pixel format: RRRGGGBB 
 * 
 * Arguments:   buf:     pointer to array of pixel structures
 *              pixels:  number of pixels in buffer
 *              o:       pointer to options structure   
 * 
 * Return:      0 if failed to create output file
 */
int create_raw_8bit(struct pixel *buf, int pixels, struct options *o) {
    
    FILE *fp;
    int i;
    unsigned char b;
    struct pixel *pix = buf;
        
    /* check if we append or overwrite if file exists */
    if (o->append == APPEND) {
        fp = fopen(o->output_file, "ab+");
    } else {
        printf("using fubar\n");
        fp = fopen(o->output_file, "wb+");
    }
    
    /* succesfully opened? */
    if (fp == NULL) {
        printf("Failed open output file %s\n", o->output_file);
        return 0;
    }

    /* write array data */
    for (i=0; i< pixels; i++) {
        
        b = pix->r;
        b &= ~(0xFF >> 3);
        b |= (pix->g >> 3);
        b &= ~(0xFF >> 6);
        b |= (pix->b >> 6);
        
        fwrite(&b, sizeof(unsigned char), 1, fp);
          
        pix++;     
    }
    
    fclose(fp);
    
    return 1;
}

/* Save the contents of the pixel buffer to a file
 * as RAW with 12 bits per pixel.
 * Pixel format: RRRRGGGG BBBB[RRRR GGGGBBBB] (two pixels) 
 * 
 * Arguments:   buf:     pointer to array of pixel structures
 *              pixels:  number of pixels in buffer
 *              o:       pointer to options structure    
 * 
 * Return:      0 if failed to create output file
 */
int create_raw_12bit(struct pixel *buf, int pixels, struct options *o) {
    
    FILE *fp;
    int i;
    unsigned char b1, b2, b3;
    struct pixel *pix = buf;
    
    /* check if we append or overwrite if file exists */
    if (o->append == APPEND) {
        fp = fopen(o->output_file, "ab+");
    } else {
        fp = fopen(o->output_file, "wb+");
    }
    
    /* succesfully opened? */
    if (fp == NULL) {
        printf("Failed open output file %s\n", o->output_file);
        return 0;
    }
        
    /* write array data */
    for (i=0; i< pixels; i+=2) {
        
        b1 = pix->r;
        b1 &= ~0x0F;
        b1 |= (pix->g >> 4);
        
        b2 = pix->b;
        b2 &= ~0x0F;
           
        /* don't write last byte for last pixel of an uneven number of pixels */
        if ((i+2) <= pixels) {
            pix++;
            b2 |= (pix->r >> 4);
            
            b3 = pix->g;
            b3 &= ~0x0F;
            b3 |= (pix->b >> 4);
            
            fwrite(&b1, sizeof(unsigned char), 1, fp);
            fwrite(&b2, sizeof(unsigned char), 1, fp);
            fwrite(&b3, sizeof(unsigned char), 1, fp);
            
        } else {
            fwrite(&b1, sizeof(unsigned char), 1, fp);
            fwrite(&b2, sizeof(unsigned char), 1, fp);
        }
        
        pix++;        
    }
    
    fclose(fp);
    
    return 1;
}

/* Save the contents of the pixel buffer to a file
 * as RAW with 16 bits per pixel.
 * Pixel format: RRRRRGGG GGGBBBBB 
 * 
 * Arguments:   buf:     pointer to array of pixel structures
 *              pixels:  number of pixels in buffer
 *              o:       pointer to options structure   
 * 
 * Return:      0 if failed to create output file
 */
int create_raw_16bit(struct pixel *buf, int pixels, struct options *o) {
    
    FILE *fp;
    int i;
    unsigned char b1, b2;
    struct pixel *pix = buf;
    
    /* check if we append or overwrite if file exists */
    if (o->append == APPEND) {
        fp = fopen(o->output_file, "ab+");
    } else {
        fp = fopen(o->output_file, "wb+");
    }
    
    /* succesfully opened? */
    if (fp == NULL) {
        printf("Failed open output file %s\n", o->output_file);
        return 0;
    }
    
    /* write array data */
    for (i=0; i< pixels; i++) {
        
        b1 = pix->r;        
        b1 &= ~(0xFF >> 5); 
        b1 |= (pix->g >> 5);
        
        b2 |= (pix->g << 3);
        b2 &= ~(0xFF >> 3);
        b2 |= (pix->b >> 3);
        
        fwrite(&b1, sizeof(unsigned char), 1, fp);
        fwrite(&b2, sizeof(unsigned char), 1, fp);
          
        pix++;        
    }

    fclose(fp);
    
    return 1;
}

/* Save the contents of the pixel buffer to a file
 * as RAW with 24 bits per pixel.
 * Pixel format: RRRRRRRR GGGGGGGG BBBBBBBB 
 * 
 * Arguments:   buf:     pointer to array of pixel structures
 *              pixels:  number of pixels in buffer
 *              o:       pointer to options structure   
 * 
 * Return:      0 if failed to create output file
 */
int create_raw_24bit(struct pixel *buf, int pixels, struct options *o) {
    
    FILE *fp;
    int i;
    struct pixel *pix = buf;
    
    /* check if we append or overwrite if file exists */
    if (o->append == APPEND) {
        fp = fopen(o->output_file, "ab+");
    } else {
        fp = fopen(o->output_file, "wb+");
    }
    
    /* succesfully opened? */
    if (fp == NULL) {
        printf("Failed open output file %s\n", o->output_file);
        return 0;
    }
    
    /* write array data */
    for (i=0; i< pixels; i++) {
        
        fwrite(&pix->r, sizeof(unsigned char), 1, fp);
        fwrite(&pix->g, sizeof(unsigned char), 1, fp);
        fwrite(&pix->b, sizeof(unsigned char), 1, fp);      
          
        pix++;   
    }

    fclose(fp);
    
    return 1;
}

/* Parse and save the command line options
 * 
 * Arguments:   argc:    number of arguments (including file name!)
 *              argv:    array of pointers to strings
 *              opts:    pointer to options structure to save the options     
 * 
 * Return:      0 if a parse error happened
 */
int parse_opts(int argc, char* argv[], struct options *opts)
{
    int i=1;
    
    while (i < argc) {
        
        /* check for input file parameter */
        if (strcmp(argv[i], "-if") == 0) {
            
            if ((i+1) >= argc) {
                printf("-if missing file name\n");
                printf("usage: -if <filename>\n");
                return 0;
            }
            opts->input_file = argv[i+1];
            i += 2; 
        } 
        /* check for output file parameter */
        else if (strcmp(argv[i], "-of") == 0) {
            
            if ((i+1) >= argc) {
                printf("-of missing file name\n");
                printf("usage: -of <filename>\n");
                return 0;
            }
            opts->output_file = argv[i+1];
            i += 2; 
        } 
        /* check for format parameter */
        else if (strcmp(argv[i], "-format") == 0) {
            
            if ((i+1) >= argc) {
                printf("-format missing format type\n");
                printf("usage: -format <carray/raw>\n");
                return 0;
            }
            
            if (strcmp(argv[i+1], "carray") == 0) {
                opts->format = FORMAT_CARRAY;
            } else if (strcmp(argv[i+1], "raw") == 0) {
                opts->format = FORMAT_RAW;
            } else {
                printf("'%s' is an invalid format\n",argv[i+1]);
                printf("usage: -format <carray/raw>\n");
            }
            i += 2;  
        } 
        /* check for bits per pixel parameter */
        else if (strcmp(argv[i], "-bpp") == 0) {
            
            if ((i+1) >= argc) {
                printf("-bpp missing number\n");
                printf("usage: -bpp <8/12>\n");
                return 0;
            }
            
            if (strcmp(argv[i+1], "8") == 0) {
                opts->bpp = 8;
            } else if (strcmp(argv[i+1], "12") == 0) {
                opts->bpp = 12;
            } else if (strcmp(argv[i+1], "16") == 0) {
                opts->bpp = 16;
            } else if (strcmp(argv[i+1], "24") == 0) {
                opts->bpp = 24;
            } else {
                printf("'%s' is an invalid bpp value\n",argv[i+1]);
                printf("usage: -bpp <8/12/16/24>\n");
                return 0;
            }
            i += 2;  
        }
        /* check for arrayname parameter */
        else if (strcmp(argv[i], "-arrayname") == 0) {
            
            if ((i+1) >= argc) {
                printf("-arrayname missing name\n");
                printf("usage: -arrayname <name>\n");
                return 0;
            }
            
            opts->arrayname = argv[i+1];
            
            i += 2;  
        } 
        /* check for append parameter */
        else if (strcmp(argv[i], "-append") == 0) {
            opts->append = APPEND;
            i++;  
        } 
        /* check for verbose parameter */
        else if (strcmp(argv[i], "-verbose") == 0) {
            opts->verbose = VERBOSE;
            i++;  
        }
        /* check for verbose parameter */
        else if (strcmp(argv[i], "help") == 0
                 || strcmp(argv[i], "-help") == 0
                 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        }
        else {
            printf("'%s' is an invalid parameter\n",argv[i]);
            return 0;
        }    
    }
    
    /* give default values for some unused options */
    
    /* default input file: bitmap.bmp */
    if (opts->input_file == NULL) {
        
        opts->input_file = malloc(sizeof(char)*11);
        
        if (opts->input_file == NULL) {
            printf("Memory allocation failed (2)");
            return 0;
        }
        
        sprintf(opts->input_file, "bitmap.bmp");
        printf("No input file specified: using %s\n", opts->input_file);
    }
    
    /* default format: c array */
    if (opts->format == UNSET) {
        opts->format = FORMAT_CARRAY;
        printf("No format specified: using carray (C array)\n");
    }
    
    /* default output file */
    if (opts->output_file == NULL) {
        /* default c array output file: bitmap.h */
        if (opts->format == FORMAT_CARRAY) {
            opts->output_file = malloc(sizeof(char)*9);
            
            if (opts->output_file == NULL) {
                printf("Memory allocation failed (3)");
                return 0;
            }
            
            sprintf(opts->output_file, "bitmap.c");
        } else if (opts->format == FORMAT_RAW) {
            opts->output_file = malloc(sizeof(char)*11);
            
            if (opts->output_file == NULL) {
                printf("Memory allocation failed (4)");
                return 0;
            }
            
            sprintf(opts->output_file, "bitmap.raw");
        }
        printf("No output file specified: using %s\n", opts->output_file);
    }
    
    /* default bpp: 12 bit */
    if (opts->bpp == UNSET) {
        opts->bpp = 12;
        printf("No bpp specified: using %d bpp\n", opts->bpp);
    }
    
    /* default C array name: bitmap */
    if (opts->format == FORMAT_CARRAY && opts->arrayname == NULL) {
        opts->arrayname = malloc(sizeof(char)*7);
        
        if (opts->arrayname == NULL) {
                printf("Memory allocation failed (5)");
                return 0;
        }
        
        sprintf(opts->arrayname, "bitmap");
        printf("No C array name specified: using %s[]\n", opts->arrayname);
    }
    
    return 1;
}

/* Print the contents of a bmp_header structure.
 * 
 * Arguments:   h:     pointer to bmp_header structure 
 * 
 * Return:      nothing
 */
void print_header(struct bmp_header *h)
{
    printf("==== BMP HEADER ====\n");
    printf("identifier: %u (0x%x)\n", h->identifier, h->identifier);
    printf("file size: %u (0x%x) bytes\n", h->file_size, h->file_size);
    printf("bitmap data offset: %u (0x%x) bytes\n", h->data_offset, h->data_offset);
    printf("header size: %u (0x%x) bytes\n", h->header_size, h->header_size);
    printf("image width: %u (0x%x)\n", h->width, h->width);
    printf("image height: %u (0x%x)\n", h->height, h->height);
    printf("planes: %u (0x%x)\n", h->planes, h->planes);
    printf("bits per pixel: %u (0x%x)\n", h->bpp, h->bpp);
    printf("compression: %u (0x%x)\n", h->compression, h->compression);
    printf("bitmap data size: %u (0x%x)\n", h->data_size, h->data_size);
    printf("horizontal resolution: %u pixels/meter\n", h->hresolution, h->hresolution);
    printf("vertical resolution: %u pixels/meter\n", h->vresolution, h->vresolution);
    printf("colors: %u\n", h->colors, h->colors);
    printf("important colors: %u\n", h->important_colors, h->important_colors);
    printf("====================\n");
}

/* Print the contents of a options structure
 * 
 * Arguments:   0:     pointer to options structure 
 * 
 * Return:      nothing
 */
void print_options(struct options *o)
{
    printf("===== OPTIONS  =====\n");
    printf("Input file: %s\n", o->input_file);
    printf("Ouput file: %s\n", o->output_file);
    
    if (o->format == FORMAT_CARRAY)
        printf("Format: C array\n");
    else if (o->format == FORMAT_RAW)
        printf("Format: Raw\n");
        
    printf("Bits per pixel: %d\n", o->bpp);
    
    if (o->append == APPEND)
        printf("Append: yes\n");
    else 
        printf("Append: no\n");
        
    printf("Array name: %s\n");
    
    if (o->verbose == VERBOSE) 
        printf("Verbose: yes\n");
    else
        printf("Verbose: no\n");
    
    printf("====================\n");
}

/* Print help dialog
 * 
 * Arguments:   none
 * 
 * Return:      nothing
 */
void print_help() 
{
    printf("bmpdump is a utility to convert a 24 bit uncompressed BMP image\ninto other formats.\n\n");
    printf("currently supported output formats:\nC array (8, 12, 16, 24 bits)\nRAW (8, 12, 16, 24 bits)\n\n");
    printf("usage: bmpdump <parameters>\n\n");
    printf("Parameters:\n");
    printf("-if <file path>                 Input BMP file\n");
    printf("-of <file path>                 Output file\n");
    printf("-append                         Append if output file exists\n");
    printf("-format <carray/raw>            Output format (C array or Raw)\n");       
    printf("-bpp <8/12/16/24>               Bits per pixel in output file\n");
    printf("-arrayname <array name>         Array name if output format is C array\n");
    printf("-verbose                        More verbose\n");
    printf("-help                           Show help\n");
}
