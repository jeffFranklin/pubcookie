#include <time.h>
#include <syslog.h>
#include "libpubcookie.h"
#include "pbc_myconfig.h"
#include "pbc_logging.h"

void pbc_log_init()
{

/* open syslog - we are appending the PID to the log, and prepending 
    the string "pubcookie login server" to make it easily greppable 
*/
    openlog("pubcookie login server", LOG_PID, LOG_LOCAL2);
}


void pbc_log_activity(int logging_level, const char *message,...)
{
  va_list   args;
  char      new_message[PBC_4K];
  char      log[PBC_4K];

  if (logging_level <= (libpbc_config_getint("logging_level", logging_level)))    {
      va_start(args, message);
#if 0
      /* should I move the timestamp here? */
      snprintf(new_message, sizeof(new_message)-1, "%s: %s",
	       SYSERR_LOGINSRV_MESSAGE, message);
      vsnprintf(log, sizeof(log)-1, new_message, args);
#else
      vsnprintf(log, sizeof(log)-1, message, args);
#endif
      /* write to syslog, making it a "private auth"  message.
     This seemed to be the most reasonable option...I originally had
     it a configurable option, but decided against it. */

      if (logging_level != PBC_LOG_ERROR)
	syslog(LOG_MAKEPRI(LOG_AUTHPRIV, LOG_INFO), message);
      else
	syslog(LOG_MAKEPRI(LOG_AUTHPRIV, LOG_ERR), message);

      va_end(args);
    }
}

void pbc_log_close()
{
  closelog();
}

#if 0
char* pbc_create_log_message(char *info, char* user, char* app_id)
{
  return sprintf(%s: user ip: %s \t app id: %s \n %s, 
libpbc_time_string(time(NULL)),user,app_id, info);
  
}
#endif
