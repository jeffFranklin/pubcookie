/*
    $Id: libpubcookie.c,v 2.4 1999-06-25 22:15:00 willey Exp $
 */

#if defined (APACHE1_2) || defined (APACHE1_3)
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_main.h"
#include "http_protocol.h"
#include "util_script.h"
#endif

#if defined (WIN32)
#include <windows.h>
typedef  int pid_t;  /* win32 process ID */
#include <process.h>  /* getpid */
#else
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#endif

/* ssleay lib stuff */
#include <pem.h>
#include <des.h>
#include <rand.h>
#include <err.h>
/* pubcookie lib stuff */
#include "pubcookie.h"
#include "libpubcookie.h"
#include "pbc_config.h"
#include "pbc_version.h"
/*#include <envelope.h> */

/* get a nice pretty log time                                                 */
char *libpbc_time_string(time_t t)
{ 
    struct tm	*tm;
    static char	buf[PBC_1K];

    tm = localtime(&t);
    strftime(buf, sizeof(buf)-1, "%Y/%m/%d %H:%M:%S", tm);

    return buf;
}

#if defined (WIN32)
extern FILE *debugFile;  /* from PubcookieFilter */
#endif

/* when things fail too badly to go on ...                                    */
void *libpbc_abend(const char *format,...)
{
    time_t	now;
    va_list	args;
    char	format_w_time[PBC_1K];
#if defined (WIN32)
    char        buff[PBC_4K];
#endif
    
    va_start(args, format);
    now = time(NULL);
    sprintf(format_w_time, "%s: ABEND: %s", libpbc_time_string(now), format);
#if defined (WIN32)
    vsprintf(buff, format_w_time, args);
    OutputDebugString(buff);  /* win32 debugging */
    if ( debugFile )
        fprintf(debugFile,"%s",buff);
#else
    vfprintf(stderr,format_w_time, args);
#endif
    va_end(args);
#if defined (WIN32)
    return NULL;
#else
    exit (EXIT_FAILURE);
#endif
}

/*                                                                            */
/* put some debugging info to stdout                                          */
/*                                                                            */
/* perhaps if your server has some other logging method you might want to     */
/* use it here                                                                */
/*                                                                            */
int libpbc_debug(const char *format,...) 
{
    time_t      now;
    va_list     args;
    char        format_w_time[PBC_4K];
#if defined (WIN32)
    char        buff[PBC_4K];
#endif

    va_start(args, format);
    now = time(NULL);
    sprintf(format_w_time, "%s: PUBCOOKIE_DEBUG: %s", libpbc_time_string(now), format);
#if defined (WIN32)
    vsprintf(buff, format_w_time, args);
    OutputDebugString(buff);  /* win32 debugging */
    if ( debugFile )
        fprintf(debugFile,"%s",buff);
#else
    vfprintf(stderr, format_w_time, args);
#endif
    va_end(args);
    return 1;
}

void libpbc_void(void *thing) {
}

void *malloc_debug(size_t x) {
    void *p;
    p = malloc (x);
    libpbc_debug("  pbc_malloc(%d)= x%X\n",x,p);
    return p;
}

void free_debug(void *p) {
    libpbc_debug("  pbc_free= x%X\n",p);
    free(p);
}

/* keep pumping stuff into the random state                                   */
void libpbc_augment_rand_state(unsigned char *array, int len)
{

/*  Window only has milliseconds */
#if defined (WIN32)
    SYSTEMTIME   ts;
    unsigned char buf[sizeof(ts.wMilliseconds)];

    GetLocalTime(&ts);
    memcpy(buf, &ts.wMilliseconds, sizeof(ts.wMilliseconds));
    RAND_seed(buf, sizeof(ts.wMilliseconds));
#else
    struct timeval 	tv; 
    struct timezone 	tz;
    unsigned char	buf[sizeof(tv.tv_usec)];

    gettimeofday(&tv, &tz);
    memcpy(buf, &tv.tv_usec, sizeof(tv.tv_usec));
    RAND_seed(buf, sizeof(tv.tv_usec));
#endif

}

/* keep 'em guessing                                                          */
#ifdef APACHE
void libpbc_rand_malloc_p(pool *p)
#else
void libpbc_rand_malloc_np()
#endif
{

    int			num = 0, i;
    unsigned char	buf[PBC_RAND_MALLOC_BYTES];

    while ( num <= 0 ) {
        RAND_bytes(buf, PBC_RAND_MALLOC_BYTES);
        for( i=0; i<PBC_RAND_MALLOC_BYTES; i++)
            num += (int)buf[i];	
    }
    pbc_malloc(num);

}

/*                                                                            */
/* any general startup stuff goes here                                        */
/*                                                                            */
#ifdef APACHE
void libpbc_pubcookie_init_p(pool *p)
#else
void libpbc_pubcookie_init_np()
#endif
{
    unsigned char	buf[sizeof(pid_t)];
    pid_t		pid;

/*  libpbc_debug("libpbc_pubcookie_init\n");  */

    pid = getpid();
    memcpy(buf, &pid, sizeof(pid_t));
    libpbc_augment_rand_state(buf, sizeof(pid));

}

/*                                                                            */
/* any general shutdown stuff goes here                                       */
/*                                                                            */
/*   since i can't find a hook in apache for this there is nothing here, yet  */
/*                                                                            */
#ifdef APACHE
void libpbc_pubcookie_exit_p(pool *p)
#else
void libpbc_pubcookie_exit_np()
#endif
{

}

/* a local malloc and init                                                    */
#ifdef APACHE
unsigned char *libpbc_alloc_init_p(pool *p, int len)
#else
unsigned char *libpbc_alloc_init_np(int len)
#endif
{
    unsigned char	*pointer;

/* Skip the rand_malloc for Windows ISAPI filter, too much overhead */
#if !defined (WIN32)
    libpbc_rand_malloc();
#endif

    if( (pointer = (unsigned char *)pbc_malloc(len)) ) 
	memset(pointer, 0, len);
    else
        libpbc_abend("libpbc_alloc_init: Failed to malloc space\n");
    return pointer;
}

/* read and store a private key                                               */
/*    no return value b/c it's fail out or succeed onward                     */
#ifdef APACHE
void libpbc_get_private_key_p(pool *p, md_context_plus *ctx_plus, char *keyfile)
#else
void libpbc_get_private_key_np(md_context_plus *ctx_plus, char *keyfile)
#endif
{

    FILE	*key_fp;
    EVP_PKEY	*key;

/*  libpbc_debug("libpbc_get_private_key\n");  */

    if( ! keyfile )
        libpbc_abend("libpbc_get_private_key: No keyfile specified\n");

    if( ! (key_fp = pbc_fopen(keyfile, "r")) )
        libpbc_abend("libpbc_get_private_key: Could not open keyfile: %s\n", keyfile);

    if( ! (key = (EVP_PKEY *)PEM_ASN1_read((char *(*)())d2i_PrivateKey,
                        PEM_STRING_EVP_PKEY, key_fp, NULL, NULL)) )
        libpbc_abend("libpbc_get_private_key: Could not read keyfile: %s\n", keyfile);

    pbc_fclose(key_fp);
    memcpy(ctx_plus->private_key, key, sizeof(EVP_PKEY));

}

/* read, decode,  and store a public key                                      */
/*    no return value b/c it's fail out or succeed onward                     */
#ifdef APACHE
void libpbc_get_public_key_p(pool *p, md_context_plus *ctx_plus, char *certfile)
#else
void libpbc_get_public_key_np(md_context_plus *ctx_plus, char *certfile)
#endif
{
    FILE 	*fp;
    X509	*x509;
    EVP_PKEY	*key;

/*  libpbc_debug("libpbc_get_public_key\n"); */

    if( ! certfile )
        libpbc_abend("libpbc_get_public_key: No certfile specified\n");

    if( ! (fp = pbc_fopen(certfile, "r")) )
	libpbc_abend("libpbc_get_public_key: Could not open keyfile: %s\n", certfile);

    if( ! (x509 = (X509 *) PEM_ASN1_read((char *(*)())d2i_X509, 
	                PEM_STRING_X509, fp, NULL, NULL)) )
        libpbc_abend("libpbc_get_public_key: Could not read cert file: %s\n", certfile);

    if( ! (key = X509_extract_key(x509)) )
        libpbc_abend("libpbc_get_public_key: Could not convert cert to public key\n");

    pbc_fclose(fp);
    memcpy(ctx_plus->public_key, key, sizeof(EVP_PKEY));
}

/* mallocs a pbc_cookie_data struct                                           */
#ifdef APACHE
pbc_cookie_data *libpbc_init_cookie_data_p(pool *p)
#else
pbc_cookie_data *libpbc_init_cookie_data_np()
#endif
{
    pbc_cookie_data *cookie_data;

    cookie_data=(pbc_cookie_data *)libpbc_alloc_init(sizeof(pbc_cookie_data));
    return cookie_data;
}

/* init md_context_plus structure                                             */
#ifdef APACHE
md_context_plus *libpbc_init_md_context_plus_p(pool *p)
#else
md_context_plus *libpbc_init_md_context_plus_np()
#endif
{
    md_context_plus	*ctx_plus;
    unsigned char	lil_buf[1];

    ctx_plus=(md_context_plus *)libpbc_alloc_init(sizeof(md_context_plus));

    RAND_bytes(lil_buf, 1);
    switch ((int)lil_buf[0] % 3) {
    case 0:
        ctx_plus->ctx=(EVP_MD_CTX *)libpbc_alloc_init(sizeof(EVP_MD_CTX));
        RAND_bytes(lil_buf, 1);
        switch ((int)lil_buf[0] % 2) {
        case 0:
            ctx_plus->public_key=(EVP_PKEY *)libpbc_alloc_init(sizeof(EVP_PKEY));
            ctx_plus->private_key=(EVP_PKEY *)libpbc_alloc_init(sizeof(EVP_PKEY));
	    break;
        case 1:
            ctx_plus->private_key=(EVP_PKEY *)libpbc_alloc_init(sizeof(EVP_PKEY));
            ctx_plus->public_key=(EVP_PKEY *)libpbc_alloc_init(sizeof(EVP_PKEY));
	    break;
	}
	break;
    case 1:
        ctx_plus->public_key=(EVP_PKEY *)libpbc_alloc_init(sizeof(EVP_PKEY));
        RAND_bytes(lil_buf, 1);
        switch ((int)lil_buf[0] % 2) {
        case 0:
            ctx_plus->ctx=(EVP_MD_CTX *)libpbc_alloc_init(sizeof(EVP_MD_CTX));
            ctx_plus->private_key=(EVP_PKEY *)libpbc_alloc_init(sizeof(EVP_PKEY));
	    break;
        case 1:
            ctx_plus->private_key=(EVP_PKEY *)libpbc_alloc_init(sizeof(EVP_PKEY));
            ctx_plus->ctx=(EVP_MD_CTX *)libpbc_alloc_init(sizeof(EVP_MD_CTX));
	    break;
	}    
	break;
    case 2:
        ctx_plus->private_key=(EVP_PKEY *)libpbc_alloc_init(sizeof(EVP_PKEY));
        RAND_bytes(lil_buf, 1);
        switch ((int)lil_buf[0] % 2) {
        case 0:
            ctx_plus->ctx=(EVP_MD_CTX *)libpbc_alloc_init(sizeof(EVP_MD_CTX));
            ctx_plus->public_key=(EVP_PKEY *)libpbc_alloc_init(sizeof(EVP_PKEY));
	    break;
        case 1:
            ctx_plus->public_key=(EVP_PKEY *)libpbc_alloc_init(sizeof(EVP_PKEY));
            ctx_plus->ctx=(EVP_MD_CTX *)libpbc_alloc_init(sizeof(EVP_MD_CTX));
	    break;
	}
	break;
    }

    return ctx_plus;
}

/*                                                                            */
#ifdef APACHE
void libpbc_free_md_context_plus_p(pool *p, md_context_plus *ctx_plus)
#else
void libpbc_free_md_context_plus_np(md_context_plus *ctx_plus)
#endif
{
    pbc_free(ctx_plus->ctx);
    pbc_free(ctx_plus->public_key);
    pbc_free(ctx_plus->private_key);
    pbc_free(ctx_plus);
}

/*                                                                            */
#ifdef APACHE
unsigned char *libpbc_gethostip_p(pool *p)
#else
unsigned char *libpbc_gethostip_np()
#endif
{
    struct hostent      *h;
    unsigned char       *addr;

#if defined (WIN32)
    char                hostname[PBC_1K];
    int                 err;
    
    hostname[0] = '\0';
    err=gethostname(hostname, sizeof(hostname));
    if( (h = gethostbyname(hostname)) == NULL ) {
        libpbc_abend("gethostname error= %d, %s: host unknown.\n", err,hostname);
    }
#else
    struct utsname      myname;

    if ( uname(&myname) < 0 )
	libpbc_abend("problem doing uname lookup\n");

    if ( (h = gethostbyname(myname.nodename)) == NULL ) 
       	libpbc_abend("%s: host unknown.\n", myname.nodename);
#endif

    addr = libpbc_alloc_init(h->h_length);
    memcpy(addr, h->h_addr_list[0], h->h_length);
    
    return addr;
}

/* we only use the first four bytes of the ip (maybe someday they'll be       */
/* longer) but it doesn't matter here.                                        */
/*                                                                            */
char *libpbc_mod_crypt_key(char *in, unsigned char *addr_bytes)
{
    int			i;

    for( i=0; i<PBC_DES_KEY_BUF; ++i ) {
	in[i] ^= addr_bytes[i % 4];
    }
    
    return in;

}

/*                                                                            */
#ifdef APACHE
void libpbc_get_crypt_key_p(pool *p, crypt_stuff *c_stuff, char *keyfile)
#else
void libpbc_get_crypt_key_np(crypt_stuff *c_stuff, char *keyfile)
#endif
{
    FILE             *fp;
    char             *key_in;
    unsigned char    *addr;

/*  libpbc_debug("libpbc_get_crypt_key\n"); */

    key_in = (char *)libpbc_alloc_init(PBC_DES_KEY_BUF);

    if( ! (fp = pbc_fopen(keyfile, "rb")) )  /* win32 - must be binary */
        libpbc_abend("libpbc_crypt_key: Failed open: %s\n", keyfile);
    
    if( fread(key_in, sizeof(char), PBC_DES_KEY_BUF, fp) != PBC_DES_KEY_BUF)
        libpbc_abend("libpbc_crypt_key: Failed read: %s\n", keyfile);
    
    pbc_fclose(fp);

    addr = libpbc_gethostip();
    memcpy(c_stuff->key_a, libpbc_mod_crypt_key(key_in, addr), sizeof(c_stuff->key_a));
    pbc_free(key_in);
    pbc_free(addr);
}

/*                                                                            */
#ifdef APACHE
crypt_stuff *libpbc_init_crypt_p(pool *p, char *keyfile)
#else
crypt_stuff *libpbc_init_crypt_np(char *keyfile)
#endif
{
    crypt_stuff	*c_stuff;

/*  libpbc_debug("libpbc_init_crypt: keyfile= %s\n",keyfile); */

    c_stuff=(crypt_stuff *)libpbc_alloc_init(sizeof(crypt_stuff));

    libpbc_get_crypt_key(c_stuff, keyfile);

    return c_stuff;
}

/*                                                                            */
#ifdef APACHE
void libpbc_free_crypt_p(pool *p, crypt_stuff *c_stuff)
#else
void libpbc_free_crypt_np(crypt_stuff *c_stuff)
#endif
{
    pbc_free(c_stuff);  
}

/*                                                                            */
#ifdef APACHE
unsigned char *libpbc_sign_cookie_p(pool *p, unsigned char *cookie_string, md_context_plus *ctx_plus)
#else
unsigned char *libpbc_sign_cookie_np(unsigned char *cookie_string, md_context_plus *ctx_plus)
#endif
{
    unsigned char	*sig;
    unsigned int	sig_len = 0;

    sig = (unsigned char *)libpbc_alloc_init(PBC_SIG_LEN);

    EVP_SignInit(ctx_plus->ctx, EVP_md5());
    EVP_SignUpdate(ctx_plus->ctx, cookie_string, sizeof(pbc_cookie_data));
    if( EVP_SignFinal(ctx_plus->ctx, sig, &sig_len, ctx_plus->private_key) )
        return sig;
    else {
        pbc_free(sig);
        return (unsigned char *)NULL;
    }
}

/* check a signature after context is established                             */
int libpbc_verify_sig(unsigned char *sig, unsigned char *cookie_string, md_context_plus *ctx_plus)
{
    int	res = 0;

    EVP_VerifyInit(ctx_plus->ctx, EVP_md5());
    EVP_VerifyUpdate(ctx_plus->ctx, cookie_string, sizeof(pbc_cookie_data));
    res = EVP_VerifyFinal(ctx_plus->ctx, sig, PBC_SIG_LEN, ctx_plus->public_key);

    return res;

}

unsigned char *libpbc_stringify_seg(unsigned char *start, unsigned char *seg, unsigned len)
{
    int			seg_len;

    seg_len = ( len < strlen((const char *)seg) ) ? len : strlen((const char *)seg);
    memcpy(start, seg, seg_len);
    return start + len;
}

/*                                                                            */
pbc_cookie_data *libpbc_destringify_cookie_data(pbc_cookie_data *cookie_data) 
{

    (*cookie_data).broken.user[PBC_USER_LEN-1] = '\0';
    (*cookie_data).broken.version[PBC_VER_LEN-1] = '\0';
    (*cookie_data).broken.app_id[PBC_APP_ID_LEN-1] = '\0';
    (*cookie_data).broken.appsrv_id[PBC_APPSRV_ID_LEN-1] = '\0';
    return cookie_data;

}

/* package the cookie info for transit                                        */
/*   - make the cookie_data struct a string                                   */
/*   - do network byte order conversion                                       */
#ifdef APACHE
unsigned char *libpbc_stringify_cookie_data_p(pool *p, pbc_cookie_data *cookie_data) 
#else
unsigned char *libpbc_stringify_cookie_data_np(pbc_cookie_data *cookie_data) 
#endif
{
    unsigned char	*cookie_string;
    unsigned char	*ptr;
    int			temp;

    ptr = cookie_string = (unsigned char *)libpbc_alloc_init(sizeof(pbc_cookie_data));
    ptr = libpbc_stringify_seg(ptr, (*cookie_data).broken.user, PBC_USER_LEN);
    ptr = libpbc_stringify_seg(ptr, (*cookie_data).broken.version, PBC_VER_LEN);
    ptr = libpbc_stringify_seg(ptr, (*cookie_data).broken.appsrv_id, PBC_APPSRV_ID_LEN);
    ptr = libpbc_stringify_seg(ptr, (*cookie_data).broken.app_id, PBC_APP_ID_LEN);
    *ptr = (*cookie_data).broken.type;
    ptr++;

    *ptr = (*cookie_data).broken.creds;
    ptr++;

    temp = htonl((*cookie_data).broken.serial);
    memcpy(ptr, &temp, sizeof(int));
    ptr += sizeof(int);

    temp = htonl((*cookie_data).broken.create_ts);
    memcpy(ptr, &temp, sizeof(time_t));
    ptr += sizeof(time_t);

    temp = htonl((*cookie_data).broken.last_ts);
    memcpy(ptr, &temp, sizeof(time_t));
    ptr += sizeof(time_t);

    return cookie_string;

}

/* get some indices for choosing a key and modifying ivec                     */
int libpbc_get_crypt_index() 
{
    unsigned char	r_byte[1];
    int			index;

    r_byte[0] = '\0';
    while ( r_byte[0] == '\0' ) 
        RAND_bytes(r_byte, 1);
    index = (int)r_byte[0] - (int)r_byte[0]/PBC_DES_INDEX_FOLDER;
    return index;
}

/* encrypt a string                                                           */
int libpbc_encrypt_cookie(unsigned char *in, unsigned char *out, crypt_stuff *c_stuff, long len) 
{
    int				c = 0, i = 0;
    int				tries = 5;
    int				index1 = 0; 
    int				index2;
    des_cblock			key;
    des_cblock			ivec;
    static unsigned char	ivec_tmp[PBC_INIT_IVEC_LEN]=PBC_INIT_IVEC;
    des_key_schedule    	ks;

    index2=libpbc_get_crypt_index();
    memcpy(ivec, &(c_stuff->key_a[index2]), sizeof(ivec));
    for( c=0; c<sizeof(ivec); ++c ) {
	ivec[c] ^= ivec_tmp[i % sizeof(ivec_tmp)];
    }

    /* find a random index into the char key array and make a key shedule */

/*  libpbc_debug("libpbc_encrypt_cookie: before setting des_check_key= %d\n",des_check_key); */

    des_check_key = 1;
    memset(key, 0, sizeof(key));
#ifdef OPENSSL_0_9_2B
    while ( des_key_sched(key, ks) != 0 && --tries ) {
#else
    while ( des_key_sched(&key, ks) != 0 && --tries ) {
#endif
        index1=libpbc_get_crypt_index();
	memcpy(key, &(c_stuff->key_a[index1]), sizeof(key));
#ifdef OPENSSL_0_9_2B
        des_set_odd_parity(key);
#else
        des_set_odd_parity(&key);
#endif
    }
    if ( ! tries ) {
       libpbc_debug("libpbc_encrypt_cookie: Coudn't find a good key\n");
       return 0;
    }

#ifdef OPENSSL_0_9_2B
    des_cfb64_encrypt(in, out, len, ks, ivec, &i, DES_ENCRYPT);
#else
    des_cfb64_encrypt(in, out, len, ks, &ivec, &i, DES_ENCRYPT);
#endif
    libpbc_augment_rand_state(ivec, sizeof(ivec));

    /* stick the indices on the end of the train */
    out[len] = (unsigned char)index1;
    out[len+1] = (unsigned char)index2;
    return 1;

}

/* decrypt a string                                                         */
int libpbc_decrypt_cookie(unsigned char *in, unsigned char *out, crypt_stuff *c_stuff, long len) 
{
    int				c = 0, i = 0;
    int				index1, index2;
    des_cblock			key;
    des_cblock			ivec;
    static unsigned char	ivec_tmp[PBC_INIT_IVEC_LEN]=PBC_INIT_IVEC;
    des_key_schedule    	ks;

    /* grab those two extra btyes off the tail */
    index1 = (int)in[len];
    index2 = (int)in[len+1];

    memcpy(ivec, &(c_stuff->key_a[index2]), sizeof(ivec));
    for( c=0; c<sizeof(ivec); ++c ) {
	ivec[c] ^= ivec_tmp[i % sizeof(ivec_tmp)];
    }

    /* use the supplied index into the char key array and make a key shedule */
    memcpy(key, &(c_stuff->key_a[index1]), sizeof(key));
#ifdef OPENSSL_0_9_2B
    des_set_odd_parity(key);
    if ( des_key_sched(key, ks) ) {
#else
    des_set_odd_parity(&key);
    if ( des_key_sched(&key, ks) ) {
#endif
       libpbc_debug("libpbc_decrypt_cookie: Didn't derive a good key\n");
       return 0;
    }

#ifdef OPENSSL_0_9_2B
    des_cfb64_encrypt(in, out, len, ks, ivec, &i, DES_DECRYPT);
#else
    des_cfb64_encrypt(in, out, len, ks, &ivec, &i, DES_DECRYPT);
#endif

    return 1;

}

/* put stuff in the cookie structure                                          */
/*  note: we don't do network byte order conversion here,                     */
/*  instead we leave that for stringify                                       */
/*                                                                            */
void libpbc_populate_cookie_data(pbc_cookie_data *cookie_data,
	                  unsigned char *user, 
	                  unsigned char type, 
			  unsigned char creds,
			  int serial,
			  unsigned char *appsrv_id,
			  unsigned char *app_id) 
{

    strncpy((char *)(*cookie_data).broken.user, (const char *)user, PBC_USER_LEN-1);
    strncpy((char *)(*cookie_data).broken.version, PBC_VERSION, PBC_VER_LEN-1);
    (*cookie_data).broken.type = type;
    (*cookie_data).broken.creds = creds;
    (*cookie_data).broken.serial = serial;
    (*cookie_data).broken.create_ts = time(NULL);
    (*cookie_data).broken.last_ts = time(NULL);
    strncpy((char *)(*cookie_data).broken.appsrv_id, (const char *)appsrv_id, PBC_APPSRV_ID_LEN-1);
    strncpy((char *)(*cookie_data).broken.app_id, (const char *)app_id, PBC_APP_ID_LEN-1);

}

/* unfortuneately libpbc_sign_bundle_cookie and libpbc_unbundle are not       */
/* symetrical in the data they deal with.  the bundle takes the stringified   */
/* info and the unbundle returns a strunct.  maybe someday i'll clean that up */
/*                                                                            */
#ifdef APACHE
unsigned char *libpbc_sign_bundle_cookie_p(pool *p, 
	                  unsigned char *cookie_string,
			  md_context_plus *ctx_plus,
			  crypt_stuff *c_stuff) 
#else
unsigned char *libpbc_sign_bundle_cookie_np(unsigned char *cookie_string,
			  md_context_plus *ctx_plus,
			  crypt_stuff *c_stuff) 
#endif
{

    unsigned char		*cookie;
    unsigned char		*sig;
    unsigned char		buf[PBC_4K];
    unsigned char		buf2[PBC_4K];

    memset(buf, 0, sizeof(buf));
    memset(buf2, 0, sizeof(buf2));

    if ( ! (sig = libpbc_sign_cookie(cookie_string, ctx_plus)) ) {
        libpbc_debug("libpbc_sign_bundle_cookie: Cookie signing failed\n");
	return (unsigned char *)NULL;
    }

    memcpy(buf, sig, PBC_SIG_LEN);
    pbc_free(sig);
    memcpy(buf+PBC_SIG_LEN, cookie_string, sizeof(pbc_cookie_data));

    /* two bytes get added on in libpbc_encrypt_cookie */
    if ( ! libpbc_encrypt_cookie(buf, buf2, c_stuff, sizeof(pbc_cookie_data)+PBC_SIG_LEN) )
	return 0;

    cookie = (unsigned char *)libpbc_alloc_init(PBC_4K);
    base64_encode(buf2, cookie, PBC_SIG_LEN + sizeof(pbc_cookie_data) + 2);

    return cookie;
}

/*                                                                            */
/* get public key and initialize verify context                               */
/*                                                                            */
#ifdef APACHE
md_context_plus *libpbc_verify_init_p(pool *p, char *certfile) 
#else
md_context_plus *libpbc_verify_init_np(char *certfile) 
#endif
{
    md_context_plus *ctx_plus;

/*  libpbc_debug("libpbc_verify_init: certfile= %s\n",certfile); */

    ctx_plus = libpbc_init_md_context_plus();
    libpbc_get_public_key(ctx_plus, certfile);

    return ctx_plus;
}

/*                                                                            */
/* get private key and initialize context                                     */
/*                                                                            */
#ifdef APACHE
md_context_plus *libpbc_sign_init_p(pool *p, char *keyfile) 
#else
md_context_plus *libpbc_sign_init_np(char *keyfile) 
#endif
{
    md_context_plus *ctx_plus;

/*  libpbc_debug("libpbc_sign_init: keyfile= %s\n",keyfile); */

    ctx_plus = libpbc_init_md_context_plus();
    libpbc_get_private_key(ctx_plus, keyfile);
    return ctx_plus;
}

/*                                                                            */
/* builds, signs and returns cookie                                           */
/*                                                                            */
#ifdef APACHE
unsigned char *libpbc_get_cookie_p(pool *p, unsigned char *user, 
	                  unsigned char type, 
			  unsigned char creds,
			  int serial,
			  unsigned char *appsrv_id,
			  unsigned char *app_id,
			  md_context_plus *ctx_plus,
			  crypt_stuff *c_stuff) 
#else
unsigned char *libpbc_get_cookie_np(unsigned char *user, 
	                  unsigned char type, 
			  unsigned char creds,
			  int serial,
			  unsigned char *appsrv_id,
			  unsigned char *app_id,
			  md_context_plus *ctx_plus,
			  crypt_stuff *c_stuff) 
#endif
{

    pbc_cookie_data 		*cookie_data;
    unsigned char			*cookie_string;
    unsigned char			*cookie;

/*  libpbc_debug("libpbc_get_cookie\n"); */

    libpbc_augment_rand_state(user, PBC_USER_LEN);

    cookie_data = libpbc_init_cookie_data();
    libpbc_populate_cookie_data(cookie_data, user, type, creds, serial, appsrv_id, app_id);
    cookie_string = libpbc_stringify_cookie_data(cookie_data);
    pbc_free(cookie_data);
    cookie = libpbc_sign_bundle_cookie(cookie_string, ctx_plus, c_stuff);
    pbc_free(cookie_string);

    return cookie;
}

/*                                                                            */
/*  deal with unbundling a cookie                                             */
/*                                                                            */
#ifdef APACHE
pbc_cookie_data *libpbc_unbundle_cookie_p(pool *p, char *in, md_context_plus *ctx_plus, crypt_stuff *c_stuff) 
#else
pbc_cookie_data *libpbc_unbundle_cookie_np(char *in, md_context_plus *ctx_plus, crypt_stuff *c_stuff) 
#endif
{
    pbc_cookie_data	*cookie_data;
    unsigned char	sig[PBC_SIG_LEN];
    unsigned char	buf[PBC_4K];
    unsigned char	buf2[PBC_4K];

/*  libpbc_debug("libpbc_unbundle_cookie\n"); */

    memset(buf, 0, sizeof(buf));
    memset(buf2, 0, sizeof(buf2));

    if ( strlen(in) < PBC_SIG_LEN || strlen(in) > PBC_4K ) {
	libpbc_debug("libpbc_unbundle_cookie: malformed cookie %s\n", in);
	return 0;
    }

    if( ! base64_decode((unsigned char *)in, buf) ) {
        libpbc_debug("libpbc_unbundle_cookie: Could not decode cookie.\n");
	return 0;
    }

    if ( ! libpbc_decrypt_cookie(buf, buf2, c_stuff, sizeof(pbc_cookie_data)+PBC_SIG_LEN) )
	return 0;

    /* break cookie in two */
    memcpy(sig, buf2, PBC_SIG_LEN);
    cookie_data = libpbc_init_cookie_data();
    memcpy((*cookie_data).string, buf2+PBC_SIG_LEN, sizeof(pbc_cookie_data));

    if( (libpbc_verify_sig(sig, (*cookie_data).string, ctx_plus)) ) {
        cookie_data = libpbc_destringify_cookie_data(cookie_data);

	(*cookie_data).broken.last_ts = ntohl((*cookie_data).broken.last_ts);
	(*cookie_data).broken.create_ts = ntohl((*cookie_data).broken.create_ts);
	(*cookie_data).broken.serial = ntohl((*cookie_data).broken.serial);

        return cookie_data;
    }
    else {
	libpbc_debug("libpbc_unbundle_cookie: sig verify failed\n");
        return NULL;
    }
}
    
/*                                                                            */
/*  update last_ts in cookie                                                  */
/*                                                                            */
/* takes a cookie_data structure, updates the time, signs and packages up     */
/* the cookie to be sent back into the world                                  */
/*                                                                            */
#ifdef APACHE
unsigned char *libpbc_update_lastts_p(pool *p, pbc_cookie_data *cookie_data, md_context_plus *ctx_plus, crypt_stuff *c_stuff)
#else
unsigned char *libpbc_update_lastts_np(pbc_cookie_data *cookie_data, md_context_plus *ctx_plus, crypt_stuff *c_stuff)
#endif
{
    unsigned char	*cookie_string;
    unsigned char	*cookie;

    (*cookie_data).broken.last_ts = time(NULL);
    cookie_string = libpbc_stringify_cookie_data(cookie_data);
    cookie = libpbc_sign_bundle_cookie(cookie_string, ctx_plus, c_stuff);

    return cookie;

}

