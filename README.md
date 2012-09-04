midipipe4
=========

Another cross-platform, message-based (non-API) approach to sending and
receiving MIDI events.

Why use this library? Why not use Open Sound Control? Well, hopefully
this library will soon be compatible with Open Sound Control (OSC).
More to be said soon...

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
- OS support: OS X, Linux.
- Knows how to turn on realtime scheduling in OS X
  (`THREAD_TIME_CONSTRAINT_POLICY`) and Linux (`SCHED_FIFO`).
- Based on PortMidi (included).

Building
========

Here's how to build:

    make platform=mac # <-- if you're on OS X
    make platform=linux # <-- if you're on Linux (ALSA)

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

