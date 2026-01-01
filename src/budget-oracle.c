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
#include <ctype.h>
#include <stdatomic.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
// 3rd Party Library Headers
#include <glib.h>

#ifndef _GNU_SOURCE
#define strerrorname_np(str) "strerrorname_np() NOT IMPLEMENTED"
#endif

/*** Macro/constexpr Functions ***/
#define ARR_LEN(arr) ( sizeof(arr) / sizeof(arr[0]) )
#define TO_LOWER(arr)

constexpr char TESTFILE[] = "./test.tl"; // .tl stands for "transaction list"

/*** File-Scope Variables ***/
static volatile sig_atomic_t bUserEndedSession = false;

static const char * const WelcomeMsgs[] =
{
   "You are now speaking to the budget oracle 💸🔮... OoooOoOoooo",
   "So... You wish to peek into your fiscal future!",
   "Welcome, welcome, my child.",
   "To face life, we must at times be bold... In our hands, the future, we hold!",
   "This is the budget oracle speaking, please hold.",
   "You again?",
   "What's up doc?",
   "It's been so long, where have you been? Am I not, your next of kin!"
};

/*** Forward Function Declarations ***/
static void handleSIGINT(int signum);
static inline void toLowercase(char arr[], size_t len);

/*** Local Types ***/
// TODO: Put these RC's into an X-macro header so that you can also create an strerror_np() kind of lookup
enum MainRC
{
   MAINRC_ALLGOOD                 = 0x0000,
   MAINRC_SIGINT_REGISTRATION_ERR = 0x0001,
   MAINRC_INFINITE_LOOP_DETECTED  = 0x0002,
   MAINRC_UNABLE_TO_TIME          = 0x0004,
   MAINRC_UNABLE_TO_PRINT         = 0x0008,
};

// TODO: Might be best to put all the struct types into a separate hdr to shorten this main src file...
// TODO: Maybe abstract away the data structure implementation /w wrappers, so I can switch out the
//       underlying libraries easier...

struct Transaction
{
   _Decimal32 amount;
   const char * desc;
};

struct MonthlyTransactions
{
   GArray * days[31]; // vector of transactions
   // FIXME: Rule for transactions that are past 28 days to account for month size variation...
};
struct YearlyTransactions
{
   GArray * days[365 + 1]; // vector of transactions
   // FIXME: Figure out leap years...
   // A leap year is divisible by 4 AND (NOT divisible by 100 OR divisible by 400)
};
struct OneOff
{
   struct tm date;
   struct Transaction transaction;
};

struct TransactionList
{
   GArray * daily; // vector of transactions
   struct MonthlyTransactions monthly;
   struct YearlyTransactions yearly;
   GArray * oneoffs; // vector of struct OneOff
};

struct Budget
{
   uint8_t id;
   _Decimal32 curr_balance;
   struct TransactionList transactions;
};

/******************************************************************************/
int main(int argc, char * argv[])
{
   int mainrc = MAINRC_ALLGOOD;
   int rc; // for various system calls
   bool able_to_time = true;
   bool able_to_print = true;

   GArray * budgets = g_array_new( false, /* zero_terminated */
                                   true,  /* clear_ */
                                   sizeof(struct Budget) /* element_size */ );

   // Register signal handler for SIGINT - one way for the user to close the app
   struct sigaction sa_cfg = {0}; // Includes setting SA_RESTART to 0 to prevent
                                  // restarting system calls after signal handler
   sigemptyset(&sa_cfg.sa_mask); // No need to mask any signals during handle
   sa_cfg.sa_handler = handleSIGINT;
   rc = sigaction( SIGINT, &sa_cfg, nullptr /* old sig action */ );
   if ( rc != 0 )
   {
      (void)fprintf( stderr,
               "Warning: sigaction() failed to register interrupt signal handler.\n"
               "Returned: %d, errno: %s (%d): %s\n"
               "You won't be able to stop the program gracefully /w Ctrl+C, although \n"
               "Ctrl+C will still terminate the program.\n",
               rc, strerrorname_np(errno), errno, strerror(errno) );

      mainrc |= MAINRC_SIGINT_REGISTRATION_ERR;
   }

   // Get today's date before proceeding, as it will probably be relevant
   time_t nowtime = time(nullptr);
   if ( nowtime == (time_t)-1 )
   {
      // FIXME: If we can't time(), that may be a deal-breaker... End program?
      //        Even if all the user wanted to do was update their transaction
      //        list, we'll probably rely on timestamping to check integrity of
      //        the transaction list...
      (void)fprintf( stderr,
               "Warning: Unable to time()\n"
               "time() returned %ld\n"
               "errno: %s (%d): %s\n",
               nowtime, strerrorname_np(errno), errno, strerror(errno) );

      mainrc |= MAINRC_UNABLE_TO_TIME;
      able_to_time = false; // TODO: Make sure this is being used downstream!
   }

   tzset();
   struct tm today;
   if ( localtime_r( &nowtime, &today) == nullptr )
   {
      (void)fprintf( stderr,
               "Warning: Unable to localtime_r()\n"
               "localtime_r() returned nullptr\n"
               "errno: %s (%d): %s\n",
               strerrorname_np(errno), errno, strerror(errno) );

      mainrc |= MAINRC_UNABLE_TO_TIME;
      able_to_time = false;
   }

#  ifndef NDEBUG
   if ( able_to_time )
      (void)printf( "Today's Date: %02d-%02d-%04d\n",
                    today.tm_mon + 1,
                    today.tm_mday,
                    today.tm_year + 1900 );
#  endif

   // Welcome Message
   srand( (unsigned int)time(nullptr) );
   size_t welcome_msg_idx = (size_t)rand() % ARR_LEN(WelcomeMsgs);
   assert( welcome_msg_idx < ARR_LEN(WelcomeMsgs) );
   (void)printf( "%s\n", WelcomeMsgs[welcome_msg_idx] );

   // The Main REPL Loop
   constexpr size_t WHILE_LOOP_CAP = 1'000'000;
   size_t nreps = 0;
   while ( !bUserEndedSession && nreps++ < WHILE_LOOP_CAP )
   {
      char userinput[200] = {0};

      // Technically, any printf-like call may report an error, but I'm not going
      // to check each and every call. Instead, I'll check these first couple of
      // calls, and assume the subsequent calls for the loop will be fine.
      // If an error does indeed occur, we break immediately and assume we can't
      // print to stdout anymore.
      rc = printf("\nbudget-oracle > ");
      if ( rc < (int)((sizeof("> ") - 1)) )
      {
         mainrc |= MAINRC_UNABLE_TO_PRINT;
         // Since we can't print to stdout, we can't print the usual errno msg.
         // I still want some indication of the failure...
         if ( INT_WIDTH > 16 && errno < 0xFF )
            mainrc |= (errno << 8);
         else
            mainrc = errno; // Discard old mainrc and prioritize this errno

         able_to_print = false;
         break;
      }

      rc = fflush(stdout); // Force flush of "> " because we'll be waiting after
      if ( rc != 0 )
      {
         mainrc |= MAINRC_UNABLE_TO_PRINT;
         // Since we can't print to stdout, we can't print the usual errno msg.
         // I still want some indication of the failure...
         if ( INT_WIDTH > 16 && errno < 0xFF )
            mainrc |= (errno << 8);
         else
            mainrc = errno; // Discard old mainrc and prioritize this errno

         able_to_print = false;
         break;
      }

      if ( fgets(userinput, sizeof userinput, stdin) == nullptr )
      {
         // Either EOF encountered /wo other characters preceding it or I/O
         // interruption occured. Either way, time to exit gracefully.
         break;
      }

      // Replace newline /w null-termination
      char * newlineptr = memchr(userinput, '\n', sizeof userinput);
      if ( nullptr == newlineptr )
      {
         // There were more characters than the size of the input buffer to fgets()
         // Clear the remaining characters in stdin...
         int c;
         while ( (c = fgetc(stdin)) != '\n' && c != EOF );

         // Take this as an invalid input and request the user to try again.
         (void)fprintf( stderr,
                  "Error: Too many characters in user input encountered.\n"
                  "Please try again.\n" );
         continue;
      }
      assert( newlineptr < (userinput + sizeof(userinput)) );
      *newlineptr = '\0';

      // Commands will be treated as case-insensitive
      toLowercase(userinput, sizeof userinput);

      // Check for quit commands
      if ( strcmp(userinput, "exit") == 0 || strcmp(userinput, "quit") == 0 )
         break;

      // TODO: Authenticate user /w password

      // TODO: Execute commands
      if ( strcmp(userinput, "add") == 0 )
      {
         char cmdargs[100];

         (void)printf("\tTransaction Type: ");
         (void)fflush(stdout);

         if ( fgets(cmdargs, sizeof cmdargs, stdin) == nullptr )
         {
            clearerr(stdin); // Removes EOF from stream
            printf("\nExiting command...\n");
            continue;
         }
         toLowercase(cmdargs, sizeof cmdargs);

         if ( strcmp(cmdargs, "regular") == 0 )
         {
            (void)printf("\tdaily, weekly, monthly, or yearly: ");
            (void)fflush(stdout);

            if ( fgets(cmdargs, sizeof cmdargs, stdin ) == nullptr )
            {
               clearerr(stdin); // Removes EOF from stream
               printf("\nExiting command...\n");
               continue;
            }
            toLowercase(cmdargs, sizeof cmdargs);

            if ( strcmp(cmdargs, "daily") == 0 )
            {
               
            }
            else if ( strcmp(cmdargs, "monthly") == 0 )
            {

            }
            else if ( strcmp(cmdargs, "yearly") == 0 )
            {

            }
            else
            {
               (void)printf("Invalid argument: %s. Please try again.\n", cmdargs);
               continue;
            }
         }

         else if ( strcmp(cmdargs, "oneoff") == 0 )
         {
            
         }

         else
         {
            (void)printf("Invalid argument: %s. Please try again.\n", cmdargs);
            continue;
         }
      }

      else
      {
         (void)fprintf( stderr,
                        "Invalid command: %s. Please try again.\n",
                        userinput );
         continue;
      }
   }

   assert(nreps < WHILE_LOOP_CAP); // If assertion fails, we infinite looped somehow...

   if ( able_to_print )
      (void)printf("\nUser has ended session. Goodbye!\n\n");

   // Free all heap data
   // FIXME: Zero-out all members
   g_array_free(budgets, false);

   return mainrc;
}

/*** Local Function Implementations ***/

static void handleSIGINT(int signum)
{
   (void)signum; // This signal handler is only for SIGINT, so signum isn't needed

   bUserEndedSession = true;
}

static inline void toLowercase(char arr[], size_t len)
{
   for ( char * ptr = arr; ptr != nullptr && ptr < (arr + len) && *ptr != '\0'; ++ptr )
      *ptr = (char)tolower(*ptr);
}
