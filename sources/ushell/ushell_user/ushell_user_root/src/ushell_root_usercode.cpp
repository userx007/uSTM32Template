#include "ushell_core_utils.h"
#include "ushell_core_printout.h"
#include "ushell_core_settings.h"

#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

/*
Note:
    The definitios of num8_t, num16_t num32_t, num64_t are declared in:
    ..sources\ushell_settings\inc\ushell_core_settings.h
    and can be extended or adapted e.g. to signed variants, according to the user's needs
*/

#define SHELLFCT_RETVAL_ERR 0xFFU


///////////////////////////////////////////////////////////////////
//                  USER'S FUNCTIONS                             //
///////////////////////////////////////////////////////////////////


/*---------------------------------------------------------------*/
int vtest(void)
{
    uSHELL_PRINTF("--> vtest()\n");

    return 0;
}

/*---------------------------------------------------------------*/
int vhexlify(void)
{
    int iRetVal = SHELLFCT_RETVAL_ERR;

    uSHELL_PRINTF("--> vhexlify()\n");

    #define TEST_LEN 16U
    const uint8_t pu8InBuf[TEST_LEN] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    char *pstrOutBuf = (char*)malloc(TEST_LEN*2 + 1);

    if (nullptr != pstrOutBuf) {
        for (unsigned int i = 0; i < TEST_LEN; ++i) {
            uSHELL_PRINTF("%d : %d (0x%02X)\n", i, pu8InBuf[i], pu8InBuf[i]);
        }

        hexlify(pu8InBuf, TEST_LEN, pstrOutBuf);
        uSHELL_PRINTF("result: [%s]\n", pstrOutBuf);
        free(pstrOutBuf);
        iRetVal = 0;
    } else {
        uSHELL_PRINTF("malloc failed\n");
    }

    return iRetVal;
}

/*---------------------------------------------------------------*/
int itest(uint32_t i)
{
    uSHELL_PRINTF("--> itest()\n");
    uSHELL_PRINTF("i = %u\n", i );

    return 0;
}

/*---------------------------------------------------------------*/
int stest(char *s)
{
    uSHELL_PRINTF("--> stest()\n");
    uSHELL_PRINTF("s = %s\n", s );

    return 0;
}

/*---------------------------------------------------------------*/
int sunhexlify(char *s)
{
    int iRetVal = SHELLFCT_RETVAL_ERR;

    uSHELL_PRINTF("--> sunhexlify()\n");

    size_t szLen = strlen(s);
    if (0 != szLen) {
        uint8_t *pu8Buf = (uint8_t*)malloc(szLen/2 + 1);

        if (nullptr != pu8Buf) {
            size_t szOutLen = 0;

            if (unhexlify(s, pu8Buf, &szOutLen)) {
                for (unsigned int i = 0; i < szOutLen; ++i) {
                    uSHELL_PRINTF("%d : %d (0x%02X)\n", i, pu8Buf[i], pu8Buf[i]);
                }
                iRetVal = 0;
            } else {
                uSHELL_PRINTF("unhexlify failed (len || content)\n");
            }
            free(pu8Buf);
        } else {
            uSHELL_PRINTF("malloc failed\n");
        }
    } else {
        uSHELL_PRINTF("empty string\n");
    }

    return iRetVal;
}

/*---------------------------------------------------------------*/
int iitest(uint32_t i1, uint32_t i2)
{
    uSHELL_PRINTF("--> iitest()\n");
    uSHELL_PRINTF("i1 = %d\n", i1 );
    uSHELL_PRINTF("i2 = %d\n", i2 );

    return 0;
}

/*---------------------------------------------------------------*/
int istest(uint32_t i, char *s)
{
    uSHELL_PRINTF("--> istest()\n");
    uSHELL_PRINTF("i = %d\n", i );
    uSHELL_PRINTF("s = %s\n", s );

    return 0;
}

/*---------------------------------------------------------------*/
int sstest(char *s1, char *s2)
{
    uSHELL_PRINTF("--> sstest()\n");
    uSHELL_PRINTF("s1 = %s\n", s1 );
    uSHELL_PRINTF("s2 = %s\n", s2 );

    return 0;
}

/*---------------------------------------------------------------*/
int liotest(uint64_t l, uint32_t i, bool o)
{
    uSHELL_PRINTF("--> liotest()\n");
    uSHELL_PRINTF("l = %ld\n", l );
    uSHELL_PRINTF("i = %d\n", i );
    uSHELL_PRINTF("o = %d\n", o );

    return 0;
}


///////////////////////////////////////////////////////////////////
//               USER SHORTCUTS HANDLERS                         //
///////////////////////////////////////////////////////////////////


#if (1 == uSHELL_IMPLEMENTS_USER_SHORTCUTS)

void uShellUserHandleShortcut_Dot( const char *pstrArgs )
{
    uSHELL_PRINTF("[.] registered but not implemented | args[%s]\n", pstrArgs);

} /* uShellUserHandleShortcut_Dot() */


/******************************************************************************/
void uShellUserHandleShortcut_Slash( const char *pstrArgs )
{
    uSHELL_PRINTF("[/] registered but not implemented | args[%s]\n", pstrArgs);

} /* uShellUserHandleShortcut_Slash() */

#endif /*(1 == uSHELL_IMPLEMENTS_USER_SHORTCUTS)*/
