// FIXME:
//
// - PortMidi timer will overflow after some time (10s of minutes?)
//
// TODO:
//
// - Make it possible to directly link midipipe3 into an application
//   (thus avoiding context switches, but at the risk of a realtime
//   lockup - FIXME: how to avoid realtime lockup?  watchdog?)
// - Can we get timestamps for any events? (Synaptics events)
// - Time synchronization (this is a generally open systems integration
//   problem)
// - Message filtering (e.g. filter out the annoying time messages from Yamaha
//   keyboards)
// - Hash on the port descriptions to determine device URI IDs.  This makes
//   them more resistant to change.
//
//      - and once that is working, then start printing messages whenever
//        MIDI ports are added to or removed from the system
//
// - Create a file-based input URI (to create a poor-man's multitrack
//   MIDI sequencer) (or something like that...)
// 
// Done:
//
// - Periodic time messages

#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
//#include <termios.h>
#include <fcntl.h>
#include <stdint.h>
//#include <RtMidi.h>
#include <portmidi.h>
#include <porttime.h>
#include <assert.h>
#include <vector>
#include <map>
#include <string>
#include <algorithm>

#define DRIVER_INFO NULL
//#define TIME_PROC ((long (*)(void *)) Pt_Time)
#define TIME_PROC ((long (*)(void *)) my_time_proc)
#define TIME_INFO NULL
#define INPUT_BUFFER_SIZE 1024
#define OUTPUT_BUFFER_SIZE 0
#define TIME_START Pt_Start(1, 0, 0) /* timer started w/millisecond accuracy */

#define ENABLE_CONNECT_MESSAGES true

int latency = 1; // in ms (XXX: how does this variable affect the system)
//int latency = 0; // in ms (XXX: how does this variable affect the system)

typedef std::map<std::string, int> uri_portid_map_type;
typedef std::map<int, PmStream *> pm_stream_map_type;
pm_stream_map_type pm_inputs;
pm_stream_map_type pm_outputs;
uri_portid_map_type connected_inputs;
uri_portid_map_type connected_outputs;
int g_next_portid = 0;

int64_t
to_ns (PmTimestamp x)
{
    //return ((uint32_t) (x - t0)) * 1000LL * 1000LL;
    return ((uint32_t) x) * 1000LL * 1000LL;
}

PmTimestamp
from_ns (int64_t x)
{
    return x / 1000 / 1000;
}

static long _t0;
static bool _initialized = false;
long
my_time_proc (void * _)
{
        if (!_initialized)
          {
            _t0 = Pt_Time ();
            _initialized = true;
          }
        return Pt_Time () - _t0;
}


void
print_help (const char * argv0)
{
    char argv0spaces[128];
    strncpy (argv0spaces, argv0, 128);
    memset (argv0spaces, ' ', strlen (argv0spaces));
    printf ("Usage: %s -h\n", argv0);
    printf ("       %s -l\n", argv0);
    printf ("       %s [-t] [-n]\n", argv0);
    printf ("       %s [-i <port_spec> [<port_spec> ...]]\n", argv0spaces);
    printf ("       %s [-o <port_spec> [<port_spec> ...]]\n", argv0spaces);
    printf ("\n");
    printf ("Options:\n");
    printf ("    -l                 - List ports\n");
    printf ("    -t                 - Print timestamps\n");
    printf ("    -n                 - Disable input throttle (useful if"
                                    " upstream pipe\n");
    printf ("                         is already throttling its output)\n");
    printf ("    -i <port-spec> ... - Create/connect input port(s)\n");
    printf ("    -o <port-spec> ... - Create/connect output port(s)\n");
}

void
print_namespace ()
{
    printf ("(xmlns:m \"http://aclevername.com/ns/20090207-midipipe4/#\")\n");
}


void
print_midi_message (const char * buf, int len, int64_t time, int port)
{
    const uint8_t status = buf[0];
    const uint8_t channel = status & 0x0f;
    const uint8_t key = buf[1];
    const uint8_t controller = buf[1];
    const uint8_t program = buf[1];
    const uint8_t velocity = buf[2];
    const uint8_t value = buf[2];
    const int32_t pitch = (buf[1] | buf[2] * 128) - 0x2000;
    //assert (status & 0x80);
    if (!(status & 0x80)) return;
    switch (status & 0xf0)
      {
      case 0x80:
        //assert (len == 3);
        printf ("(m:note_off %Ld %d %d %d %d)\n",
                time, port, channel, key, velocity);
        break;
      case 0x90:
        //assert (len == 3);
        printf ("(m:note_on %Ld %d %d %d %d)\n",
                time, port, channel, key, velocity);
        break;
      case 0xa0:
        {
            //assert (len == 3);
            const uint8_t pressure = buf[2];
            printf ("(m:key_pressure %Ld %d %d %d %d)\n",
                    time, port, channel, key, pressure);
        }
        break;
      case 0xb0:
        //assert (len == 3);
        printf ("(m:control_change %Ld %d %d %d %d)\n",
                time, port, channel, controller, value);
        break;
      case 0xc0:
        //assert (len == 2);
        printf ("(m:program_change %Ld %d %d %d)\n",
                time, port, channel, program);
        break;
      case 0xd0:
        {
            //assert (len == 2);
            const uint8_t pressure = buf[1];
            printf ("(m:channel_pressure %Ld %d %d %d)\n",
                    time, port, channel, pressure);
        }
        break;
      case 0xe0:
        //assert (len == 3);
        printf ("(m:pitch_bend %Ld %d %d %d)\n",
                time, port, channel, pitch);
        break;
      // TODO: sysex
      }
    //state->last_time = time;
    //fflush (stdout);
    return;
}

// Returns number of bytes put into buf
int
parse_midi_message (const char * str,
                    char * buf, int len,
                    int64_t * time, int * port)
{
    int channel;
    int key;
    int controller;
    int program;
    int velocity;
    int value;
    int pitch;
    int pressure;
    assert (len >= 3);

    if (sscanf (str, "(m:note_off %Ld %d %d %d %d)",
                time, port, &channel, &key, &velocity) == 5)
      {
        buf[0] = 0x80 | channel;
        buf[1] = key;
        buf[2] = velocity;
        return 3;
      }

    if (sscanf (str, "(m:note_on %Ld %d %d %d %d)",
                time, port, &channel, &key, &velocity) == 5)
      {
        buf[0] = 0x90 | channel;
        buf[1] = key;
        buf[2] = velocity;
        return 3;
      }

    if (sscanf (str, "(m:key_pressure %Ld %d %d %d %d)",
                time, port, &channel, &key, &pressure) == 5)
      {
        buf[0] = 0xa0 | channel;
        buf[1] = key;
        buf[2] = pressure;
        return 3;
      }

    if (sscanf (str, "(m:control_change %Ld %d %d %d %d)",
                time, port, &channel, &controller, &value) == 5)
      {
        buf[0] = 0xb0 | channel;
        buf[1] = controller;
        buf[2] = value;
        return 3;
      }

    if (sscanf (str, "(m:program_change %Ld %d %d %d)",
                time, port, &channel, &program) == 4)
      {
        buf[0] = 0xc0 | channel;
        buf[1] = program;
        return 2;
      }

    if (sscanf (str, "(m:channel_pressure %Ld %d %d %d)",
                time, port, &channel, &pressure) == 4)
      {
          buf[0] = 0xd0 | channel;
          buf[1] = pressure;
        return 2;
      }

    if (sscanf (str, "(m:pitch_bend %Ld %d %d %d)",
                time, port, &channel, &pitch) == 4)
      {
        int pitchx = pitch + 0x2000;
        assert (pitchx >= 0);
        buf[0] = 0xe0 | channel;
        buf[1] = pitchx & 0x7f;
        buf[2] = (pitchx >> 7) & 0x7f;
        return 3;
      }

    // TODO: sysex and misc other messages
    return -1;
}

void
print_portmidi_list (bool as_sexps = false)
{
    int i;
    int n;
    typedef std::map<int, std::string> foo_type;
    foo_type inputs;
    foo_type outputs;
    //
    for (i = 0; i < Pm_CountDevices (); ++i)
      {
        const PmDeviceInfo * info = Pm_GetDeviceInfo (i);
        if (info->input)
          {
            std::string s = "";
            inputs[i] = s + info->interf + ", "  + info->name;
          }
        if (info->output)
          {
            std::string s = "";
            outputs[i] = s + info->interf + ", "  + info->name;
          }
      }
    //
    if (!as_sexps)
      {
        printf ("Listing MIDI input/output ports.\n");
        printf ("\n");
        //
        printf ("Inputs:\n");
        for (foo_type::iterator x = inputs.begin ();
             x != inputs.end ();
             ++x)
          {
            printf ("    pmi:%d - %s\n", x->first, x->second.c_str ());
          }
        //
        printf ("\n");
        printf ("Outputs:\n");
        for (foo_type::iterator x = outputs.begin ();
             x != outputs.end ();
             ++x)
          {
            printf ("    pmo:%d - %s\n", x->first, x->second.c_str ());
          }
        printf ("\n");
      }
    else
      {
        for (foo_type::iterator x = inputs.begin ();
             x != inputs.end ();
             ++x)
          {
            // FIXME: need to sanitize the description (x->second) so that
            //        it doesn't contain quotes
            printf ("(m:input-port \"pmi:%d\" \"%s\")\n",
                    x->first,
                    x->second.c_str ());
          }
        //
        for (foo_type::iterator x = outputs.begin ();
             x != outputs.end ();
             ++x)
          {
            // FIXME: need to sanitize the description (x->second) so that
            //        it doesn't contain quotes
            printf ("(m:output-port \"pmo:%d\" \"%s\")\n",
                    x->first,
                    x->second.c_str ());
          }
          printf ("(m:end-of-ports)\n");
      }
}

void
enable_realtime ()
{
#ifdef USE_LINUX_SCHED_FIFO
    static bool enabled = false;
    if (enabled) return;
      {
        struct sched_param schp;
        int ret;
        ret = sched_get_priority_max (SCHED_FIFO);
        if (ret == -1)
          {
            perror ("sched_get_priority_max");
            assert (false);
          }
        //schp.sched_priority = ret;
        schp.sched_priority = 10;
        //
        ret = sched_setscheduler (0, SCHED_FIFO, &schp);
        if (ret == -1)
          {
            perror ("sched_setscheduler");
            assert (false);
          }
      }
    enabled = true;
#endif
}

void
set_nonblocking_stdin ()
{
#if 0
    struct termios oldt;
    struct termios newt;
    tcgetattr (0, &oldt);
    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO );
    tcsetattr (0, TCSANOW, &newt);
    //tcsetattr (0, TCSANOW, &oldt);
    return;
#endif
    int flags;
    flags = fcntl (0, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl (0, F_SETFL, flags);
    return;
}

bool g_eof_flag = false;
const char *
cr_fgets_stdin ()
{
    const int BUFLEN = 1024;
    static char buf[BUFLEN];
    static int pos = 0;
    static char cur = 0;
    static char last = 0;
    bool loop = true;
    if (g_eof_flag) { return NULL; }
    while (loop)
      {
        ssize_t x;
        x = read (0, &cur, 1);
        g_eof_flag = x == 0; 
        loop = x == 1;
        if (x == 1)
          {
            if (cur == '\r' && last == '\n') { last = cur; continue; }
            if (cur == '\n' && last == '\r') { last = cur; continue; }
            if (cur == '\n' || cur == '\n')
              {
                last = cur; 
                buf[pos] = 0;
                pos = 0;
                return buf;
              } 
            if (pos < BUFLEN - 1)
              {
                buf[pos++] = cur;
              }
          }
      }
    return NULL;
}

int
connect_input (std::string uri)
{
    int x;
    int ret = -1;
    const char * suri = uri.c_str ();

    if (connected_inputs.count (uri))
      {
        ret = connected_inputs[uri];
      }
    else if (sscanf (suri, "pmi:%d", &x) == 1)
      {
        assert (x < Pm_CountDevices ());
        assert (Pm_GetDeviceInfo (x)->input);
        connected_inputs[uri] = g_next_portid;
        Pm_OpenInput (&pm_inputs[g_next_portid],
                      x,
                      DRIVER_INFO,
                      INPUT_BUFFER_SIZE,
                      TIME_PROC,
                      TIME_INFO);
        // XXX: shouldn't this be an option?
        Pm_SetFilter (pm_inputs[g_next_portid],
                      PM_FILT_ACTIVE | PM_FILT_CLOCK);
        ret = g_next_portid;
        g_next_portid++;
      }

    if (ret != -1)
      {
        printf ("(m:connected-input \"%s\" %d)\n", suri, ret);
      }

    return ret;
}

int
connect_output (std::string uri)
{
    int x;
    int ret = -1;
    const char * suri = uri.c_str ();

    if (connected_inputs.count (uri))
      {
        ret = connected_outputs[uri];
      }
    else if (sscanf (suri, "pmo:%d", &x) == 1)
      {
        assert (x < Pm_CountDevices ());
        assert (Pm_GetDeviceInfo (x)->output);
        connected_outputs[uri] = g_next_portid;
        Pm_OpenOutput (&pm_outputs[g_next_portid],
                       x,
                       DRIVER_INFO,
                       OUTPUT_BUFFER_SIZE,
                       TIME_PROC,
                       TIME_INFO,
                       latency);
        ret = g_next_portid;
        g_next_portid++;
      }

    if (ret != -1)
      {
        printf ("(m:connected-output \"%s\" %d)\n", suri, ret);
      }

    return ret;
}

int
main (int argc, char * argv[])
{
    bool help = false;
    bool list = false;
    bool run = false;
    bool timestamps = false;
    bool no_throttle = false;

    TIME_START;

    {
        int c;
        int x;
        Pm_CountDevices (); // NOTE: it appears this or Pm_GetDeviceInfo
                            //       must be called before calling
                            //       Pm_OpenInput or Pm_OpenOutput
        while ((c = getopt (argc, argv, "hltno:i:")) != -1)
          {
            switch (c)
              {

              case 'l':
                list = true;
                assert (pm_inputs.empty ());
                assert (pm_outputs.empty ());
                assert (!help);
                break;

              case 'h':
                help = true;
                assert (pm_inputs.empty ());
                assert (pm_outputs.empty ());
                assert (!list);
                break;

              case 'i':
                assert (!connected_outputs.size ()); // inputs must come before outputs
                for (int i = optind - 1; i < argc; ++i)
                  {
                    int x;
                    if (argv[i][0] == '-') break;
                    if (connect_input (argv[i]) == -1)
                      {
                        assert (false);
                      }
                  }
                run = true; enable_realtime ();
                break;

              case 'o':
                for (int i = optind - 1; i < argc; ++i)
                  {
                    int x;
                    if (argv[i][0] == '-') break;
                    if (connect_output (argv[i]) == -1)
                      {
                        assert (false);
                      }
                  }
                run = true; enable_realtime ();
                break;

              case 't':
                timestamps = true;
                run = true; enable_realtime ();
                break;

              case 'n':
                no_throttle = true;
                break;

              }
          }
    }

    if (help + list + run != 1)
      {
        help = true;
        list = false;
        run = false;
      }

    if (help)
      {
        print_help (argv[0]);
      }

    if (list)
      {
        print_portmidi_list (false);
        fflush (stdout);
      }

    if (run)
      {
        int64_t cur_time;
        int64_t end_time = 0;
        int64_t throttle_time = 0;
        print_namespace ();
        set_nonblocking_stdin ();
        //TIME_START;
        while (true)
          {
            bool again = false;

            cur_time = to_ns (TIME_PROC(TIME_INFO));

            if (timestamps)
              {
                static int64_t last_time;
                static bool initialized = false;
                if (!initialized)
                  {
                    last_time = cur_time - 1000LL * 1000LL * 1000LL;
                    initialized = true;
                  }
                if ((cur_time - last_time) >= 1000LL * 1000LL * 1000LL)
                  {
                    printf ("(m:time %Ld)\n", cur_time);
                    //fflush (stdout);
                    again = true;
                    last_time += 1000LL * 1000LL * 1000LL;
                  }
              }

            // Check MIDI for input data:
            for (pm_stream_map_type::iterator i = pm_inputs.begin ();
                 i != pm_inputs.end ();
                 ++i)
              {
                int portid = i->first;
                PmStream * port = i->second;
                while (Pm_Poll (port))
                  {
                    PmEvent pmbuf[1];
                    int pmlen;
                    char buf[4]; // TODO: sysex
                    pmlen = Pm_Read (port, pmbuf, 1);
                    if (pmlen)
                      {
                        // FIXME: will overflows be handled gracefully?
                        buf[0] = Pm_MessageStatus (pmbuf[0].message);
                        buf[1] = Pm_MessageData1 (pmbuf[0].message);
                        buf[2] = Pm_MessageData2 (pmbuf[0].message);
                        print_midi_message (buf,
                                            3,
                                            // XXX: this is a hack because
                                            //      either ALSA or PortMidi
                                            //      seems to have been giving
                                            //      bad timestamps...?
                                            to_ns(TIME_PROC(TIME_INFO)),
                                            // XXX: this is what I would prefer:
                                            //to_ns (pmbuf[0].timestamp),
                                            portid);
                      }
                    again = true;
                  }
              }

            // Check stdin for input data:
            while (no_throttle || (cur_time - throttle_time) > 0)
              {
                const char * buf = NULL;
                buf = cr_fgets_stdin ();
                if (!buf) break;
                if (buf)
                  {
                    int64_t time;
                    int port;
                    const int BUFLEN = 1024;
                    char midi_buf[BUFLEN];
                    int ret;
                    again = true;
                    ret = parse_midi_message (buf,
                                              midi_buf, BUFLEN, &time, &port);
                    if (ret > 0 && pm_outputs.count (port) > 0)
                      {
                        end_time += std::max (0LL, time - end_time);
                        // Don't advance more than 2 seconds past the current
                        // time:
                        throttle_time = end_time - 2LL * 1000 * 1000 * 1000;
                        //end_time = std::max (end_time, time);
                        // TODO: need a priority queue (since PortMidi
                        //       requires monotonic ordering of events)
                        //       However!  When using ALSA as the backend,
                        //       things seem to work regardless of event
                        //       order.  Therefore I am not implementing
                        //       a priority queue right now.
                        PmTimestamp time1 = from_ns (time);
                        long msg = Pm_Message (midi_buf[0],
                                               midi_buf[1],
                                               midi_buf[2]);
                        Pm_WriteShort (pm_outputs[port], time1, msg);
                      }
                    if (ENABLE_CONNECT_MESSAGES && ret == -1)
                      {
                        // XXX: this block of code should not be subject to
                        //      rate throttling
                        char uri[1024];
                        // XXX: parsing strings is blech:
                        if (sscanf (buf, "(m:connect-input \"%[^\"]\")", uri) == 1)
                          {
                            if (connect_input (uri) == -1)
                              {
                                assert (false);
                              }
                          }
                        else if (sscanf (buf, "(m:connect-output \"%[^\"]\")", uri) == 1)
                          {
                            if (connect_output (uri) == -1)
                              {
                                assert (false);
                              }
                          }
                        else if (strcmp (buf, "(m:list-ports)") == 0)
                          {
                            print_portmidi_list (true);
                          }
                        else if (strcmp (buf, "(m:list-connections)") == 0)
                          {
                            uri_portid_map_type::iterator i;
                            for (i = connected_inputs.begin ();
                                 i != connected_inputs.end ();
                                 ++i)
                              {
                                printf ("(m:connected-input \"%s\" %d)\n",
                                        i->first.c_str (), i->second);
                              }
                            for (i = connected_outputs.begin ();
                                 i != connected_outputs.end ();
                                 ++i)
                              {
                                printf ("(m:connected-output \"%s\" %d)\n",
                                        i->first.c_str (), i->second);
                              }
                          }
                      }
                  }
              }

            if (again) { fflush (stdout); }

            if (g_eof_flag && (cur_time - end_time) > 1000LL * 1000 * 1000 / 10)
              {
                // If we're at the end of the stream (g_eof_flag),
                // don't terminate until 0.1 seconds after the last
                // MIDI event.
                //
                // TODO: make sure to flush remaining events...
                break;
              }

            if (again) continue;
            else       usleep (1000);
          }
      }
    return 0;
}
