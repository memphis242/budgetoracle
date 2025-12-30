/**
 * @brief TODO
 * @author Abdulla Almosalami (@memphis242)
 * @date December 30, 2025
 */

/*** Header Includes ***/
// Standard C Headers
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdatomic.h>
#include <errno.h>
// POSIX Headers
#include <signal.h>
#include <time.h>

#ifndef _GNU_SOURCE
#define strerrorname_np(str) "strerrorname_np() NOT IMPLEMENTED"
#endif

/*** Macro/constexpr Functions ***/
#define ARR_LEN(arr) ( sizeof(arr) / sizeof(arr[0]) )

/*** File-Scope Variables ***/
static volatile sig_atomic_t bUserEndedSession = false;

/*** Forward Function Declarations ***/
static void handleSIGNIT(int signum);

/*** Local Types ***/
enum MainRC
{
   MAINRC_ALLGOOD                 = 0x0000,
   MAINRC_SIGINT_REGISTRATION_ERR = 0x0001;
   MAINRC_INFINITE_LOOP_DETECTED  = 0x0002;
};

/******************************************************************************/
int main(int argc, char * argv[])
{
   int mainrc = MAINRC_ALLGOOD;
   int rc; // for various system calls

   // Register signal handler for SIGINT - one way for the user to close the app
   struct sigaction sa_cfg = {0}; // Includes setting SA_RESTART to 0 to prevent
                                  // restarting system calls after signal handler
   sigemptyset(&sa_cfg.sa_mask); // No need to mask any signals during handle
   sa_cfg.sa_handler = handleSIGINT;
   rc = sigaction( SIGINT, &sa_cfg, nullptr /* old sig action */ );
   if ( rc != 0 )
   {
      fprintf( stderr,
               "Warning: sigaction() failed to register interrupt signal handler.\n"
               "Returned: %d, errno: %s (%d): %s\n"
               "You won't be able to stop the program gracefully /w Ctrl+C, although \n"
               "Ctrl+C will still terminate the program.\n"
               rc, strerrorname_np(errno), errno, strerror(errno) );

      mainrc |= MAINRC_SIGINT_REGISTRATION_ERR;
   }

   constexpr size_t WHILE_LOOP_CAP = 1'000'000;
   size_t nreps = 0;
   while ( !bUserEndedSession && nreps++ < WHILE_LOOP_CAP )
   {
      char userinput[200] = {0};

      printf("> ");
      fflush(stdout);

      if ( fgets(userinput, sizeof userinput, stdin) == nullptr )
      {
         // Either EOF encountered /wo other characters preceding it or I/O
         // interruption occured. Either way, time to exit gracefully.
         break;
      }
   }

   assert(nreps < WHILE_LOOP_CAP);

   return mainrc;
}

/*** Local Function Implementations ***/

static void handleSIGNIT(int signum)
{
   (void)signum; // This signal handler is only for SIGINT, so signum isn't needed

   bUserEndedSession = true;
}
