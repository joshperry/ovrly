# ovrly

NB: this `linix` branch from master will probably persist for a bit (today: 4/21/23) until I get time to do windows integration testing.

    ovrly manages vr overlays and runs browser overlies

The purpose of this project is to compose [chromium embedded](https://bitbucket.org/chromiumembedded/cef/src/master/)
with [OpenVR](https://github.com/ValveSoftware/openvr) to allow rapid creation of, and experimentation with,
VR overlays whose logic and rendering are handled by web technologies.

Why Though? Well, though they are anemic feature-wise, up to this point they are the only HCI in the VR
world that can persist across different VR app executions. To use an anachronism, they're like the
[TSRs](https://en.wikipedia.org/wiki/Terminate_and_stay_resident_program) of the DOS days, which brought
the first instance of persistent multitasking into a single-tasking non-reentrant world.

This is the state of VR today.

Besides using cef to create the content of overlays placed in the VR world, an OS-windowed browser instance
will be shown as the primary desktop UI and hosts the root context where the ovrly core javascript-side
logic is executed.

## ovrlies

    Extensibility points should be rich and experimentally compelling.

An ovrlie is an "app" for ovrly.

The interface for integrating a custom ovrlie is meant to be simple while supporting the widest possible
range of target development and runtime environments. Because ovrly employs a fully-featured browser runtime
and HTTP server libraries are ubiquitous, it makes sense to use HTTP for content delivery and IPC.

Each ovrlie implementation must provide an http server executable (written in any OS executable format)
with a `--port` argument.

When loading an overlie, the server process is started as a child daemon and passed a random available local
highport, a root browser context is then spun up and pointed at `http://localhost:<randomport>/`.
This URL should serve the HTML/JS/CSS assets that implement the ovrlie client logic and UI.

- TODO: Do we need to synchronize browser load with the server process startup somehow?

The content area of the root browser context is rendered into a root VR overlay that is also created
at ovrlie start time, the ovrly javascript APIs are bound in the browser javascript context where
code can manipulate them at will.

A primary benefit offered by the daemon model is that overlies can directly access OS resources
that are not available through the ovrly browser environment. This access can be shared with the browser
context via HTTP requests or websockets back to the daemon.

- TODO: Expose desktop UI for ovrlies?

- TODO: Securing access to the server daemon
- TODO: Consider security and other aspects related to loading ovrlies directly from public https URIs
  - Any ovrlie without the need for a daemon could deliver assets over the internet.
  - (pro) Zero install or update
  - (con) Scripts loaded from the internet have access to the ovrly javascript API

## Javascript API

To facilitate ease of development and updates it is desirable to have as much logic implemented in javascript as possible.
While logic in the input->render loop is time sensitive and must be implemented in native code, a large majority is not.

    Some experiments need to be conducted as to what kind of latency is introduced by shifting logic into js.
    The primary use-case which may need to be done in native code, but possibly could be done in js, is what
    effectively could be considered the ovrly "window manager".

    Will js be able to track input controller pose and button updates quickly enough to detect intersections
    and interactions, execute logic, and then update overlay state with low enough latency that the
    interaction experience will be comfortable?

All javascript interface will be exposed beneath the global `ovrly` object, its existence can be used by
code to detect it is running in the ovrly runtime environment.

*ovrly.initialized* (bool): Whether everything was successfully initialized.
If this is false, only the string property `ovrly.error` will exist with a description of the init error.

### Overlay

The primary purpose of ovrly is to expose and manage one or more overlays into the OpenVR environment.

*ovrly.Overlay(options)*: Constructor function to create an overlay.

```js
{
  id: 'is.invr.ovrly-main',
  name: 'ovrly main',
  matrix: [3][4],
}
```

*ovrly.root* (Overlay): The root overlay instance created for an overlie. Does not exist in the ovrly root js context.

TODO: Hash out the overlay JS interface

### OpenVR


Information about the VR environment is available through the OpenVR API which is bound globally in
each js context at `ovrly.vr`.

- TODO: chaperone bounds/playarea info

- TODO: tracked device info

TODO: Hash out the openvr JS interface

### Facade

Owing to the archaic nature of the native interface exposed by v8, the API exported from native code into the
js context is rudimentary. To make this API more idiomatic javascript, a facade will sit on top
of the native exports to bridge that gap.

The interface described above is this ideomatic interface.

### Low-level JS Interface

This describes the low-level JS interface that the ideomatic facade described above uses under the covers.

### Native to JS Event Dispatch

There will be single-source events and APIs exposed in the browser process, for openvr and other modules,
to 1..n javascript contexts running separately in each render process.
For events, not every context will be interested in every type, so a method is needed for subscribing and
then dispatching specific events between processes.

Because of its high-performance and low-latency, [ZeroMQ](zeromq.org) was chosen for communications
between the browser process and the render processes. The cef interprocess messaging system was explored for
this task and was found to be lacking in a number of places for the efficient delivery of realtime data.

Because zmq supports zero-copy transport of opaque binary messages, and both the sending and receiving
sides are guaranteed to be processes of the same executable on the same machine, this should allow the use
of extremely high-performance methods of serializing data (see `serralize.hpp`).

The browser process sets up an xpub socket listening on localhost, to which each render process connects
using a sub socket and sets up needed subscriptions. Because zmq's IPC transport only supports unix
domain sockets, a localhost TCP connection is the only cross-platform method of IPC that it offers.

TODO: Implement xpub subscriptions to filter message topics on the server side instead of the client.

## C++ Architecture

The ovrly C++ architecture is primarily wrappers and specializations interconnected
via event dispatch.

A bulk of the modules in the project expose an interface which calls into the OpenVR or cef
dependency libraries. The modules are organized primarily in a data-oriented fashion.

The API exposed by each module, which facilitate interaction between the multiple modules, each
have both a functional interface and an event-driven interface. Event subscription is accomplished
with Observables to which many target listeners can subscribe.

### App

Implements the CefApp interface and is in charge of dispatching logic between code
intented to run in the browser or render process on startup.

It exposes two observables, `OnBrowser` and `OnRender` which are dispatched in
their respective process types.

It also exposes a task dispatch function `runOnMain` which will run a provided
code on the process' main thread.

### GFX

This is defines and implements access to the the graphics system like OpenGL,
Vulkan, or DirectX. It's primary purpose is to provide an abstract interface
over these different techs to provide for xplat.

This is accomplished through duck-typing; a platform-specific header is
included with conditional ifdefs, and the build system will compile/link only
the OS-specific source module.

Ovrly has fairly simple needs as far as the graphics subsystem is concerned. It
only needs to create and blit into a texture which is given to the OpenVR
overlay for rendering.

It's possible that OpenGL or Vulkan may eventually be the only needed backend.

### JS

This contains the logic that mediates between OpenVR and browser JS instances
(one in each render process). This acts similarly to other cef interfaces which
use IPC to single-source certain logic (like the gpu-process) so that we only
have a single connection to the OpenVR API.

On the browser process it uses the OpenVR API to enumerate the interface
(hardware, playarea, etc.) and receive events for changes to the environment
(input events, positional events, hardware state changes, etc), it also handles
requests for changes to the OpenVR state.

For delivering events it creates a listening ZMQ PUB socket, for taking
state-change requests it creates a listening ZMQ REP socket.

On the render process it connects to the browser process' ZMQ PUB socket to
recieve VR state-change events. It marshals this event data into the JS object
model using its V8 engine interface.

When JS functions are called that should mutate the VR state, it marshals this
data and makes a call to the browser process using the ZMQ REP socket and
marshals any response data back to the V8 callsite.

### Logging

A simple logging module based on spdlog. It's job is to initialize and make the
logger available to other modules.

### Main

This is the primary startup code for ovrly, it composes all the modules and
then calls cef functions to initialize the app. The rest of the application
logic branches from cef-triggered observables like `OnBrowser`.

### Manager

This is where code for managing the native instances of browsers and overlay
instances.

It currently does very little, but may eventually be where the
window-manageresque logic will reside.

### UI

This is mainly a CefClient implementation that handles the desktop UI for
ovrly. It's fairly xplat already as it uses chromium's views for creating and
managing most of the browser UI/UX.

### VR

This module handles most logic for interacting and marshaling data and events
between OpenVR, abstracting much of the interface to ovrly's needs so other
modules don't need to know much of anything about OpenVR.

In the future, this may be where support for other VR tech like OpenXR is
implemented (OpenXR doesn't yet have the concept of overlays yet).

One of the primary interfaces provided by this module is the `Overlay` abstract
class which handles base logic that can be used to implement overlays which
derive their visual surface from any source (e.g. a cef browser, a PNG, a
webcam, etc), and provides virtual functions like `onLayout` for delivering
events overlay shape changes.

### Web

Similar to UI, this is a CefClient implementation, though its implementation is
specific to the offscreen browser instances behind each VR overlay. It also
provides an implementation of the VR module's Overlay class.

It's primary task is to recieve the buffer and dirty rects provided by the
render process and hand them to the Overlay render logic, . It currently doesn't
implement much UX, but this is also where things like virtual keyboard and
mouse input handling would occur.

### CEF integration

Because cef wraps chromium to act as the ovrly user interface, the logic necessarily
is split between processes; architecturally, cef exposes an event-driven interface to
which a single handler can be provided from user code.

When interfacing with cef, a common pattern is used to map the cef handler interface to an ovrly Observable interface.

As an example of using the Observable module interface, any module that needs to run in the cef
browser process would subscribe to the `process::OnBrowser` event to be notified,
in the browser process, when it is about to come up. This observable also delivers a
`process::Browser` object which has additional process lifetime events that can be observed
(e.g. `OnContextInitialized`).

There is also a `process::OnRender` event for code that needs to run at render process
startup. These are found in the app module and are only a few among a number of other events
across different modules.

### Conventions

- Some modules expose a `registerHooks()` function that should be called at app start to allow the module to setup inter-module event listeners.
- Primary modules in ovrly are in files that end with `ovrly` (e.g. uiovrly.cc, or vrovrly.h).
- Because the observable system is meant for composing application code there is no facility made for removing event subscriptions.
- Module local code is wrapped in an anonymous namespace to prevent symbol exports.


## Browser to Overlay Rendering

The cef lib is in charge of creating the composited view of the off-screen browser, and it is OpenVR's job to render
that view into the overlay presented by the VR compositor. Shuffling this data between the two is ovrly's task.

Page rendering is currently implemented using the [standard off-screen rendering technique](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage#markdown-header-off-screen-rendering)
demonstrated in the cefclient test application.
This provides a pixel buffer to the `OnPaint` function of the [CefRenderHandler interface](https://magpcss.org/ceforum/apidocs3/projects/(default)/CefRenderHandler.html).

For each frame rendered to `OnPaint`, the buffer is copied into the DirectX/opengl texture that is bound to the openvr overlay.
The framerate is driven by cef and is configured by the `windowless_frame_rate` setting, this value can
be set for each overlay to balance the needs of the content with desired performance.

In order to keep the desktop UI hardware accelerated, ovrly will have cef use GPU rendering
(though it does support software rendering with some caveats).
For overlay UIs, cef will be pulling the rendered page frame from the GPU and then ovrly will copy it through the CPU,
and back to the GPU to the texture bound to the OpenVR overlay.

This is not optimally performant, but for the type of content meant for overlays, this limit on perf shouldn't be a huge limitation for now.

    texture = glCreateTexture()
    vrSetOverlayTexture(texture)
    cef osr onpaint(pixel buffer) { copyToTexture(texture, pixel buffer) }

### Shared Texture Optimization

There have been patches to cef in the past to support shared GPU textures so that the data need not travel through the CPU
when the consumer is also the GPU, but this functionality is currently broken as chromium recently implemented
a new compositing layer called [Viz](https://chromium.googlesource.com/chromium/src/+/master/services/viz/).

When the [new shared texture patch](https://bitbucket.org/chromiumembedded/cef/pull-requests/285/reimplement-shared-texture-support-for-viz/diff)
is merged into cef, ovrly will be able to mediate rendering pages into OpenVR at native VR framerates (if necessary for the overlay content)
by sharing a texture between the cef and openvr rendering pipelines.

    Using this method will allow ovrly to take over the main rendering loop by telling cef when it should render each frame with `SendExternalBeginFrame`.


## CEF Notes

As cef is a wrapper over chromium, it also shares chromiums multi-process design.

In a cef app there is one primary browser process with multiple threads and is
generally responsible for rendering to the UI, processing OS input events,
doing disk and network IO, and handling general orchestration of browser frame
operations. This is the application startup process.

Each browser frame renders and executes javascript inside of a child process
called the render process to sandbox untrusted code and data using the process
boundary (many sandboxing techs at the OS-level target processes).

There are also a number of utility processes, such as the gpu-process where
access to the GPU is mediated since access to the GPU is a privileged operation
(e.g. it's possible to scrape the user's screen). For simplicity, the rest of
the text will just be written as if the render process was the only other
child because our code doesn't touch the others.

### Process Dispatch

When cef needs to fork a new child process, it executes its own executable
again but with special cli flags. When the cef init code sees these flags, it
will internally coopt the startup thread and only return when the render
process shuts down.

When the init function returns in this case, it returns `-1` so that the code
knows this was a terminating child render process and can `exit()` instead of
setting up the browser process singleton.

As little work as possible should be done in the process before cef init
function is called to minimize child process load time.

### CefApp

This is an embedder-implemented collection of event dispatch interfaces used by
cef to find handlers for the browser process and render processes. These
handlers are primarily for lifetime events, threads, IPC, and V8 contexts of
these processes.

### CefBrowserProcessHandler

Event dispatch interface that contains functions cef code calls from the
browser process.

*Important Handlers*

*OnContextInitialized:* Called when the browser process is bootstrapped and cef
is fully ready to do things. This is a good place to trigger initial browser
display and loading other modules that require access to a ready-to-go cef.

### CefRenderProcessHandler

Event interface with functions that cef calls in the render process.

*Important Handlers*

*OnContextCreated:* When the javascript context for the render process is
created. This is where extensions to the JS environment should be hooked in.

*OnProcessMessageReceived:* When a render process gets a message from another
process.

### CefClient

Another embedder-implemented event dispatch interface collection with functions
used by cef that define operational logic for browser frames.

This interface is implemented by cef as an interprocess proxy instances in the
browser process for orchestration logic to interact with the browser frame and
v8 context running in the render processes.

These handlers are for communicating things like display, dialog, download,
render, request, focus, find, drag-n-drop, and loading events. This forms the primary
abstraction for UI and OS-specific logic; there is a separate impl for overlays
and for the desktop window as the UX differs quite a bit between the two.

When creating each new browser instance (render process), a unique instance of
this and/or each each of the dispatch interface impls can be created, or a
single instance can be used if the logic between all are absolutely identical
(i.e. no local state in the dispatch logic).

*Important Handlers*

- *OnProcesMessageReceived:* When a render process sends the browser process a message.

### CefRenderHandler

Another event dispatch interface with functions cef runs from the browser
process to respond to events related to rendering happening in the render
process for a particular browser frame.

This is a primary integration point for hooking render data when using cef to
render browser views in off-screen mode.

*Important Handlers*

*OnPaint:* Provides a pixel buffer of the rendered browser view to the browser
process each time the render process finishes a render.

*OnAcceleratedPaint:* When using shared textures (currently broken) provides
notification to the browser process after the render process updates the
texture.
