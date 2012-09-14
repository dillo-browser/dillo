#ifndef __BINARYCONST_H__
#define __BINARYCONST_H__

/* binaryconst.h was integrated into the Dillo project in April 2004, and
 * presumably comes from the ancestor of the code found at
 * http://cprog.tomsweb.net/binconst.txt
 */

/* Macros for allowing binary constants in C
 * By Tom Torfs - donated to the public domain */

#define HEX__(n) 0x##n##LU

/* 8-bit conversion function */
#define B8__(x) ((x&0x0000000FLU)?1:0)  \
               +((x&0x000000F0LU)?2:0)  \
               +((x&0x00000F00LU)?4:0)  \
               +((x&0x0000F000LU)?8:0)  \
               +((x&0x000F0000LU)?16:0) \
               +((x&0x00F00000LU)?32:0) \
               +((x&0x0F000000LU)?64:0) \
               +((x&0xF0000000LU)?128:0)


/*
 * *** USER MACROS ***
 */

/* for upto 8-bit binary constants */
#define B8(d) ((unsigned char)B8__(HEX__(d)))

/* for upto 16-bit binary constants, MSB first */
#define B16(dmsb,dlsb) (((unsigned short)B8(dmsb)<<8) + B8(dlsb))

/*
 * Sample usage:
 *    B8(01010101) = 85
 *    B16(10101010,01010101) = 43605
 */


#endif /* __BINARYCONST_H__ */

