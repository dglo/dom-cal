/*
 * Iceboot utilities borrowed to allow dynamic switching of FPGA
 *
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "zlib.h"
#include "booter/epxa.h"
#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"


#include "../iceboot/fis.h"
#include "icebootUtils.h"

static int domapp_loaded = 0;

/******************************************************************************/
/* 
 * Return status of whether we have switched to the domapp FPGA
 *    0 = not loaded, 1 = loaded
 */
int is_domapp_loaded(void) { return domapp_loaded;}

/******************************************************************************/
/*
 * Load the domapp FPGA, replacing the STF FPGA used by default in iceboot
 * Returns 0 for success, nonzero for failure.
 */
int load_domapp_fpga(void) {

    /*------------------------------------------------------------------*/
    /* Find the FPGA file on the flash system */
    unsigned long data_length;
    char *filename = "domapp.sbi.gz";
    void *flash_base = fisLookup(filename, &data_length);

    if (flash_base==NULL) {
        printf("ERROR: could not find %s on flash filesystem.\r\n", filename);      
        return 1;
    }

    /* Gunzip the file */
    uLong zLen = 0;
    Bytef *zDest = NULL;
    gunzip(data_length, flash_base, &zLen, &zDest);

    if ((zLen <= 0) || (zDest == NULL)) {
        printf("ERROR: could not unzip FPGA file %s.\r\n", filename);
        return 1;
    }
    /* Switch to DOMAPP FPGA using routine stolen from iceboot */
    if (fpga_config((int *)zDest, zLen) != 0) {
        printf("ERROR: FPGA switch failed!\r\n");
        return 1;
    }
#ifdef DEBUG
    printf("Successfully switched to DOMAPP FPGA.\r\n");
#endif

    halUSleep(100000);

    /* Set static flag that we've switched FPGAs */
    domapp_loaded = 1;

    return 0;
}
/******************************************************************************/
/*
 * fpga_config code stolen from iceboot -- currently impossible to 
 * link in without restructuring iceboot code
 *
 * returns: 0 ok, non-zero error...
 */

int fpga_config(int *p, int nbytes) {
    int *cclk = (int *) (REGISTERS + 0x144);
    int *ccntl = (int *) (REGISTERS + 0x140);
    int *idcode = (int *) (REGISTERS + 0x8);
    int *cdata = (int *) (REGISTERS + 0x148);
    int i;
    unsigned interrupts = 0;

    /* check magic number...
     */
    if (p[0] != 0x00494253) {
        printf("invalid magic number...\n");
        return 4;
    }

    /* check buffer length */
    if ((nbytes%4) != 0 || nbytes<p[2]+p[3]) {
        printf("invalid buffer length\n");
        return 1;
    }

    /* check file length */
    if ((p[3]%4)!=0 || (p[2]%4)!=0) {
        printf("invalid file length/offset\n");
        return 1;
    }

    /* setup clock */
    *cclk = 4;

    /* check lock bit of ccntl */
    if ( (*(volatile int *)ccntl)&1 ) {
        int *cul = (int *) (REGISTERS + 0x14c);
        *(volatile int *) cul = 0x554e4c4b;
      
        if ((*(volatile int *)ccntl)&1 ) {
            printf("can't unlock config_control!\n");
            return 1;
        }
    }

    /* check idcode */
    if (*idcode!=p[1]) {
        printf("idcodes do not match\n");
        return 2;
    }

    /* save interrupts... */
    interrupts = *(volatile unsigned *) (REGISTERS + 0xc00);

    /* no interrupt sources during reload... */
    *(volatile unsigned *) (REGISTERS + 0xc04) = 0;


    /* set CO */
    *(volatile int *) ccntl = 2;

    /* write data... */
    for (i=(p[2]/4); i<(p[2]+p[3])/4; i++) {
        /* write next word */
        *(volatile int *) cdata = p[i];

        /* wait for busy bit... */
        while ( (*(volatile int *)ccntl)&4) {
            /* check for an error...
             */
            if ( (*(volatile int *) ccntl)&0x10) {
                printf("config programming error!\n");

                /* restore interrupts... */
                *(volatile unsigned *) (REGISTERS + 0xc04) = interrupts;
                return 10;
            }
        }
    }

    /* wait for CO bit to clear...
     */
    while ((*(volatile int *) ccntl)&0x2) {
        if ( (*(volatile int *) ccntl)&0x10) {
            /* restore interrupts... */
            *(volatile unsigned *) (REGISTERS + 0xc04) = interrupts;
            printf("config programming error (waiting for CO)!\n");
            return 10;
        }
    }

    /* restore interrupts... */
    *(volatile unsigned *) (REGISTERS + 0xc04) = interrupts;

#if 0
    /* load thresholds for comm -- these are defaults except sdelay
     * and rdelay...
     */
    if (hal_FPGA_query_type()==DOM_HAL_FPGA_TYPE_STF_COM) {
        *(unsigned *) 0x90081080 = (970<<16)|960; /* clev */
        *(unsigned *) 0x90081084 = (1<<24)|(1<<16)|(3<<8)|64;
    }
#endif

    /* load domid here so that "hardware" domid
     * works... sample domid: 710200073c7214d0
     *
     * FIXME: test fpga registers are used directly
     * here.  i'm not sure how else to do this, exporting
     * an fpga hal routine doesn't seem very nice, maybe
     * we should just export the fpga config routine from
     * the hal -- however, we don't want anyone else except
     * iceboot reloading the fpga probably...
     */
    /* HAHA!  I like that last comment! */
    {  
        unsigned long long domid = halGetBoardIDRaw();
        unsigned base = 
            (hal_FPGA_query_type()==DOM_HAL_FPGA_TYPE_DOMAPP) ?
            0x90000530 : 0x90081058;
     
        /* low 32 bits...
         */
        *(volatile unsigned *)(base) = domid&0xffffffff;

        /* high 16 bits + ready bit...
         */
        *(volatile unsigned *)(base+4) = (domid>>32) | 0x10000;
    }

    return 0;
}

/******************************************************************************/
/* 
 * gunzip code stolen from iceboot
 */
static int uncomp(Bytef *dest, uLongf *destLen, 
                  const Bytef *source, uLong sourceLen) {
    z_stream stream;
    int err;

    stream.next_in = (Bytef*)source;
    stream.avail_in = (uInt)sourceLen;

    /* Check for source > 64K on 16-bit machine: */
    if ((uLong)stream.avail_in != sourceLen) return Z_BUF_ERROR;

    stream.next_out = dest;
    stream.avail_out = (uInt)*destLen;
    if ((uLong)stream.avail_out != *destLen) return Z_BUF_ERROR;

    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;

    /* send -MAX_WBITS to indicate no zlib header...
     */
    err = inflateInit2(&stream, -MAX_WBITS);
    if (err != Z_OK) return err;
    
    err = inflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        inflateEnd(&stream);

        /* hmmm, sometimes we get an error, even though
         * the data are ok...
         *
         * FIXME: track this down...
         */
        if (stream.total_out==*destLen) return Z_OK;

        return err == Z_OK ? Z_BUF_ERROR : err;
    }
    *destLen = stream.total_out;

    err = inflateEnd(&stream);
    return err;
}

void gunzip(uLong srcLen, Bytef *src, uLong *zLen_p, Bytef **zDest_p) {

    /* uncompress a gzip file...
     */
    if (src[0]==0x1f && src[1]==0x8b && src[2]==8) {
        Bytef *dest;
        uLongf destLen;
        int idx = 10, dl;
        int ret;
        
        if (src[3]&0x04) {
            int xlen = src[10] + (((int)src[11])<<8);
            idx += xlen + 2;
        }
        
        if (src[3]&0x08) {
            while (src[idx]!=0) idx++;
            idx++;
        }
      
        if (src[3]&0x10) {
            while (src[idx]!=0) idx++;
            idx++;
        }

        if (src[3]&0x02) {
            idx+=2;
        }

        /* header is now stripped...
         */
        dl = (int) src[srcLen-4] +
            ((int) src[srcLen-3]<<8) + 
            ((int) src[srcLen-2]<<16) +
            ((int) src[srcLen-1]<<24);
        
        dest = (Bytef *) malloc(dl);
        destLen = dl;

        ret = uncomp(dest, &destLen, src + idx, srcLen - idx - 8);

        if (ret==Z_OK && destLen==dl) {
            *zLen_p = dl;
            *zDest_p = dest;
        }
        else {
#ifdef DEBUG
            printf("gunzip: error uncompressing: %d %lu %d\r\n", ret, destLen, dl);
#endif
            free(dest);
            *zLen_p = 0;
            *zDest_p = NULL;
        }
    }
    else {
#ifdef DEBUG
        printf("gunzip: invalid file header...\r\n");
#endif
        *zLen_p = 0;
        *zDest_p = NULL;
    }

}
