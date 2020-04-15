# ovrly

The purpose of this project is to compose [chromium embedded](https://bitbucket.org/chromiumembedded/cef/src/master/)
with [OpenVR](https://github.com/ValveSoftware/openvr) to allow creation of overlays using primarily web technologies.

Besides using cef to render the content of overlays placed in the VR world, an OS rendered browser instance will
also be used as the primary UI with its javascript context being used for as much application logic as possible.

## Javascript API

To facilitate ease of development and updates it is desirable to have as much logic implemented in javascript as possible.
While logic in the input->render loop is time sensitive and must be implemented in native code, a large majority is not.

    Some experiments need to be conducted as to what kind of latency is introduced by shifting logic into js.
    The primary use-case which may need to be done in native code, but possibly could be done in js, is what
    effectively could be considered the ovrly "window manager".

    Will js be able to track input controller pose and button updates quickly enough to detect intersections
    and interactions, execute logic, and then update overlay state with low enough latency that the
    interaction experience will be comfortable?

All javascript functionality will be exposed beneath the `window.ovrly` object, its existence can be used by
code to detect it is running in the ovrly runtime environment.

*ovrly.initialized* (bool): Whether everything was successfully initialized.
If this is false, only the string property `ovrly.error` will exist with a description of the init error.
The js context in overlay browser views will never be created if ovrly was not able to properly be initialized.

### Overlay

The primary purpose of ovrly is to expose and manage one or more overlays into the OpenVR environment.
The `ovrly.Overlay` constructor function creates just such an overlay and allows access to read and modify its properties.

TODO: Hash out the overlay JS interface

### OpenVR

Information about the VR environment is available through the OpenVR api. An idiomatic javascript version of this
api is exposed under `ovrly.vr`.

TODO: Hash out the openvr JS interface

### Facade

Owing to the archaic nature of the native interface exposed by v8, the API exported from native code into the
js context is rudimentary. To make this API more idiomatic javascript, a facade will sit on top
of the native exports to bridge that gap.

### Native to JS Event Dispatch

There will be single-source events from openvr and other modules which need to be dispatched to 1..n javascript contexts.
Not every context will be interested in every event, so a method is needed for subscribing and then dispatching specific
events raised in native code.

The cef lib includes a [message router](https://bitbucket.org/chromiumembedded/cef/src/master/include/wrapper/cef_message_router.h?at=master)
that's intended specifically for this purpose.
A message router is created in each render process, and one is created in
the browser process. The browser-process side router will watch for subscriptions from the js facade
and handle the required bookkeeping to deliver native events for the requested subscriptions.

The native code will keep as little state as possible while the javascript facade that interacts
with the native exports and message router will be in charge of keeping most state and updating
it in response to event messages from the router.

The message router js-side functions are exposed as `ovrlyNativeQuery` and `ovrlyNativeQueryCancel`.

## C++ Architecture

Because cef wraps chromium, the logic necessarily is split between processes;
architecturally, cef implements an event-driven interface.

The ovrly architecture is similarly implemented, with individual modules exposing
events that others can observe, with some modules implementing a `registerHooks()` function
that are used first-thing at app start to compose the the module event listeners.

Conventionally, primary modules in ovrly are in files that end with `ovrly` (e.g. uiovrly.cc, or vrovrly.h).

For example, any module that needs to run in the ovrly browser process would subscribe
to the `process::OnBrowser` event to be notified when the browser process is about to
come up. The `process::OnBrowser` observable also delivers a `process::Browser` object which
has additional process lifetime events that can be subscribed to (e.g. `OnContextInitialized`).

There is also a `process::OnRender` event that allows code that runs in the render processes
to be executed at the proper time, among a number of other events across different modules.

Because the observable system is meant for composing application code there is no facility
made for removing event subscriptions.

Module local code is wrapped in an anonymous namespace to prevent symbol exports.

## Browser to Overlay Rendering

The cef lib is in charge of creating the composited view of the off-screen browser, and it is OpenVR's job to render
that view into the overlay presented by the VR compositor. Shuffling this data between between the two is ovrly's task.

Page rendering is currently implemented using the [standard off-screen rendering technique](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage#markdown-header-off-screen-rendering)
demonstrated in the cefclient test application.
This provides a pixel buffer to the `OnPaint` function of the [CefRenderHandler interface](https://magpcss.org/ceforum/apidocs3/projects/(default)/CefRenderHandler.html).

For each frame rendered to `OnPaint`, this buffer is copied into the opengl texture that is bound to the openvr overlay.
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

Using this method, the framerate is driven by ovrly as it tells cef when it should render each frame with `SendExternalBeginFrame`.


## CEF Notes

As cef is a wrapper over chromium, it also shares chromiums multi-process design.

In a cef app there is one primary browser process with multiple threads and is generally responsible for
rendering to the UI, processing OS input events, doing disk and network IO, and handling general orchestration
of browser frame operations. This is the application startup process.

Each browser frame renders and executes javascript inside of a child process called the render process.

### Process Dispatch

When cef needs to fork a new child process, it executes its own executable again but with a special cli flag.
When the cef init code sees this flag, it will internally coopt the startup thread and only return when
the render process shuts down.

When the init function returns in this case, it returns `-1` so that the code knows this was a terminating
child render process and can `exit()` instead of setting up the browser process singleton.

As little work as possible should be done in the process before cef init function is called to
minimize render process load time.

### CefApp

Used by cef to find handlers for the browser process and render processes.
These handlers are primarily for lifetime events, threads, IPC, and V8 contexts of these processes.

### CefBrowserProcessHandler

This code runs in the browser process and is a singleton.

*Important Handlers*

*OnContextCreated:* Called when the browser process is bootstrapped and cef is fully ready to do things.
This is a good place to trigger initial browser display and loading other modules that require access to a ready-to-go cef.

### CefRenderProcessHandler

This code runs in the render process and is a singleton in its process.

*Important Handlers*

- *OnProcesMessageReceived:* When a render process gets a message from another process.

### CefClient

Used by cef to find handlers for each browser frame.

This is effectively an interprocess proxy for code in the browser process to interact with code
for the browser frame and v8 context running in the render processes.

These handlers are for communicating things like display, dialog, download, render,
request, focus, find, drag-n-drop, and loading events.

This code runs in the browser process and each browser can be created a unique instance of
this and/or each handler classes.

*Important Handlers*

- *OnProcesMessageReceived:* ? Is this when a render process sends the browser process a message or is it a
browser process mirror of the function with the same name on `CefRenderProcessHandler`?

### CefRenderHandler

Handlers for events related to rendering happening on the render thread for a particular browser instance.

This is a primary integration point for hooking render data when using cef to render browser views off-screen.

*Important Handlers*

- *OnPaint:* Provides a pixel buffer of the rendered browser view to the browser process each time the render process finishes a render.
- *OnAcceleratedPaint:* When using shared textures (currently broken) provides notification to the browser process after the render process updates the texture.
