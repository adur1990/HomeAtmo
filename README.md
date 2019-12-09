# HomeAtmo
Simple ESP32 based BMP680 indoor air quality sensor.

## What?
This is a very basic project, which reads the sensor values from a BMP680 sensor (gas, pressure, temperature & humidity) and computes the [Indoor Air Quality (IAQ)](https://en.wikipedia.org/wiki/Indoor_air_quality).
If the IAQ reaches non-healthy values, an HTTP request is sent to an IFTTT webhook, which in turn sends a SMS about the air quality, so I can open the window.
Additionally, a simple HTTP server is used to show the current sensor values as well as a few historic data points.

## How?
At this point in time, this part is not ready. This part of the README will be update, as soon as available.
