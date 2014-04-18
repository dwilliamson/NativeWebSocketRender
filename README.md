NativeWebSocketRender
=====================

Run Server.exe and navigate to NativeWebSocketRender.html with Chrome or Firefox (untested on IE or others). You should see a plasma effect animate in the browser.

The C++ Server application hosts a WebSockets server that generates a plasma effect. The buffer to which the plasma effect is rendered is sent to the browser via **localhost**, demonstrating base-line performance/latency for an uncompressed data set.
