/*

    Copyright 1999-2002, University of Washington.  All rights reserved.

     ____        _                     _    _
    |  _ \ _   _| |__   ___ ___   ___ | | _(_) ___
    | |_) | | | | '_ \ / __/ _ \ / _ \| |/ / |/ _ \
    |  __/| |_| | |_) | (_| (_) | (_) |   <| |  __/
    |_|    \__,_|_.__/ \___\___/ \___/|_|\_\_|\___|


    All comments and suggestions to pubcookie@cac.washington.edu
    More info: http://www.washington.edu/computing/pubcookie/
    Written by the Pubcookie Team

 */

/* check_crypted_blob.c  */

#if !defined(WIN32)
#include <netdb.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pem.h>
#if defined (WIN32) 
#include <winsock2.h>   // jimb - WSASTARTUP for gethostname
#include <getopt.h>     // jimb - getopt from pdtools
extern char * optarg;
#define bzero(s,n)	memset((s),0,(n))  // jimb - win32
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include "pubcookie.h"
#include "libpubcookie.h"
#include "pbc_config.h"
#include "pbc_version.h"

#if defined (WIN32)
extern int Debug_Trace = 0;
extern FILE *debugFile = NULL;
#endif

void usage(const char *progname) {
    printf("%s -c crypted_file [-k c_key_file] [-h]\n\n", progname);
    printf("\t crypted_file:\tcrypted stuff to be decrypted.\n");
    printf("\t c_key_file:\tdefault is %s\n\n", PBC_CRYPT_KEYFILE);
    exit (1);
}

int main(int argc, char **argv) {
    crypt_stuff		*c1_stuff;
    unsigned char	in[PBC_1K];
    unsigned char	intermediate[PBC_1K];
    unsigned char	out[PBC_1K];
    FILE		*cfp;
    int 		c, barfarg = 0;
    char		*key_file = NULL;
    char		*crypted_file = NULL;
#if defined (WIN32)
    char		SystemRoot[256];

    Debug_Trace = 1;
    debugFile = stdout;
#endif
	
    printf("check_crypted_blob\n\n");

    bzero(in, 1024);
    bzero(out, 1024);
    bzero(intermediate, 1024);
    strcpy(in, "Maybe this plaintext is another world's ciphertext.");

    optarg = NULL;
    while (!barfarg && ((c = getopt(argc, argv, "hc:k:")) != -1)) {
	switch (c) {
	case 'h' :
	    usage(argv[0]);
	    break;
	case 'c' :
            if( crypted_file != NULL ) {
	        usage(argv[0]);
		break;
	    }
	    crypted_file = strdup(optarg);
	    break;
	case 'k' :
	    key_file = strdup(optarg);
	    break;
	default :
            if( crypted_file != NULL ) {
	        usage(argv[0]);
		break;
	    }
	    crypted_file = strdup(optarg);
	    break;
	}
    }

#if defined(WIN32)                                           
    {   
	WSADATA wsaData;

	if( WSAStartup((WORD)0x0101, &wsaData ) ) 
	{  
	    printf( "Unable to initialize WINSOCK: %d", WSAGetLastError() );
	    return -1;
	}
    }   
#endif

    if ( key_file )
        c1_stuff = libpbc_init_crypt(key_file);
    else {
	key_file = malloc(256);
#if defined(WIN32)  
	GetEnvironmentVariable ("windir",SystemRoot,256);
        sprintf(key_file,"%s%s", SystemRoot,PBC_CRYPT_KEYFILE);
#else
        sprintf(key_file,"%s",PBC_CRYPT_KEYFILE);
#endif
	printf("Using c_key file: %s\n\n",key_file);
        c1_stuff = libpbc_init_crypt(key_file);
    }

    if ( c1_stuff == NULL ) {
	printf("unable to initialize encryption context\n");
        usage(argv[0]);
    }

    if ( crypted_file != NULL ) {
        if( ! (cfp = pbc_fopen(crypted_file, "r")) ) {
            libpbc_abend("\n*** Cannot open the crypted file %s\n", crypted_file);
            exit(1);
	}
        fread(intermediate, sizeof(char), PBC_1K, cfp);
    } else {
	printf("Must specify a file with ciphertext\n\n");
	usage(argv[0]);
    }

    if ( ! libpbc_decrypt_cookie(intermediate, out, c1_stuff, strlen(in)) ) {
	printf("\n*** Libpbc_decrypt_cookie failed\n");
        exit(1);
    }

    printf("\nencrypted message is: %s\n", out);

    if( memcmp(in,out,sizeof(in)) != 0 )
	printf("\n*** cfb64 encrypt/decrypt error ***!\n");
    else
	printf("\nYeah!  It worked\n\n");

#if defined(WIN32)  
    WSACleanup();
#endif

    exit(0);

}
