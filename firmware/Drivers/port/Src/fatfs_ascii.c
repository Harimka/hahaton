/**
 * @file    fatfs_ascii.c
 * @author  team-04
 * @brief   ASCII-only LFN conversion for application-generated SD filenames.
 */
#include "ff.h"

/* Non-ASCII names are deliberately unsupported. Existing files remain accessible
 * by their FAT short aliases. Application filenames use ASCII exclusively. */
WCHAR ff_convert(WCHAR character, UINT direction)
{
    (void)direction;
    return character < 0x80U ? character : 0U;
}

WCHAR ff_wtoupper(WCHAR character)
{
    return character >= 'a' && character <= 'z'
        ? (WCHAR)(character - 'a' + 'A') : character;
}
/* END OF FILE */
