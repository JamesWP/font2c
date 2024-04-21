#include <stdio.h>
#include <assert.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/ftbitmap.h>

#define WIDTH   640
#define BYTEWIDTH (WIDTH)/8
#define HEIGHT  65

static int image_width;
static int image_byte_width;
static int image_height;
static unsigned char *image; //[HEIGHT][BYTEWIDTH];

static FT_Library library;
static FT_Face face;
static FT_Error err;
static FT_Bitmap tempbitmap;

static void to_bitmap( FT_Bitmap*  bitmap, FT_Int x, FT_Int y) {

    FT_Int  i, j, p, q;
    FT_Int  x_max = x + bitmap->width;
    FT_Int  y_max = y + bitmap->rows;

    for ( i = x, p = 0; i < x_max; i++, p++ ) {
        for ( j = y, q = 0; j < y_max; j++, q++ ) {
            if ( (i < 0) || (j < 0) || (i >= image_width || j >= image_height) )
                continue;
            image[j*image_byte_width + (i >> 3)] |= (bitmap->buffer[q * bitmap->width + p]) << (i & 7);
        }
    }
}

static void draw_glyph(unsigned char glyph, int *x, int *y) {
    FT_UInt  glyph_index;
    FT_GlyphSlot  slot = face->glyph;

    glyph_index = FT_Get_Char_Index( face, glyph );

    if ((err = FT_Load_Glyph( face, glyph_index, FT_LOAD_DEFAULT ))) {
        fprintf( stderr, "warning: failed FT_Load_Glyph 0x%x %d\n", glyph, err);
        return;
    }

    if ((err = FT_Render_Glyph( face->glyph, FT_RENDER_MODE_MONO ))) {
        fprintf( stderr, "warning: failed FT_Render_Glyph 0x%x %d\n", glyph, err);
        return;
    }

    FT_Bitmap_New(&tempbitmap);
    FT_Bitmap_Convert( library, &slot->bitmap, &tempbitmap, 1);

    to_bitmap( &tempbitmap, *x + face->glyph->metrics.horiBearingX/64, *y - face->glyph->metrics.horiBearingY/64);
    FT_Bitmap_Done( library, &tempbitmap );
    
    // to_bitmap( &slot->bitmap, *x + slot->bitmap_left, *y - slot->bitmap_top );
}

static void out_xbm(int w, int h, int char_width, int char_height, const char* tag) {
    int x, y;
    // TODO: define image label?
    printf("#define %s_width %d\n", tag, w*8);
    printf("#define %s_height %d\n", tag, h);
    printf("#define %s_glyph_h %d\n", tag, char_height);
    printf("#define %s_glyph_w %d\n", tag, char_width);
    printf("static char %s_bits[] = {\n", tag);
    for (y=0; y < h; y++) {
        printf("\t");
        for (x=0; x < w; x++) {
            printf("0x%x, ", image[y*image_byte_width + x]);
        }
        printf("\n");
    }
    printf("\n}\n");
}

const char* or_null(const char* val, const char* els) {
    assert(els);
    if(val) {
        return val;
    } else {
        return els;
    }
}

int main(int argc, char **argv) {
    char *filename, *tag;
    int x = 0, y = 0;
    int g;

    memset (image, 0, image_byte_width*image_height);

    if (argc < 3) {
        fprintf( stderr, "usage: font2c [font] [tag]\n");
        exit(1);
    }
    filename = argv[1];
    tag = argv[2];

    if ((err = FT_Init_FreeType( &library ))) {
        fprintf( stderr, "error: Init_Freetype failed %d\n", err);
        exit(1);
    }
    if ((err = FT_New_Face( library, filename, 0, &face ))) {
        fprintf( stderr, "error: FT_New_Face failed %d\n", err);
        exit(1);
    }

    fprintf(stderr, "Loaded font %s - %s\n", or_null(face->family_name, "[FACE]"), or_null(face->style_name, "[STYLE]"));

    if (face->face_flags & FT_FACE_FLAG_FIXED_SIZES) { 
        int size = -1;
        fprintf(stderr, "Found %d fixed sizes in font\n", face->num_fixed_sizes); 
        for(int size_i=0;size_i<face->num_fixed_sizes; size_i++) {
            int height = face->available_sizes[size_i].height;
            int width = face->available_sizes[size_i].width;
            fprintf(stderr, "Size %d: width: %d height: %d\n", size_i, width, height);
            if (size == -1) {
                size = size_i;
            }
        }
  
        if (size == -1){
           fprintf(stderr, "Fixed font with no sizes\n");
           exit(1);
        }

        fprintf(stderr, "Selecting size: %d\n", size);
        FT_Select_Size(face, size);
    } else {
        FT_Size_RequestRec req;
        memset(&req, 0, sizeof(req));
        req.type = FT_SIZE_REQUEST_TYPE_CELL;
        req.height=28*64;
        fprintf(stderr, "Font is not bitmap, using size for height=%d\n", (int)req.height / 64);
        FT_Request_Size(face, &req);
    }

    int char_height = (face->size->metrics.height+63) / 64;
    int char_width = (face->size->metrics.max_advance+63) / 64;

    fprintf(stderr, "Chosen size: width: %d, height: %d\n", char_width, char_height);

    // 16x16 glyph grid
    image_width = char_width * 16;
    image_byte_width = image_width / 8;
    image_height = char_height * 16;
    image = (unsigned char*)malloc(sizeof(unsigned char)*image_byte_width*image_height);
    memset(image, 0, sizeof(unsigned char) * image_byte_width*image_height);
    fprintf(stderr, "image size: %dx%d\n", image_width, image_height);

    for (g = 0; g < 256; g++) {
        if (x+char_width >= image_width) {
                x = 0;
                y += char_height;
        }
        draw_glyph(g, &x, &y);
        x += char_width;
    }

    fprintf(stderr, "Outputting XBM format with tag: %s\n", tag);
    out_xbm(image_byte_width, image_height, char_width, char_height, tag);
    free(image);
    FT_Done_Face( face );
    FT_Done_FreeType( library );

    return 0;
}
