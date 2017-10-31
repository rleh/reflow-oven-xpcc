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


# Ui

The oven has two buttons
### Start / Temperature button
This button starts the reflow process or increases the temperature by 5°C if the reflow process is already running.

### Stop / Mode button
To stop a running reflow process press this button.
If no reflow process is running the button is used to switch between the reflow modes (displayes in the upper right corner of the display):
* NoPb
* Pb
* Constant Temperature (e.g. T85 = 85°C)


## Get the code

```sh
git clone --recursive https://github.com/rleh/reflow-oven-xpcc.git
```

The repository contains the lastest release of the xpcc framework as a git submodule,
a `SConstruct` file for the [xpcc build system](http://xpcc.io/reference/build-system/#build-commands) and the source code.
