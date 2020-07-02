/* KallistiOS ##version##

   dc/biosfont.h
   (c)2000-2001 Dan Potter
   Japanese Functions (c)2002 Kazuaki Matsumoto
   (c)2017 Donald Haase

*/

/** \file   dc/biosfont.h
    \brief  BIOS font drawing functions.

    This file provides support for utilizing the font built into the Dreamcast's
    BIOS. These functions allow access to both the western character set and
    Japanese characters.

    \author Dan Potter
    \author Kazuaki Matsumoto
    \author Donald Haase
*/

#ifndef __DC_BIOSFONT_H
#define __DC_BIOSFONT_H

#include <kos/cdefs.h>
__BEGIN_DECLS

#include <arch/types.h>

/** \defgroup bfont_size  Dimensions of the Bios Font
    @{
*/
#define BFONT_THIN_WIDTH                        12   /**< \brief Width of Thin Font (ISO8859_1, half-JP) */
#define BFONT_WIDE_WIDTH      BFONT_THIN_WIDTH * 2   /**< \brief Width of Wide Font (full-JP) */
#define BFONT_HEIGHT                            24   /**< \brief Height of All Fonts */
/** @} */

#define JISX_0208_ROW_SIZE  94
/** \defgroup bfont_indecies Structure of the Bios Font 
    @{
*/
#define BFONT_NARROW_START          0   /**< \brief Start of Narrow Characters in Font Block */
#define BFONT_OVERBAR               BFONT_NARROW_START 
#define BFONT_ISO_8859_1_33_126     BFONT_NARROW_START+( 1*BFONT_THIN_WIDTH*BFONT_HEIGHT/8) 
#define BFONT_YEN                   BFONT_NARROW_START+(95*BFONT_THIN_WIDTH*BFONT_HEIGHT/8) 
#define BFONT_ISO_8859_1_160_255    BFONT_NARROW_START+(96*BFONT_THIN_WIDTH*BFONT_HEIGHT/8) 

/* JISX-0208 Rows 1-7 and 16-84 are encoded between BFONT_WIDE_START and BFONT_DREAMCAST_SPECIFIC. 
    Only the box-drawing characters (row 8) are missing. */
#define BFONT_WIDE_START            (288*BFONT_THIN_WIDTH*BFONT_HEIGHT/8)   /**< \brief Start of Wide Characters in Font Block */
#define BFONT_JISX_0208_ROW1        BFONT_WIDE_START    /**< \brief Start of JISX-0208 Rows 1-7 in Font Block */
#define BFONT_JISX_0208_ROW16       BFONT_WIDE_START+(658*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)  /**< \brief Start of JISX-0208 Row 16-47 (Start of Level 1) in Font Block */
#define BFONT_JISX_0208_ROW48       BFONT_JISX_0208_ROW16+((32*JISX_0208_ROW_SIZE)*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8) /**< \brief JISX-0208 Row 48-84 (Start of Level 2) in Font Block */


#define BFONT_DREAMCAST_SPECIFIC    BFONT_WIDE_START+(7056*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8) /**< \brief Start of DC Specific Characters in Font Block */
#define BFONT_CIRCLECOPYRIGHT       BFONT_DREAMCAST_SPECIFIC+(0*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_CIRCLER               BFONT_DREAMCAST_SPECIFIC+(1*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_TRADEMARK             BFONT_DREAMCAST_SPECIFIC+(2*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_UPARROW               BFONT_DREAMCAST_SPECIFIC+(3*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_DOWNARROW             BFONT_DREAMCAST_SPECIFIC+(4*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_LEFTARROW             BFONT_DREAMCAST_SPECIFIC+(5*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_RIGHTARROW            BFONT_DREAMCAST_SPECIFIC+(6*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_UPRIGHTARROW          BFONT_DREAMCAST_SPECIFIC+(7*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_DOWNRIGHTARROW        BFONT_DREAMCAST_SPECIFIC+(8*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_DOWNLEFTARROW         BFONT_DREAMCAST_SPECIFIC+(9*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_UPLEFTARROW           BFONT_DREAMCAST_SPECIFIC+(10*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_ABUTTON               BFONT_DREAMCAST_SPECIFIC+(11*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_BBUTTON               BFONT_DREAMCAST_SPECIFIC+(12*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_CBUTTON               BFONT_DREAMCAST_SPECIFIC+(13*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_DBUTTON               BFONT_DREAMCAST_SPECIFIC+(14*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_XBUTTON               BFONT_DREAMCAST_SPECIFIC+(15*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_YBUTTON               BFONT_DREAMCAST_SPECIFIC+(16*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_ZBUTTON               BFONT_DREAMCAST_SPECIFIC+(17*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_LTRIGGER              BFONT_DREAMCAST_SPECIFIC+(18*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_RTRIGGER              BFONT_DREAMCAST_SPECIFIC+(19*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_STARTBUTTON           BFONT_DREAMCAST_SPECIFIC+(20*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)
#define BFONT_VMUICON               BFONT_DREAMCAST_SPECIFIC+(21*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)

#define VICON_DIMEN                 32    /**< \brief Dimension of vmu icons */            
#define VICON_DREAMCAST_SPECIFIC    BFONT_DREAMCAST_SPECIFIC+(22*BFONT_WIDE_WIDTH*BFONT_HEIGHT/8)

#define VICON_INVALID_VMU           0x00
#define VICON_HOURGLASS_ONE         0x01
#define VICON_HOURGLASS_TWO         0x02
#define VICON_HOURGLASS_THREE       0x03
#define VICON_HOURGLASS_FOUR        0x04
#define VICON_VMUICON               0x05
#define VICON_EARTH                 0x06
#define VICON_SATURN                0x07
#define VICON_QUARTER_MOON          0x08
#define VICON_LAUGHING_FACE         0x09
#define VICON_SMILING_FACE          0x0A
#define VICON_CASUAL_FACE           0x0B
#define VICON_ANGRY_FACE            0x0C
#define VICON_COW                   0x0D
#define VICON_HORSE                 0x0E
#define VICON_RABBIT                0x0F
#define VICON_CAT                   0x10
#define VICON_CHICK                 0x11
#define VICON_LION                  0x12
#define VICON_MONKEY                0x13
#define VICON_PANDA                 0x14
#define VICON_BEAR                  0x15
#define VICON_PIG                   0x16
#define VICON_DOG                   0x17
#define VICON_FISH                  0x18
#define VICON_OCTOPUS               0x19
#define VICON_SQUID                 0x1A
#define VICON_WHALE                 0x1B
#define VICON_CRAB                  0x1C
#define VICON_BUTTERFLY             0x1D
#define VICON_LADYBUG               0x1E
#define VICON_ANGLER_FISH           0x1F
#define VICON_PENGUIN               0x20
#define VICON_CHERRIES              0x21
#define VICON_TULIP                 0x22
#define VICON_LEAF                  0x23
#define VICON_SAKURA                0x24
#define VICON_APPLE                 0x25
#define VICON_ICECREAM              0x26
#define VICON_CACTUS                0x27
#define VICON_PIANO                 0x28
#define VICON_GUITAR                0x29
#define VICON_EIGHTH_NOTE           0x2A
#define VICON_TREBLE_CLEF           0x2B
#define VICON_BOAT                  0x2C
#define VICON_CAR                   0x2D
#define VICON_HELMET                0x2E
#define VICON_MOTORCYCLE            0x2F
#define VICON_VAN                   0x30
#define VICON_TRUCK                 0x31
#define VICON_CLOCK                 0x32
#define VICON_TELEPHONE             0x33
#define VICON_PENCIL                0x34
#define VICON_CUP                   0x35
#define VICON_SILVERWARE            0x36
#define VICON_HOUSE                 0x37
#define VICON_BELL                  0x38
#define VICON_CROWN                 0x39
#define VICON_SOCK                  0x3A
#define VICON_CAKE                  0x3B
#define VICON_KEY                   0x3C
#define VICON_BOOK                  0x3D
#define VICON_BASEBALL              0x3E
#define VICON_SOCCER                0x3F
#define VICON_BULB                  0x40
#define VICON_TEDDY_BEAR            0x41
#define VICON_BOW_TIE               0x42
#define VICON_BOW_ARROW             0x43
#define VICON_SNOWMAN               0x44
#define VICON_LIGHTNING             0x45
#define VICON_SUN                   0x46
#define VICON_CLOUD                 0x47
#define VICON_UMBRELLA              0x48
#define VICON_ONE_STAR              0x49
#define VICON_TWO_STARS             0x4A
#define VICON_THREE_STARS           0x4B
#define VICON_FOUR_STARS            0x4C
#define VICON_HEART                 0x4D
#define VICON_DIAMOND               0x4E
#define VICON_SPADE                 0x4F
#define VICON_CLUB                  0x50
#define VICON_JACK                  0x51
#define VICON_QUEEN                 0x52
#define VICON_KING                  0x53
#define VICON_JOKER                 0x54
#define VICON_ISLAND                0x55
#define VICON_0                     0x56
#define VICON_1                     0x57
#define VICON_2                     0x58
#define VICON_3                     0x59
#define VICON_4                     0x5A
#define VICON_5                     0x5B
#define VICON_6                     0x5C
#define VICON_7                     0x5D
#define VICON_8                     0x5E
#define VICON_9                     0x5F
#define VICON_A                     0x60
#define VICON_B                     0x61
#define VICON_C                     0x62
#define VICON_D                     0x63
#define VICON_E                     0x64
#define VICON_F                     0x65
#define VICON_G                     0x66
#define VICON_H                     0x67
#define VICON_I                     0x68
#define VICON_J                     0x69
#define VICON_K                     0x6A
#define VICON_L                     0x6B
#define VICON_M                     0x6C
#define VICON_N                     0x6D
#define VICON_O                     0x6E
#define VICON_P                     0x6F
#define VICON_Q                     0x70
#define VICON_R                     0x71
#define VICON_S                     0x72
#define VICON_T                     0x73
#define VICON_U                     0x74
#define VICON_V                     0x75
#define VICON_W                     0x76
#define VICON_X                     0x77
#define VICON_Y                     0x78
#define VICON_Z                     0x79
#define VICON_CHECKER_BOARD         0x7A
#define VICON_GRID                  0x7B
#define VICON_LIGHT_GRAY            0x7C
#define VICON_DIAG_GRID             0x7D
#define VICON_PACMAN_GRID           0x7E
#define VICON_DARK_GRAY             0x7F
#define VICON_EMBROIDERY            0x80


/** @} */

/** \brief  Set the font foreground color.

    This function selects the foreground color to draw when a pixel is opaque in
    the font. The value passed in for the color should be in whatever pixel
    format that you intend to use for the image produced.

    \param  c               The color to use.
    \return                 The old foreground color.
*/
uint32 bfont_set_foreground_color(uint32 c);

/** \brief  Set the font background color.

    This function selects the background color to draw when a pixel is drawn in
    the font. This color is only used for pixels not covered by the font when
    you have selected to have the font be opaque.

    \param  c               The color to use.
    \return                 The old background color.
*/
uint32 bfont_set_background_color(uint32 c);

/** \brief  Set the font to draw 32-bit color.

    This function changes whether the font draws colors as 32-bit or 16-bit. The
    default is to use 16-bit.

    \param  on              Set to 0 to use 16-bit color, 32-bit otherwise.
    \return                 The old state (1 = 32-bit, 0 = 16-bit).
*/
int bfont_set_32bit_mode(int on) __depr("Please use the bpp function of the the bfont_draw_ex functions");

/* Constants for the function below */
#define BFONT_CODE_ISO8859_1    0   /**< \brief ISO-8859-1 (western) charset */
#define BFONT_CODE_EUC          1   /**< \brief EUC-JP charset */
#define BFONT_CODE_SJIS         2   /**< \brief Shift-JIS charset */
#define BFONT_CODE_RAW          3   /**< \brief Raw indexing to the BFONT */

/** \brief  Set the font encoding.

    This function selects the font encoding that is used for the font. This
    allows you to select between the various character sets available.

    \param  enc             The character encoding in use
    \see    BFONT_CODE_ISO8859_1
    \see    BFONT_CODE_EUC
    \see    BFONT_CODE_SJIS
    \see    BFONT_CODE_RAW
*/
void bfont_set_encoding(uint8 enc);

/** \brief  Find an ISO-8859-1 character in the font.

    This function retrieves a pointer to the font data for the specified
    character in the font, if its available. Generally, you will not have to
    use this function, use one of the bfont_draw_* functions instead.

    \param  ch              The character to look up
    \return                 A pointer to the raw character data
*/
uint8 *bfont_find_char(uint32 ch);

/** \brief  Find an full-width Japanese character in the font.

    This function retrieves a pointer to the font data for the specified
    character in the font, if its available. Generally, you will not have to
    use this function, use one of the bfont_draw_* functions instead.

    This function deals with full-width kana and kanji.

    \param  ch              The character to look up
    \return                 A pointer to the raw character data
*/
uint8 *bfont_find_char_jp(uint32 ch);

/** \brief  Find an half-width Japanese character in the font.

    This function retrieves a pointer to the font data for the specified
    character in the font, if its available. Generally, you will not have to
    use this function, use one of the bfont_draw_* functions instead.

    This function deals with half-width kana only.

    \param  ch              The character to look up
    \return                 A pointer to the raw character data
*/
uint8 *bfont_find_char_jp_half(uint32 ch);

/** \brief Draw a single character of any sort to the buffer.

    This function draws a single character in the set encoding to the given 
    buffer. This function sits under draw, draw_thin, and draw_wide, while 
    exposing the colors and bitdepths desired. This will allow the writing 
    of bfont characters to palletted textures.
    
    \param buffer       The buffer to draw to.
    \param bufwidth     The width of the buffer in pixels.
    \param fg           The foreground color to use.
    \param bg           The background color to use.
    \param bpp          The number of bits per pixel in the buffer.
    \param opaque       If non-zero, overwrite background areas with black,
                            otherwise do not change them from what they are.
    \param c            The character to draw.
    \param wide         Draw a wide character.
    \param iskana       Draw a half-width kana character.    
    \return             Amount of width covered in bytes.
*/
unsigned char bfont_draw_ex(uint8 *buffer, uint32 bufwidth, uint32 fg, uint32 bg, uint8 bpp, uint8 opaque, uint32 c, uint8 wide, uint8 iskana);

/** \brief  Draw a single character to a buffer.

    This function draws a single character in the set encoding to the given
    buffer. Calling this is equivalent to calling bfont_draw_thin() with 0 for
    the final parameter.

    \param  buffer          The buffer to draw to (at least 12 x 24 pixels)
    \param  bufwidth        The width of the buffer in pixels
    \param  opaque          If non-zero, overwrite blank areas with black,
                            otherwise do not change them from what they are
    \param  c               The character to draw
    \return                 Amount of width covered in bytes.
*/
unsigned char bfont_draw(void *buffer, uint32 bufwidth, uint8 opaque, uint32 c);

/** \brief  Draw a single thin character to a buffer.

    This function draws a single character in the set encoding to the given
    buffer. This only works with ISO-8859-1 characters and half-width kana.

    \param  buffer          The buffer to draw to (at least 12 x 24 pixels)
    \param  bufwidth        The width of the buffer in pixels
    \param  opaque          If non-zero, overwrite blank areas with black,
                            otherwise do not change them from what they are
    \param  c               The character to draw
    \param  iskana          Set to 1 if the character is a kana, 0 if ISO-8859-1
    \return                 Amount of width covered in bytes.
*/
unsigned char bfont_draw_thin(void *buffer, uint32 bufwidth, uint8 opaque, uint32 c, uint8 iskana);

/** \brief  Draw a single wide character to a buffer.

    This function draws a single character in the set encoding to the given
    buffer. This only works with full-width kana and kanji.

    \param  buffer          The buffer to draw to (at least 24 x 24 pixels)
    \param  bufwidth        The width of the buffer in pixels
    \param  opaque          If non-zero, overwrite blank areas with black,
                            otherwise do not change them from what they are
    \param  c               The character to draw
    \return                 Amount of width covered in bytes.
*/
unsigned char bfont_draw_wide(void *buffer, uint32 bufwidth, uint8 opaque, uint32 c);

/** \brief  Draw a full string to any sort of buffer.

    This function draws a NUL-terminated string in the set encoding to the given
    buffer. This will automatically handle mixed half and full-width characters
    if the encoding is set to one of the Japanese encodings. Colors and bitdepth 
    can be set.

    \param b       The buffer to draw to.
    \param width     The width of the buffer in pixels.
    \param fg           The foreground color to use.
    \param bg           The background color to use.
    \param bpp          The number of bits per pixel in the buffer.
    \param opaque       If non-zero, overwrite background areas with black,
                            otherwise do not change them from what they are.
    \param str          The string to draw.
    
*/
void bfont_draw_str_ex(void *b, uint32 width, uint32 fg, uint32 bg, uint8 bpp, uint8 opaque, char *str);

/** \brief  Draw a full string to a buffer.

    This function draws a NUL-terminated string in the set encoding to the given
    buffer. This will automatically handle mixed half and full-width characters
    if the encoding is set to one of the Japanese encodings. Draws pre-set 
    16-bit colors.

    \param  b          The buffer to draw to
    \param  width           The width of the buffer in pixels
    \param  opaque          If one, overwrite blank areas with bfont_bgcolor,
                            otherwise do not change them from what they are
    \param  str             The string to draw
*/
void bfont_draw_str(void *b, uint32 width, uint8 opaque, char *str);

/** \brief  Find a vmu icon.

    This function retrieves a pointer to the icon data for the specified
    vmu icon in the bios, if its available. The icon data is flipped both
    vertically and horizontally. Each vmu icon has dimens 32x32 
    and is 128 bytes long.

    \param  icon            The icon to look up. Use VICON_* values starting with 
                            VICON_INVALID_VMU
    \return                 A pointer to the raw icon data or NULL if icon value is 
                            incorrect
*/
uint8 *vicon_find_icon(uint8 icon);

__END_DECLS

#endif  /* __DC_BIOSFONT_H */
