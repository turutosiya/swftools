/* swffont.c

   Functions for loading external fonts.

   Extension module for the rfxswf library.
   Part of the swftools package.

   Copyright (c) 2003, 2004 Matthias Kramm
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifdef USE_FREETYPE

#include <freetype/freetype.h>
#include <freetype/ftglyph.h>
#include <freetype/ftsnames.h>
#include <freetype/ttnameid.h>
#include <freetype/ftoutln.h>

static int ft_move_to(FT_Vector* _to, void* user) 
{
    drawer_t* draw = (drawer_t*)user;
    FPOINT to;
    to.x = _to->x/256.0;
    to.y = _to->y/256.0;
    draw->moveTo(draw, &to);
    return 0;
}
static int ft_line_to(FT_Vector* _to, void* user) 
{
    drawer_t* draw = (drawer_t*)user;
    FPOINT to;
    to.x = _to->x/256.0;
    to.y = _to->y/256.0;
    draw->lineTo(draw, &to);
    return 0;
}
static int ft_cubic_to(FT_Vector* _c1, FT_Vector* _c2, FT_Vector* _to, void* user)
{
    drawer_t* draw = (drawer_t*)user;
    FPOINT c1,c2,to;
    to.x = _to->x/256.0;
    to.y = _to->y/256.0;
    c1.x = _c1->x/256.0;
    c1.y = _c1->y/256.0;
    c2.x = _c2->x/256.0;
    c2.y = _c2->y/256.0;
    draw_cubicTo(draw, &c1, &c2, &to);
    return 0;
}
static int ft_conic_to(FT_Vector* _c, FT_Vector* _to, void* user) 
{
    drawer_t* draw = (drawer_t*)user;
    FPOINT c,to;
    to.x = _to->x/256.0;
    to.y = _to->y/256.0;
    c.x = _c->x/256.0;
    c.y = _c->y/256.0;
    draw_conicTo(draw, &c, &to);
    return 0;
}
static FT_Outline_Funcs outline_functions =
{
  ft_move_to,
  ft_line_to,
  ft_conic_to,
  ft_cubic_to,
  0,0
};

static FT_Library ftlibrary = 0;

SWFFONT* swf_LoadTrueTypeFont(char*filename)
{
    FT_Face face;
    FT_Error error;
    const char* name;
    FT_ULong charcode;
    FT_UInt gindex;
    SWFFONT* font;
    int t;
   
    if(ftlibrary == 0) {
	if(FT_Init_FreeType(&ftlibrary)) {
	    fprintf(stderr, "Couldn't init freetype library!\n");
	    exit(1);
	}
    }
    error = FT_New_Face(ftlibrary, filename, 0, &face);
    if(error) {
	fprintf(stderr, "Couldn't load file %s- not a TTF file?\n", filename);
	return 0;
    }
    if(face->num_glyphs <= 0)
	return 0;

    font = malloc(sizeof(SWFFONT));
    memset(font, 0, sizeof(SWFFONT));
    font->id = -1;
    font->version = 2;
    font->layout = malloc(sizeof(SWFLAYOUT));
    memset(font->layout, 0, sizeof(SWFLAYOUT));
    font->layout->bounds = malloc(face->num_glyphs*sizeof(SRECT));
    font->numchars = face->num_glyphs;
    font->style =  ((face->style_flags&FT_STYLE_FLAG_ITALIC)?FONT_STYLE_ITALIC:0)
	          |((face->style_flags&FT_STYLE_FLAG_BOLD)?FONT_STYLE_BOLD:0);
    font->encoding = FONT_ENCODING_UNICODE;
    font->glyph2ascii = malloc(face->num_glyphs*sizeof(U16));
    font->maxascii = 0;
    memset(font->ascii2glyph, 0, font->maxascii*sizeof(int));
    font->glyph = malloc(face->num_glyphs*sizeof(SWFGLYPH));
    memset(font->glyph, 0, face->num_glyphs*sizeof(U16));
    if(FT_HAS_GLYPH_NAMES(face)) {
	font->glyphnames = malloc(face->num_glyphs*sizeof(char*));
    }

    font->layout->ascent = face->ascender; //face->bbox.xMin;
    font->layout->descent = face->descender; //face->bbox.xMax;
    font->layout->leading = -face->bbox.xMin;
    font->layout->kerningcount = 0;
    
    if(name && *name)
	font->name = (U8*)strdup(FT_Get_Postscript_Name(face));

/*    // Map Glyphs to Unicode, version 1 (quick and dirty):
    int t;
    for(t=0;t<65536;t++) {
	int index = FT_Get_Char_Index(face, t);
	if(index>=0 && index<face->num_glyphs) {
	    if(font->glyph2ascii[index]<0)
		font->glyph2ascii[index] = t;
	}
    }*/
    
    // Map Glyphs to Unicode, version 2 (much nicer):
    // (The third way would be the AGL algorithm, as proposed
    //  by Werner Lemberg on freetype@freetype.org)

    charcode = FT_Get_First_Char(face, &gindex);
    while(gindex != 0)
    {
	if(gindex >= 0 && gindex<face->num_glyphs) {
	    if(!font->glyph2ascii[gindex]) {
		font->glyph2ascii[gindex] = charcode;
		if(charcode + 1 > font->maxascii) {
		    font->maxascii = charcode + 1;
		}
	    }
	}
	charcode = FT_Get_Next_Char(face, charcode, &gindex);
    }
    
    memset(font->glyph2ascii, 0, face->num_glyphs*sizeof(U16));
    font->ascii2glyph = malloc(font->maxascii*sizeof(int));
    
    for(t=0;t<font->maxascii;t++)
	font->ascii2glyph[t] = FT_Get_Char_Index(face, t);

    for(t=0; t < face->num_glyphs; t++) {
	FT_Glyph glyph;
	FT_BBox bbox;
	char name[128];
	drawer_t draw;
	name[0]=0;
	if(FT_HAS_GLYPH_NAMES(face)) {
	    error = FT_Get_Glyph_Name(face, t, name, 127);
	    if(!error) 
		font->glyphnames[t] = strdup(name);
	}
	error = FT_Load_Glyph(face, t, FT_LOAD_NO_BITMAP|FT_LOAD_NO_SCALE);
	if(error) return 0;
	error = FT_Get_Glyph(face->glyph, &glyph);
	if(error) return 0;

	FT_Glyph_Get_CBox(glyph, ft_glyph_bbox_unscaled, &bbox);

	swf_Shape01DrawerInit(&draw, 0);

	error = FT_Outline_Decompose(&face->glyph->outline, &outline_functions, &draw);
	if(error) return 0;

	draw.finish(&draw);

	font->glyph[t].advance = glyph->advance.x*20/256;
	font->glyph[t].shape = swf_ShapeDrawerToShape(&draw);
	//swf_ShapeDrawerGetBBox(&draw);
	draw.dealloc(&draw);
    
	font->layout->bounds[t].xmin = (bbox.xMin*5*20)/266;
	font->layout->bounds[t].ymin = (bbox.yMin*5*20)/266;
	font->layout->bounds[t].xmax = (bbox.xMax*5*20)/266;
	font->layout->bounds[t].ymax = (bbox.yMax*5*20)/266;

	FT_Done_Glyph(glyph);
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ftlibrary);ftlibrary=0;

    return font;
}
#else  //USE_FREETYPE

SWFFONT* swf_LoadTrueTypeFont(char*filename)
{
    fprintf(stderr, "Warning: no freetype library- not able to load %s\n", filename);
    return 0;
}

#endif

#ifdef HAVE_T1LIB

#include <t1lib.h>

SWFFONT* swf_LoadT1Font(char*filename)
{
    SWFFONT * font;
    int nr;
    float angle,underline;
    char*fontname,*fullname,*familyname;
    BBox bbox;
    int s,num;
    char*encoding[256];
    char**charnames;
    char*charname;

    T1_SetBitmapPad( 16);
    if ((T1_InitLib(NO_LOGFILE)==NULL)){
	fprintf(stderr, "Initialization of t1lib failed\n");
	return 0;
    }
    nr = T1_AddFont(filename);
    T1_LoadFont(nr);

    num = T1_SetDefaultEncoding(encoding);
    for(;num<256;num++) encoding[num] = 0;

    charnames = T1_GetAllCharNames(nr);

    angle = T1_GetItalicAngle(nr);
    fontname = T1_GetFontName(nr);
    fullname = T1_GetFullName(nr);
    familyname = T1_GetFamilyName(nr);
    underline = T1_GetUnderlinePosition(nr);
    bbox = T1_GetFontBBox(nr);

    font = (SWFFONT*)malloc(sizeof(SWFFONT));
    memset(font, 0, sizeof(SWFFONT));

    font->version = 2;
    font->name = (U8*)strdup(fontname);
    font->layout = (SWFLAYOUT*)malloc(sizeof(SWFLAYOUT));
    memset(font->layout, 0, sizeof(SWFLAYOUT));

    num = 0;
    charname = charnames[0];
    while(*charname) {
	charname++;
	num++;
    }

    font->maxascii = 256;
    font->numchars = num;
    
    font->style = (/*bold*/0?FONT_STYLE_BOLD:0) + (angle>0.05?FONT_STYLE_ITALIC:0);

    font->glyph = (SWFGLYPH*)malloc(num*sizeof(SWFGLYPH));
    memset(font->glyph, 0, num*sizeof(SWFGLYPH));
    font->glyph2ascii = (U16*)malloc(num*sizeof(U16));
    memset(font->glyph2ascii, 0, num*sizeof(U16));
    font->ascii2glyph = (int*)malloc(font->maxascii*sizeof(int));
    memset(font->ascii2glyph, -1, font->maxascii*sizeof(int));
    font->layout->ascent = (U16)(underline - bbox.lly);
    font->layout->descent = (U16)(bbox.ury - underline);
    font->layout->leading = (U16)(font->layout->ascent - 
	                     font->layout->descent -
			     (bbox.lly - bbox.ury));
    font->layout->bounds = (SRECT*)malloc(sizeof(SRECT)*num);
    memset(font->layout->bounds, 0, sizeof(SRECT)*num);
    font->layout->kerningcount = 0;
    font->layout->kerning = 0;
  
    num = 0;
    
    charname = charnames[0];
    while(*charname) {
	int c;
	T1_OUTLINE * outline = T1_GetCharOutline(nr, c, 100.0, 0);
	int firstx = outline->dest.x/0xffff;

	font->ascii2glyph[s] = num;
	font->glyph2ascii[num] = s;
	    
	/* fix bounding box */
	SHAPE2*shape2;
	SRECT bbox;
	shape2 = swf_ShapeToShape2(font->glyph[s].shape);
	if(!shape2) { fprintf(stderr, "Shape parse error\n");exit(1);}
	bbox = swf_GetShapeBoundingBox(shape2);
	swf_Shape2Free(shape2);
	font->layout->bounds[num] = bbox;
	//font->glyph[num].advance = (int)(width/6.4); // 128/20
	font->glyph[num].advance = bbox.xmax/20;
	if(!font->glyph[num].advance) {
	    font->glyph[num].advance = firstx;
	}
    }
    return font;
}

#else

SWFFONT* swf_LoadT1Font(char*filename)
{
    fprintf(stderr, "Warning: no t1lib- not able to load %s\n", filename);
    return 0;
}

#endif

