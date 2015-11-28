#include <stdlib.h>
#include <stdio.h>

#include "decoder.h"

#ifdef HAVE_LIBARIBB25

unsigned char *_data;

DECODER *
b25_startup(decoder_options *opt)
{
    DECODER *dec = (DECODER *)calloc(1, sizeof(DECODER));
    int code;
    const char *err = NULL;

    if(!dec) {
        err = "Memory allocation failed";
        goto error;
    }

    dec->b25 = create_arib_std_b25();
    if(!dec->b25) {
        err = "create_arib_std_b25 failed";
        goto error;
    }

    code = dec->b25->set_multi2_round(dec->b25, opt->round);
    if(code < 0) {
        err = "set_multi2_round failed";
        goto error;
    }

    code = dec->b25->set_strip(dec->b25, opt->strip);
    if(code < 0) {
        err = "set_strip failed";
        goto error;
    }

    code = dec->b25->set_emm_proc(dec->b25, opt->emm);
    if(code < 0) {
        err = "set_emm_proc failed";
        goto error;
    }

    dec->bcas = create_b_cas_card();
    if(!dec->bcas) {
        err = "create_b_cas_card failed";
        goto error;
    }
    code = dec->bcas->init(dec->bcas);
    if(code < 0) {
        err = "bcas->init failed";
        goto error;
    }

    code = dec->b25->set_b_cas_card(dec->b25, dec->bcas);
    if(code < 0) {
        err = "set_b_cas_card failed";
        goto error;
    }

    return dec;

error:
    fprintf(stderr, "%s\n", err);
    free(dec);
    return NULL;
}

int
b25_shutdown(DECODER *dec)
{
    if(_data)
        free(_data);

    dec->b25->release(dec->b25);
    dec->bcas->release(dec->bcas);
    free(dec);

    return 0;
}

int
b25_decode(DECODER *dec, ARIB_STD_B25_BUFFER *sbuf, ARIB_STD_B25_BUFFER *dbuf)
{
    int code;

    if(_data) {
        free(_data);
        _data = NULL;
    }

    code = dec->b25->put(dec->b25, sbuf);
    if(code < 0) {
        fprintf(stderr, "b25->put failed\n");
        if(code < ARIB_STD_B25_ERROR_NO_ECM_IN_HEAD_32M) {
            ARIB_STD_B25_BUFFER buf = *sbuf;
            unsigned char *p = NULL;

            dec->b25->withdraw(dec->b25, &buf);    // withdraw src buffer
            if(buf.size > 0) {
                fprintf(stderr, "b25->withdraw: size(%u)\n", buf.size);
                p = (unsigned char *)::malloc(buf.size + sbuf->size);
            }

            if(p) {
                memcpy(p, buf.data, buf.size);
                memcpy(p + buf.size, sbuf->data, sbuf->size);
                dbuf->data = p;
                dbuf->size = buf.size + sbuf->size;
                _data = p;
            }

            if(code != ARIB_STD_B25_ERROR_ECM_PROC_FAILURE)
                code = 0;
        }
        return code;
    }

    code = dec->b25->get(dec->b25, dbuf);
    if(code != 0) {
        fprintf(stderr, "b25->get failed\n");
        return code;
    }

    return 0;
}

int
b25_finish(DECODER *dec, ARIB_STD_B25_BUFFER *sbuf, ARIB_STD_B25_BUFFER *dbuf)
{
    int code;

    code = dec->b25->flush(dec->b25);
    if(code < 0) {
        fprintf(stderr, "b25->flush failed\n");
        return code;
    }

    code = dec->b25->get(dec->b25, dbuf);
    if(code < 0) {
        fprintf(stderr, "b25->get failed\n");
        return code;
    }

    return code;
}

#else

/* functions */
DECODER *
b25_startup(decoder_options *opt)
{
    return NULL;
}

int
b25_shutdown(DECODER *dec)
{
    return 0;
}

int
b25_decode(DECODER *dec, ARIB_STD_B25_BUFFER *sbuf, ARIB_STD_B25_BUFFER *dbuf)
{
    return 0;
}

int
b25_finish(DECODER *dec, ARIB_STD_B25_BUFFER *sbuf, ARIB_STD_B25_BUFFER *dbuf)
{
    return 0;
}

#endif
