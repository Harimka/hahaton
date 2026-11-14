/**
 * @file    ffconf.h
 * @author  team-04
 * @brief   FatFs configuration file for STM32G474RE embedded shock logger.
 *          Configured for static allocation (zero malloc), single drive, 512-byte sectors.
 */

#ifndef FFCONF_H
#define FFCONF_H

#define _FFCONF 68300   /* Revision ID */

/* Function Configurations */
#define _FS_READONLY    0   /* 0: Read/Write */
#define _FS_MINIMIZE    0   /* 0: Full basic API enabled */
#define _USE_STRFUNC    0   /* 0: Disable string functions (we write preformatted buffers) */
#define _USE_FIND       0   /* 0: Disable filtered directory search */
#define _USE_MKFS       1   /* 1: Enable f_mkfs */
#define _USE_FASTSEEK   0   /* 0: Disable fast seek */
#define _USE_EXPAND     0   /* 0: Disable f_expand */
#define _USE_CHMOD      0   /* 0: Disable chmod */
#define _USE_LABEL      0   /* 0: Disable volume label */
#define _USE_FORWARD    0   /* 0: Disable forward */

/* Locale and Namespace */
#define _CODE_PAGE      850 /* Latin 1 */
#define _USE_LFN        0   /* 0: 8.3 filenames (e.g. LOG.TXT, IMU_LOG.CSV) - ZERO HEAP / MALLOC! */
#define _MAX_LFN        255
#define _LFN_UNICODE    0
#define _STRF_ENCODE    3
#define _FS_RPATH       0   /* 0: Disable relative paths */

/* Drive/Volume Configurations */
#define _VOLUMES        1   /* 1 logical drive (MicroSD) */
#define _STR_VOLUME_ID  0
#define _VOLUME_STRS    "SD"
#define _MULTI_PARTITION 0
#define _MIN_SS         512 /* Standard SD block size */
#define _MAX_SS         512

#define _USE_TRIM       0
#define _FS_NOFSINFO    0

/* System Configurations */
#define _FS_TINY        0   /* Normal buffer */
#define _FS_EXFAT       0   /* Standard FAT16/FAT32 */
#define _FS_NORTC       1   /* 1: No hardware RTC required, fixed timestamp */
#define _NORTC_MON      11
#define _NORTC_MDAY     14
#define _NORTC_YEAR     2026

#define _FS_LOCK        0   /* 0: Disable lock (single-threaded superloop) */
#define _FS_REENTRANT   0   /* 0: Non-reentrant (bare metal superloop) */

#endif /* FFCONF_H */
/***************************** END OF FILE ************************************/
