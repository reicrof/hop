# HOP (Header Only Profiler)

![alt text](https://github.com/reicrof/hop/blob/master/images/hop_icon.png)

[![Build Status](https://travis-ci.com/reicrof/hop.svg?branch=develop)](https://travis-ci.com/reicrof/hop)

HOP is a C++ real-time intrusive profiler that works on Linux, MacOs and Windows.

- Real-time
Traces are displayed in real time as they come in so you can see not only what is slow, but when it is slow. This makes interactive appliaction easier to profile.
- Intrusive : 
The traces are created from the application by adding macros directly in the code (as opposed to an non-instrusive profiler which usually either samples the application or adds call directly in the binary). This allows for a better control of the granularity of what to profile

## Overview
This project was heavily inspired RAD's Telemetry (http://www.radgametools.com/telemetry.htm) and a blog post by Rich Geldreich (https://richg42.blogspot.com/2015/02/a-telemetry-style-visualization-of.html). Having never used Telemetry before, I thought the tool looked amazing and gave a try at building my own version of it in the same way Rich did. The choice of going 'header only' was also inspired by Sean Barrett's stb libraries (https://nothings.org/).

HOP is a personal project of mine that I started during a hack day, but continued mostly at home in my spare weekend time (#nerd). It was part of the "What grinds my gear" theme of the hackday. I found that we did not have any good/reliable/simple tool for profiling our code. I looked at what was open source or free, and nothing seemed to really worked for me. The only tools that looked interesting were proprietary. Notably VTune and Rad's Telemetry. It was the latter that actually impressed me the most and so I tried to create my own version of it. (You can see a nice overview of the software at this link It is also a good intro to HOP as this they are very similar) I decided to go with a Header Only design because I always like the simplicity of integrating these third parties (see https://github.com/nothings/single_file_libs).

## Current Design
HOP uses shared memory to communicate with the viewer. On the client side (app that is profiled), every macro record the start/end time of its scope (using C++ RAII mechanism), and writes it to the shared memory. (The actual writing is only done when we reach the first depth level. They are thus batched by call tree). Every thread in the client app has its own memory/depth/data. A MPSC (Multi Producer Single Consumer) lock-free ring buffer is used to write the data to the shared memory to reduce the impact on the client side. The actual implementation comes from Mindaugas Rasiukevicius (https://github.com/rmind/ringbuf). The viewer is the single consumer and reads data on the shared memory as it comes in.  All the macros can be removed by simply not defining HOP_ENABLED.

## How to use
To profile using HOP only a few steps needs to be done.

- Add a preprocessor definition for `HOP_ENABLED` to activate HOP.
- Define `HOP_IMPLEMENTATION` before including `Hop.h` in **one** of your source file. (This will create all function definitions and static variables)
- [Linux Only] Link against rt `-lrt` for the shared memory

## Adding Traces
All HOP entry points are defined as macros so you can easily undefined them (don't define `HOP_ENABLED`)

`HOP_PROF( x )`
Create a simple guard with the name provided as x. The name MUST be a **static** const char*

`HOP_PROF_FUNC()`
Create a simple guard with the name of the current function. (Using __PRETTY_FUNCTION__ on MacOs/Linux and __FUNCTION__ on Windows)

`HOP_PROF_SPLIT( x )`
Used in combination with `HOP_PROF_FUNC()`, will terminate the scope of the previous trace and create a new one with the name specified by `x`.

`HOP_PROF_DYN_NAME( x )`
Create a guard with a dynamic string. You should use this one sparingly as it requires an additional hash and an immediate copy to the string table.

`HOP_ZONE( x )`
Push a zone associated with value "x" (with the zone ids spanning from [0-255], with 0 being the default zone). All subsequent traces in the scope of this guard will be associated with this zone.

`HOP_PROF_MUTEX_LOCK( x )`
Special type of trace for Mutex. This trace should be created before locking a mutex, and destroyed once the mutex is acquired. This allows the viewer to display the locks and highlight their owner in the viewer. x should be the address of the mutex

`HOP_PROF_MUTEX_UNLOCK( x )`
Special type of trace for Mutex unlock. Should be created when the mutex is unlocked.

`HOP_SET_THREAD_NAME( x )`
Set the name of the current thread. This will be shown in the colored label in the viewer. It is only set for each thread once (the first time the function is called).

In the file Hop.h, there are 2 macros that can be pre-defined

`HOP_SHARED_MEM_SIZE`
This is the size of the shared memory that the application will write to and that the viewer will read from. This is the size of the Multi Producer Single Consumer (MPSC) ring buffer that is used. The actual size of the memory will be this + the metadata necessary for HOP to work properly. If you find out you sometimes have spikes of traces that are dropped, you might want to increase the size of the ring buffer.

`HOP_MAX_THREAD_NB`
This is the max number of threads that the application will be able to trace. If you create more threads than this number, they won't be profiled.

## Navigation
Most of the interaction with the application is directly inspired from RAD's Ttelemetry, so you should refer to this video : https://www.youtube.com/watch?v=RE04LQffZfs

## Build
The only external thirdparty dependency is SDL2 (https://www.libsdl.org/). On Linux, you might also need to install gtk3 for the file browser. You can also turn off the native file browser by defining `USE_OS_FILE_DIALOG=0` when building.

## License
The Hop.h file is https://unlicense.org/, except for the MPSC part which is owned by Mindaugas Rasiukevicius (https://github.com/rmind/ringbuf) with a MIT License. The rest of the files are under the MIT License.

### Third parties used
- IO : https://www.libsdl.org/
- UI : https://github.com/ocornut/imgui
- MPSC : https://github.com/rmind/ringbuf
- File Browser (when enabled) : https://github.com/guillaumechereau/noc/blob/master/noc_file_dialog.h
