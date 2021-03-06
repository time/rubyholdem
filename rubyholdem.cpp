/*
  Copyright 2009 Time

  This file is part of RubyHoldem.

  RubyHoldem is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  RubyHoldem is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with RubyHoldem.  If not, see <http://www.gnu.org/licenses/>.
*/

//#define DEBUG_THREADS
#define _CRT_SECURE_NO_DEPRECATE

#ifdef __WINE__
#undef _WIN32
#include <ruby.h>
#define _WIN32
#else
#include <ruby.h>
#endif

#include <stdarg.h>
#include <windef.h>
#include <winbase.h>

#ifndef RSTRING_PTR
# define RSTRING_PTR(s) (RSTRING(s)->ptr)
# define RSTRING_LEN(s) (RSTRING(s)->len)
#endif

#define RH_NO_EVENT 0
#define RH_PROCESS_MESSAGE 1
#define RH_EXIT 2

void rh_log(const char *format, ...)
{
#ifdef __WINE__
	va_list args;
	va_start(args, format);
	vprintf(format, args);
#else
	int size = 0;
	char text[100];
	memset(text, 0, sizeof(text));
	va_list args;
	va_start(args, format);
#if 1
	FILE *f = fopen("rubyholdem.log", "at");
	vfprintf(f, format, args);
	fclose(f);
#else
	size = vsnprintf(text, sizeof(text) - 1, format, args);
	MessageBox(NULL, text, "RubyHoldem message", 0);
#endif

#endif // __WINE__
}

struct holdem_player {
  char            m_name[16]          ;       //player name if known
  double          m_balance           ;       //player balance 
  double          m_currentbet        ;       //player current bet
  unsigned char   m_cards[2]          ;       //player cards
  
  unsigned char   m_name_known    : 1 ;       //0=no 1=yes
  unsigned char   m_balance_known : 1 ;       //0=no 1=yes
  unsigned char   m_fillerbits    : 6 ;       //filler bits
  unsigned char   m_fillerbyte        ;       //filler bytes
};

struct holdem_state {
  char            m_title[64]         ;       //table title
  double          m_pot[10]           ;       //total in each pot
  
  unsigned char   m_cards[5]          ;       //common cards
  
  unsigned char   m_is_playing    : 1 ;       //0=sitting-out, 1=sitting-in
  unsigned char   m_is_posting    : 1 ;       //0=autopost-off, 1=autopost-on
  unsigned char   m_fillerbits    : 6 ;       //filler bits
  
  unsigned char   m_fillerbyte        ;       //filler byte
  unsigned char   m_dealer_chair      ;       //0-9
  
  holdem_player   m_player[10]        ;       //player records
};


typedef VALUE (*rh_function)(ANYARGS);

typedef double (*pfgws_t)(int chair, const char* name, bool& error);
pfgws_t get_openholdem_symbol;
HANDLE rh_event_job;
HANDLE rh_event_done;
HANDLE rh_event_symbol_request;
HANDLE rh_event_symbol_request_done;
HANDLE rh_thread;
int rh_event_type = RH_NO_EVENT;

const char *rh_message;
const void *rh_param;
double rh_result;

int rh_symbol_chair;
const char *rh_symbol_name;
bool rh_symbol_error;
double rh_symbol_result;

extern "C" double __declspec(dllexport) process_message( const char* message, const void* param );
extern "C" BOOL APIENTRY DllMain(HANDLE module, DWORD reason, LPVOID reserved);

VALUE rh_module;

VALUE rh_safe_call_failed(VALUE arg, VALUE exception) {
  char *description = (char*)arg;
  char *message = RSTRING_PTR(rb_funcall(exception, rb_intern("message"), 0));
  
  VALUE klass = rb_funcall(exception, rb_intern("class"), 0);
  char *klass_name = RSTRING_PTR(rb_funcall(klass, rb_intern("name"), 0));
  VALUE backtrace = rb_funcall(exception, rb_intern("backtrace"), 0);
  char *backtrace_lines = RSTRING_PTR(rb_funcall(backtrace, rb_intern("join"), 1, rb_str_new2("\n")));
  
  
  rh_log(
	  "Unhandled Ruby exception occured in %s\n"
	  "%s (%s)\n"
	  "%s\n",
	  description,
	  message, klass_name,
	  backtrace_lines
	  );
  return rb_float_new(0.0);
}

VALUE rh_safe_call(const char *description, rh_function process_function, VALUE arg)
{
  return rb_rescue2(
    process_function, arg,
    (rh_function)rh_safe_call_failed, (VALUE)description,
    rb_eException, (VALUE)0
  );
}

VALUE rh_process_query(VALUE arg)
{
  VALUE result;
  result = rb_funcall(rh_module, rb_intern("process_query"), 1, arg);
  return rb_funcall(result, rb_intern("to_f"), 0);
}

VALUE rh_process_state(VALUE arg)
{
  VALUE result;
  result = rb_funcall(rh_module, rb_intern("process_state"), 1, arg);
  return rb_funcall(result, rb_intern("to_f"), 0);
}

VALUE rh_process_event(VALUE arg)
{
  VALUE result;
  result = rb_funcall(rh_module, rb_intern("process_event"), 1, arg);
  return rb_funcall(result, rb_intern("to_f"), 0);
}

void rh_emit_symbol_request()
{
  DWORD dwWaitResult;
  
  if (!SetEvent(rh_event_symbol_request))
  {
    rh_log("SetEvent failed (%d)\n", GetLastError());
    return;
  }
  dwWaitResult = WaitForSingleObject(rh_event_symbol_request_done, INFINITE);
  switch (dwWaitResult) 
  {
    // Event object was signaled
    case WAIT_OBJECT_0: 
      break;
      
    // An error occurred
    default:
      rh_log("RubyHoldem thread: Wait error (%d)\n", GetLastError());
  }
}

VALUE rh_get_symbol(VALUE self, VALUE chair, VALUE name)
{
  bool error;
  double result;
  char *name_ptr = RSTRING_PTR(name);
  
#ifdef DEBUG_THREADS
  rh_log("Thread %d getting symbol %s\n", GetCurrentThreadId(), name_ptr);
#endif
  if (!get_openholdem_symbol)
    rb_raise(rb_eRuntimeError, "Unable to get symbol: pfgws not received");
  
  rh_symbol_chair = NUM2INT(chair);
  rh_symbol_name = name_ptr;
  
  rh_emit_symbol_request();
  
  result = rh_symbol_result;
  error = rh_symbol_error;
  
  if (error)
    rb_raise(rb_eRuntimeError, "OpenHoldem returned an error while getting symbol \"%s\"", name_ptr);
#ifdef DEBUG_THREADS
  rh_log("Thread %d finished getting symbol %s\n", GetCurrentThreadId(), name_ptr);
#endif
  return rb_float_new(result);
}

void process_symbol_request()
{
  rh_symbol_result = get_openholdem_symbol(rh_symbol_chair, rh_symbol_name, rh_symbol_error);
  if (!SetEvent(rh_event_symbol_request_done))
  {
    rh_log("SetEvent failed (%d)\n", GetLastError());
    return;
  }
}


VALUE rh_init_module()
{
  get_openholdem_symbol = NULL;
  rh_module = rb_define_module("OpenHoldem");
  rb_define_module_function(rh_module, "get_symbol", (rh_function)rh_get_symbol, 2);
  return rb_eval_string("load File.join(Dir.pwd, 'rubyholdem.rb')");
}

void rh_job_done()
{
#ifdef DEBUG_THREADS
  rh_log("Thread %d sets job to done\n", GetCurrentThreadId());
#endif
  rh_event_type = RH_NO_EVENT;
  if (!SetEvent(rh_event_done))
  {
    rh_log("SetEvent failed (%d)\n", GetLastError());
    return;
  }
}

void rh_start_job(int type)
{
  DWORD dwWaitResult;
  HANDLE events[2];
  int job_done = 0;
  
  if (rh_event_type != RH_NO_EVENT)
  {
	  rh_log("Starting a job while another one is running!\n");
  }

  rh_event_type = type;

  if (!SetEvent(rh_event_job))
  {
    rh_log("SetEvent failed (%d)\n", GetLastError());
    return;
  }
  
#ifdef DEBUG_THREADS
  rh_log("Thread %d waiting for job done\n", GetCurrentThreadId());
#endif
  events[0] = rh_event_done;
  events[1] = rh_event_symbol_request;
  while (!job_done)
  {
    dwWaitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);
    switch (dwWaitResult) 
    {
      // rh_event_done was signaled
      case WAIT_OBJECT_0:
#ifdef DEBUG_THREADS
        rh_log("Thread %d: job done\n", GetCurrentThreadId());
#endif
        
        if (rh_event_type != RH_NO_EVENT)
        {
          rh_log("Job done event was signaled but job doesn't look done!\n");
        }
        
        //ResetEvent(rh_event_done);
        job_done = 1;
        break;
        
        // rh_event_symbol_request was signaled
      case WAIT_OBJECT_0 + 1:
#ifdef DEBUG_THREADS
        rh_log("Thread %d: symbol request\n", GetCurrentThreadId());
#endif
        process_symbol_request();
        break;
        
      // An error occurred
      default:
        rh_log("Error waiting for job done: Wait error (%d)\n", GetLastError());
        return;
    }
  }
}

double process_message( const char* message, const void* param )
{
  if (message == NULL || param == NULL)
    return 0;
  
#ifdef DEBUG_THREADS
  rh_log("process_message in thread %d\n", GetCurrentThreadId());
#endif

  rh_message = message;
  rh_param = param;
  rh_start_job(RH_PROCESS_MESSAGE);
  return rh_result;
}

void rh_process_message()
{
  const char *message = rh_message;
  const void *param = rh_param;
  VALUE result = Qnil;
  
#ifdef DEBUG_THREADS
  rh_log("rh_process_message in thread %d (message: %s)\n", GetCurrentThreadId(), message);
#endif

  if (strcmp(message, "query") == 0)
    result = rh_safe_call("rh_process_query", (rh_function)rh_process_query, rb_str_new2((char*)param));
  else if (strcmp(message, "state") == 0)
    result = rh_safe_call("rh_process_state", (rh_function)rh_process_state, rb_str_new((char*)param, sizeof(struct holdem_state)));
  else if (strcmp(message, "event") == 0)
    result = rh_safe_call("rh_process_event", (rh_function)rh_process_event, rb_str_new2((char*)param));
  else if (strcmp(message,"pfgws") == 0) {
    rh_log("pfgws received\n");
    get_openholdem_symbol = (pfgws_t)param;
    rh_result = 0.0;
  }
  if (TYPE(result) == T_FLOAT)
    rh_result = NUM2DBL(result);
  else
    rh_result = 0.0;
}

DWORD WINAPI rh_thread_proc(LPVOID lpParam)
{
  DWORD dwWaitResult;
  int must_exit = 0;

#ifdef DEBUG_THREADS
  rh_log("RubyHoldem thread %d starting\n", GetCurrentThreadId());
#endif

  ruby_init();
  rh_safe_call("rh_load_init_script", (rh_function)rh_init_module, Qnil);
  
  while (!must_exit)
  {
#ifdef DEBUG_THREADS
    rh_log("RubyHoldem thread %d waiting for event...\n", GetCurrentThreadId());
#endif

    dwWaitResult = WaitForSingleObject(rh_event_job, INFINITE);
  
    switch (dwWaitResult) 
    {
      // Event object was signaled
      case WAIT_OBJECT_0: 
#ifdef DEBUG_THREADS
        rh_log("Event occured in RubyHoldem thread %d\n", GetCurrentThreadId());
#endif
        switch(rh_event_type)
        {
          case RH_EXIT:
#ifdef DEBUG_THREADS
            rh_log("Event is RH_EXIT\n");
#endif
            must_exit = 1;
            break;
          case RH_PROCESS_MESSAGE:
#ifdef DEBUG_THREADS
            rh_log("Event is RH_PROCESS_MESSAGE\n");
#endif
            rh_process_message();
            break;
        }
        break;

      // An error occurred
      default:
        rh_log("RubyHoldem thread: Wait error (%d)\n", GetLastError());
        return 0; 
    }
    rh_job_done();
  }
  ruby_finalize();
#ifdef DEBUG_THREADS
  rh_log("RubyHoldem thread %d finished\n", GetCurrentThreadId());
#endif

  return 1;
}

void rh_init()
{
  DWORD dwThreadID;
  
  rh_log("rh_init();\n");
#ifdef DEBUG_THREADS
  rh_log("rh_init in thread %d\n", GetCurrentThreadId());
#endif

  rh_event_job = CreateEvent(
    NULL,                    // default security attributes
    FALSE,                   // auto-reset event
    FALSE,                   // initial state is nonsignaled
    NULL  // object name
  );

  if (rh_event_job == NULL)
  {
    rh_log("CreateEvent for job failed (%d)\n", GetLastError());
    return;
  }
  
  rh_event_done = CreateEvent(
    NULL,                    // default security attributes
    FALSE,                   // auto-reset event
    FALSE,                   // initial state is nonsignaled
    NULL  // object name
  );

  if (rh_event_done == NULL)
  {
    rh_log("CreateEvent for done failed (%d)\n", GetLastError());
    return;
  }

  rh_event_symbol_request = CreateEvent(
    NULL,                    // default security attributes
    FALSE,                   // auto-reset event
    FALSE,                   // initial state is nonsignaled
    NULL  // object name
  );

  if (rh_event_symbol_request == NULL)
  {
    rh_log("CreateEvent for symbol request failed (%d)\n", GetLastError());
    return;
  }

  rh_event_symbol_request_done = CreateEvent(
    NULL,                    // default security attributes
    FALSE,                   // auto-reset event
    FALSE,                   // initial state is nonsignaled
    NULL  // object name
  );

  if (rh_event_symbol_request_done == NULL)
  {
    rh_log("CreateEvent for symbol request done failed (%d)\n", GetLastError());
    return;
  }

  rh_thread = CreateThread(
    NULL,              // default security
    0,                 // default stack size
    rh_thread_proc,    // name of the thread function
    NULL,              // no thread parameters
    0,                 // default startup flags
    &dwThreadID);
  if (rh_thread == NULL)
  {
    rh_log("CreateThread failed (%d)\n", GetLastError());
  }
}

void rh_finalize()
{
  //DWORD dwWaitResult;
  
  rh_log("rh_finalize();\n");
#ifdef DEBUG_THREADS
  rh_log("rh_finalize in thread %d\n", GetCurrentThreadId());
#endif
  
  rh_start_job(RH_EXIT);
  
#if 0 // disabled, doesn't work but the thread exits, don't know why
  rh_log("Waiting for RubyHoldem thread in thread %d\n", GetCurrentThreadId());
  dwWaitResult = WaitForSingleObject(rh_thread, INFINITE);
  switch (dwWaitResult)
  {
    case WAIT_OBJECT_0:
      rh_log("RubyHoldem thread exited\n");
      break;
      
      // An error occurred
    default: 
      rh_log("WaitForMultipleObjects failed (%d)\n", GetLastError());
      return;
  }
#endif
}

BOOL APIENTRY DllMain(HANDLE module, DWORD reason, LPVOID reserved)
{
  switch (reason)
  {
    case DLL_PROCESS_ATTACH:
      rh_init();
      break;
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      break;
    case DLL_PROCESS_DETACH:
      rh_finalize();
      break;
  }
  return TRUE;
}
