/**
 * @brief TODO
 * @author Abdulla Almosalami (@memphis242)
 * @date December 30, 2025
 */

/*** Header Includes ***/
#define __STDC_WANT_IEC_60559_EXT__ 1 // Some compilers will only expose DFP support like this
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

//#ifndef __STDC_IEC_60559_DFP__
//#  warning "__STDC_IEC_60559_DFP__ is not defined, so DFP support is limited."
//#endif

/*** External Objects ***/
extern const char * const WelcomeMsgs[];
extern const size_t WelcomeMsgsLen;

/*** Macro/constexpr Functions ***/
#define ARR_LEN(arr) ( sizeof(arr) / sizeof(arr[0]) )
#define TO_LOWER(arr)

constexpr char TESTFILE[] = "./test.tl"; // .tl stands for "transaction list"

/*** File-Scope Variables ***/
static volatile sig_atomic_t bUserEndedSession = false;

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

// Even though C23 supposedly comes with decimal floating-point support, the support
// through the available compilers (gcc, clang, etc.) is super lackluster. So,
// we'll just go the fixed-point route for now.
struct Amount
{
   int64_t integral;
   int8_t fractional; // Up to 2 fractional digits, [00, 99]
};

struct Transaction
{
   struct Amount amount;
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
   struct Amount curr_balance;
   struct TransactionList transactions;
};

/*** Forward Function Declarations ***/
static void handleSIGINT(int signum);

[[nodiscard]] static inline bool isNulTerminated(const char str[]);

static inline void toLowercase(char arr[], size_t len);

[[nodiscard]] static inline bool getUserInput(char * buf, size_t sz);
//
// As of the time of this writing (Jan 2, 2026), there still isn't much support
// for decimal FP I/O formatting... So, I need to implement my own, although...
// TODO: Look into using IBM's libdfp...
static inline bool strtoamount(
      struct Amount * result,
      const char * const restrict str );

static inline bool amounttostr(
      char * const str,
      size_t maxlen,
      struct Amount val );

/******************************************************************************/
int main(int argc, char * argv[])
{
   int mainrc = MAINRC_ALLGOOD;
   int rc; // for various system calls
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
      // Even if all the user wanted to do was update their transaction list,
      // we will rely on timestamping updates to the list for the sake of
      // integrity checking, so print an error and abort the program.
      (void)fprintf( stderr,
               "Error: Unable to time()\n"
               "time() returned %ld\n"
               "errno: %s (%d): %s\n",
               nowtime, strerrorname_np(errno), errno, strerror(errno) );

      return MAINRC_UNABLE_TO_TIME;
   }

   tzset();
   struct tm today;
   if ( localtime_r( &nowtime, &today) == nullptr )
   {
      (void)fprintf( stderr,
               "Error: Unable to localtime_r()\n"
               "localtime_r() returned nullptr\n"
               "errno: %s (%d): %s\n",
               strerrorname_np(errno), errno, strerror(errno) );

      return MAINRC_UNABLE_TO_TIME;
   }

#  ifndef NDEBUG
   (void)printf( "Today's Date: %02d-%02d-%04d\n",
                 today.tm_mon + 1,
                 today.tm_mday,
                 today.tm_year + 1900 );
#  endif

   // Welcome Message
   srand( (unsigned int)time(nullptr) );
   size_t welcome_msg_idx = (size_t)rand() % WelcomeMsgsLen;
   assert( welcome_msg_idx < WelcomeMsgsLen );
   (void)printf( "%s\n", WelcomeMsgs[welcome_msg_idx] );

   // The Main REPL Loop
   constexpr size_t WHILE_LOOP_CAP = 1'000'000;
   size_t nreps = 0;
   while ( !bUserEndedSession && nreps++ < WHILE_LOOP_CAP )
   {
      char userinput[200] = {0};

      // Technically, any printf-like call may report an error, but I'm not going
      // to check each and every call. Instead, I'll check these first couple of
      // calls, and assume the subsequent calls for the loop will be fine if
      // these are fine.
      // If an error does indeed occur, we break immediately and assume we can't
      // print to stdout anymore.
      rc = printf("\nbudget_oracle > ");
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
      if ( strcmp(userinput, "addt") == 0 )
      {
         char cmdargs[100];

         (void)printf("transaction type: ");
         (void)fflush(stdout);

         bool succeeded = getUserInput(cmdargs, sizeof cmdargs);
         if ( !succeeded )
            continue;

         if ( strcmp(cmdargs, "regular") == 0 )
         {
            (void)printf("daily, weekly, monthly, or yearly: ");
            (void)fflush(stdout);

            succeeded = getUserInput(cmdargs, sizeof cmdargs);
            if ( !succeeded )
               continue;

            if ( strcmp(cmdargs, "daily") == 0 )
            {
               (void)printf("amount: ");
               (void)fflush(stdout);

               succeeded = getUserInput(cmdargs, sizeof cmdargs);
               if ( !succeeded )
                  continue;

               // TODO: Account for different currencies

               // TODO: Convert decimal FP number string to number
               struct Amount num;

               //(void)printf("Number received after conversion: %H\n", num);

               char amount[16] = {0};
               // TODO: Get decimal floating-point input from user...

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

[[nodiscard]] static inline bool isNulTerminated(const char str[])
{
   assert(str != nullptr);

   constexpr size_t MAX_STR_LEN = 10'000;

   for ( size_t i = 0; i < MAX_STR_LEN; ++i )
      if ( str[i] == '\0' )
         return true;

   return false;
}

static inline void toLowercase(char arr[], size_t len)
{
   for ( char * ptr = arr; ptr != nullptr && ptr < (arr + len) && *ptr != '\0'; ++ptr )
      *ptr = (char)tolower(*ptr);
}

[[nodiscard]] static inline bool getUserInput(char * buf, size_t sz)
{
   if ( fgets(buf, (int)sz, stdin) == nullptr )
   {
      // Either EOF encountered /wo other characters preceding it or I/O
      // interruption occured. Either way, time to exit gracefully.
      clearerr(stdin);
      printf("\nExiting command...\n");
      return false;
   }

   // Replace newline /w null-termination
   char * newlineptr = memchr(buf, '\n', sz);
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

      return false;
   }
   assert( newlineptr < (buf + sz) );
   *newlineptr = '\0';

   // User input will be treated as case-insensitive
   toLowercase(buf, sz);

   return true;
}

static inline bool strtoamount(
      struct Amount * restrict result,
      const char * const restrict str )
{
   assert(result != nullptr);
   assert(str != nullptr);
   assert(isNulTerminated(str));

   constexpr size_t MAX_NUM_LEN = 15; // A man can dream... ☁ 
   constexpr int64_t MAX_NUM = 999'999'999'999'999; 
   constexpr size_t MAX_LEADING_WHITESPACE = 10;

   // Move past any leading whitespace
   bool space_bound = false;
   for ( size_t i = 0; i < MAX_LEADING_WHITESPACE; ++i )
   {
      if ( !isspace(str[i]) )
      {
         space_bound = true;
         break;
      }
   }

   if ( !space_bound )
      return false;

   // TODO: Investigate more efficient conversion algorithms...

   // Loop through characters until either a max limit is reached or non-digit
   // and parse out the digits
   int64_t integral_part = 0;
   int8_t fractional_part = 0; // I'm expecting that fractional digits [00, 99]
   uint8_t fractional_part_len = 0; // Shouldn't be > 2
   bool sign = false;
   bool valid_chars = true;
   bool found_decimal_point = false;
   bool rounding_performed = false;

   size_t i = 0;
   for ( ; i < MAX_NUM_LEN && str[i] != '\0'; ++i )
   {
      if ( str[i] == '-' && !sign )
      {
         sign = true;
         continue;
      }
      else
      {
         valid_chars = false;
         break;
      }

      if ( str[i] == '.' )
      {
         if ( found_decimal_point )
         {
            valid_chars = false;
            break;
         }

         found_decimal_point = true;
         continue;
      }

      if ( !isdigit(str[i]) )
      {
         valid_chars = false;
         break;
      }

      uint8_t digit = (uint8_t)(str[i] - '0');

      if ( found_decimal_point )
      {
         if ( !rounding_performed && fractional_part_len < 2 )
         {
            assert(fractional_part <= 9);
            fractional_part = (int8_t)((fractional_part * 10) + digit);
         }
         else if ( fractional_part_len >= 2 )
         {
            // Round, then we'll ignore subsequent digits
            if ( digit >= 5 )
               fractional_part++;

            assert(fractional_part <= 100);
            rounding_performed = true;
         }

         if ( fractional_part == 100 )
         {
            integral_part++;
            fractional_part = 0;
         }

         assert(fractional_part <= 99);
         assert(fractional_part_len < MAX_NUM_LEN);

         fractional_part_len++;
      }
      else
      {
         assert(integral_part < ((MAX_NUM - digit) / 10) );
         integral_part = (integral_part * 10) + digit;
      }
   }

   if ( sign )
   {
      integral_part *= -1;
      fractional_part *= -1;
   }

   assert(fractional_part_len < MAX_NUM_LEN);
   assert(abs(fractional_part) <= 99);
   assert(llabs(integral_part) <= MAX_NUM);

   if ( i >= MAX_NUM_LEN || !valid_chars )
      return false;

   if ( fractional_part_len > 2 )
   {
      (void)fprintf(stderr,
               "Warning: Rounding to two fractional digits! %hhu detected.\n",
               fractional_part_len );
   }

   // Perform conversion
   result->integral = integral_part;
   result->fractional = fractional_part;

   return true;
}

static inline bool amounttostr(
      char * const str,
      size_t maxlen,
      struct Amount val )
{
   constexpr size_t MAX_DIGITS = 16;

   assert(str != nullptr);
   assert(maxlen > 0);

   // This'll be a pain but within the constraints of my app environment, I'm not
   // using the float struct Amount range.
   int exp = 0;

   size_t i = 0;
   for ( ; i <= MAX_DIGITS && i < (maxlen-1); ++i )
   {
   }

   assert(i < MAX_DIGITS);
   assert(i < (maxlen-1));
   str[i] = '\0';

   return true;
}
