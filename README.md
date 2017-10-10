# Reflow Oven Controller - Soldering

Software to build a reflow solder oven using a cheap pizza oven.

Build using [the xpcc microcontroller framework](http://xpcc.io).


## Hardware

* [Cheap pizza oven](http://www.ebay.de/itm/401355469313) (or similar)
* Solid State Relais (e.g. *Fotek SSR-25 DA*)
* SSD1306 OLED Display (128x64 px, 1")
* Pt100 temperature sensor (and LTC2984 measurement IC)
* High temperature wire ([e.g.](http://fuehlerdirekt.de/shop/Zubehoer/Anschlussleitung/Glassseideisoliertes-Kabel-mit-Edelstahlmantelgeflecht-4-Leiter.html))
* 5V Power supply


## Get the code

```sh
git clone --recursive https://github.com/rleh/reflow-oven-xpcc.git
```

The repository contains the lastest release of the xpcc framework as a git submodule,
a `SConstruct` file for the [xpcc build system](http://xpcc.io/reference/build-system/#build-commands) and the source code.
