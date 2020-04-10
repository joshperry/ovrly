# ovrly

The purpose of this project is to compose [chromium embedded](https://bitbucket.org/chromiumembedded/cef/src/master/)
with [OpenVR](https://github.com/ValveSoftware/openvr) to allow creation of overlays using primarily web technologies.

## Javascript API

To facilitate ease of development and updates it is desirable to have as much logic implemented in javascript as possible.
While logic in the input->render loop is time sensitive and must be implemented in native code, a large majority is not.

All javascript functionality will be exposed beneath the `window.ovrly` object, its existence can be used by
code to detect it is running in the ovrly runtime environment.

### Overlay

The primary purpose of ovrly is to expose and manage one or more overlays into the OpenVR environment.
The `ovrly.Overlay` constructor function creates just such an overlay and allows access to read and modify its properties.

TODO: Hash out the overlay JS interface

### OpenVR

Information about the VR environment is available through the OpenVR api. An idiomatic javascript version of this
api is exposed under `ovrly.vr`.

TODO: Hash out the openvr JS interface

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
