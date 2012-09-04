midipipe4
=========

Another cross-platform, message-based (non-API) approach to sending and
receiving MIDI events.

Rationale: Instead of having to worry about linking to special libraries
or importing special modules to use your your OS's native MIDI APIs, this
tool does that work for you. [PortMidi](http://portmedia.sourceforge.net/)
has already made this job fairly easy (and, this tool happens to use
PortMidi). But, with this tool, you don't have to link to anything -
not even to PortMidi, because this tool creates a pipe-based message
interface for your application to use.

Caveat: This tool is based on older (somewhat ad-hoc) ideas, and is not
caught up to most recent trends (like JSON!). The message boundaries
are also determined by EOL characters (which is great for humans), but
with the advent of some standards for framing (namely, WebSocket), more
framing options might also be a worthwhile pursuit. Also, running this
tool as a separate process, and incurring additional context switches,
carries some overhead (hence a desire to support an optional linkable
mode, so this operates in the same process/thread). In practice, this
tool works fairly nicely and there seems little need to worry about the
context switches.

Why not use Open Sound Control? Well, hopefully this tool will soon be
compatible with Open Sound Control (OSC).  More to be said soon...

<!---
I would also recommend looking into the JSON serialization (when
it becomes available) due to the simplicity of unversal applicability
of JSON.

OSC is popular in numerous environments (like
[Max](http://en.wikipedia.org/wiki/Max_(software))) and generally regarded
as a standard. Specific capabilities of OSC emphasize: limited addressing
(end point specification), timestamping and concurrent events that should
be scheduled entirely as a single unit.

JSON can mimick the same functionality as OSC by requiring a field for
timestamp, and using a JSON array to store a set of concurrent events.

(Btw, whoever wrote the reference implementations for Open Sound Control
is nuts! Reference implementations number on the order of 10klocs!
That is WAY too much for something that should be so simple.)
-->

Features
========

- Warning: feature refactoring is going to happen soon (especially, changing
  how endpoints are named and referenced)!!! (and soon after be fairly
  stabilized and useful)
- Can list MIDI endpoints (in and out ports).
- Can subscribe to many inputs at once, and can send to many outputs at once.
- Supports encoding as Sexps (Lisp). (To do: JSON and OSC)
- OS support: OS X (at least, whatever was around in 2008), Linux.
- Knows how to turn on realtime scheduling in OS X
  (`THREAD_TIME_CONSTRAINT_POLICY`) and Linux (`SCHED_FIFO`). This probably
  doesn't do you much good though since these scheduling priorities are
  only enabled in the tool itself, and not yet in your application.
- Based on PortMidi (included).

Building
========

Here's how to build:

    make PLATFORM=mac # <-- if you're on OS X

    sudo apt-get install libasound2-dev
    make PLATFORM=linux # <-- if you're on Linux (ALSA)

Examples
========

Usage to make a recording, and then play it back:

    midipipe4 -l # get a list of ports
    midipipe4 -i pmi:15 > my-recording.txt # make a recording
    midipipe4 -o pmo:14 < my-recording.txt # playback the recording

A simple metronome, in Python, using Sexps:

    TODO

Usage over a TCP-socket:

    # Using netcat:
    # N.B.: there is also an OpenBSD netcat which is missing the "-c" option.
    #       (and therefore, will not work!)
    nc.traditional -l -p 12345 -c "midipipe4 ..."

To do
=====

- Add a configuration file, to map human-friendly names to MIDI endpoints
  (using glob-style pattern matching). (especially for Linux!)
- Add an encoding for OSC (Open Sound Control).
- Add an encoding for JSON.
- Add socket modes (netcat-friendly vanilla TCP socket, TCP WebSocket, UDP OSC).
- Add support for Windows.

