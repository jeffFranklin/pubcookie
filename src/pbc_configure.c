
#ifdef HAVE_CONFIG_H
# include "config.h"
# include "pbc_path.h"
#endif

typedef void pool;

#ifdef HAVE_TIME_H
# include <time.h>
#endif /* HAVE_TIME_H */

#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif /* HAVE_STRINGS_H */

#ifdef HAVE_CTYPE_H
# include <ctype.h>
#endif

#ifdef HAVE_STDIO_H
# include <stdio.h>
#endif

#ifdef HAVE_STDARG_H
# include <stdarg.h>
#endif

#include "libpubcookie.h"
#include "pbc_configure.h"
#include "pbc_logging.h"
#include "snprintf.h"

#ifdef HAVE_DMALLOC_H
# if (!defined(APACHE) && !defined(APACHE1_3))
#  include <dmalloc.h>
# endif /* ! APACHE */
#endif /* HAVE_DMALLOC_H */


static config_getint *gint = NULL;
static config_getlist *glist = NULL;
static config_getstring *gstring = NULL;
static config_getswitch *gswitch = NULL;

/**
 *  For backward compatibilty
 */
void libpbc_config_init(pool *p, const char *alt_config, const char *ident)
{
    pbc_configure_init(p, ident, 
        (config_initialize *) &libpbc_myconfig_init,
        (void *)alt_config,
        &libpbc_myconfig_getint,
        &libpbc_myconfig_getlist,
        &libpbc_myconfig_getstring,
        &libpbc_myconfig_getswitch);
}

void pbc_configure_init(pool *p, const char *ident,
                        config_initialize *initialize,
                        void * initarg,
                        config_getint *i,
                        config_getlist *l,
                        config_getstring *s,
                        config_getswitch *w)
{
    /* sigh, prototypes not totally standardized so I need to cast */
    if (!i) i = (config_getint *) &libpbc_myconfig_getint;
    if (!l) l = (config_getlist *) &libpbc_myconfig_getlist;
    if (!s) s = (config_getstring *) &libpbc_myconfig_getstring;
    if (!w) w = (config_getswitch *) &libpbc_myconfig_getswitch;

    gint = i;
    glist = l;
    gstring = s;
    gswitch = w;

    if (!ident) {
        ident = "pubcookie";
    }

    if (initialize) {
        initialize(p, initarg, ident);
    }
}

int libpbc_config_getint(pool *p, const char *key, int def)
{
    return(gint(p, key, def));
}

int libpbc_config_getswitch(pool *p, const char *key, int def)
{
    return(gswitch(p, key, def));
}

const char *libpbc_config_getstring(pool *p, const char *key, const char *def)
{
    return(gstring(p, key, def));
}

char **libpbc_config_getlist(pool *p, const char *key)
{
    return(glist(p, key));
}
