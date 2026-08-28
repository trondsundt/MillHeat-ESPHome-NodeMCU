# MillHeat-ESPHome-NodeMCU

A  guide to replacing the HF-LPT120 V1 WiFi chip in the Mill Convection Heater (Gen2) with a NodeMCU using ESPHome.

> ## ⚠️ Outdated — use the maintained component instead
>
> This repository dates from 2023 and relies on ESPHome's old **custom component**
> mechanism (`esphome: includes: mill.h` + `climate: platform: custom`).
> That mechanism was **removed in ESPHome 2025.2.0**, and the climate API has changed
> twice since (`CLIMATE_SCHEMA` removed in 2025.11.0, `ClimateTraits::set_supports_*`
> removed in 2026.5.0). **`mill.h` will not compile on a current ESPHome.**
>
> For the same heater, use the maintained external component
> [`mill_panelheater_gen2`](https://devices.esphome.io/devices/mill-panelheater-gen2/)
> (source: [owangen/esphome](https://github.com/owangen/esphome)). It is a proper
> external component, adds a power sensor, and is kept in step with ESPHome releases:
>
> ```yaml
> external_components:
>   source:
>     type: git
>     url: https://github.com/owangen/esphome
>   components: [ mill_panelheater_gen2 ]
>
> esp8266:
>   board: d1_mini
>
> logger:
>   baud_rate: 0   # must be off, or the logger floods the same UART
>
> uart:
>   id: uart_bus
>   tx_pin: GPIO01
>   rx_pin: GPIO03
>   baud_rate: 9600
>
> climate:
>   - platform: mill_panelheater_gen2
>     name: Mill heater
> ```
>
> The wiring notes below still apply — in particular **GPIO02 must be held high**,
> which appears to be the "module is online" signal the heater expects.
>
> If you insist on running the code in this repository, either pin ESPHome to 2025.1,
> or use [robertklep/esphome-custom-component](https://github.com/robertklep/esphome-custom-component)
> to restore the `custom` platform and replace
> `traits.set_supports_current_temperature(true)` with
> `traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE)`
> (likewise for `set_supports_action`).

## FAQ

**Do the display and the +/- buttons still work?** Yes. The wifi module (HF-LPT120) is
only a UART peer; the heater's own board drives the display and the buttons. The ESP
listens to the status frames and keeps Home Assistant in sync with changes made on the
heater itself.

**Can I use an ESP-01S / NodeMCU / Wemos D1 mini?** All three. Change `esp8266: board:`
(`esp01_1m` / `nodemcuv2` / `d1_mini`); UART is on GPIO1 (TX) / GPIO3 (RX) on all of
them. The ESP-01S is the smallest and draws the least; NodeMCU is awkward to fit inside
the heater. NodeMCU and D1 mini also power their USB-serial chip from the heater's 3.3 V
rail — add 470–1000 µF across 3V3/GND. The EN-to-3V3 trick below is ESP-01 only.

**Is 1 MB flash too small?** No — the yaml here targets `board: esp01_1m`. Only caveat is
OTA, which needs room for the binary twice (under ~512 kB); a minimal build lands around
400–450 kB. If OTA complains about space, flash over USB-TTL or drop `captive_portal`.

⚠️ The API encryption key and OTA password committed in `mill-zero.yaml` are public —
generate your own.



## On the shoulders of giants

This project is based on initial work by [JDolven](https://github.com/JDolven/) and [metrafonic](https://github.com/metrafonic). Their repositories laid the groundwork and is the whole reason this repository exists.

* [JDolven/Replacing-HF_LPT120A-in-a-millheat-heater](https://github.com/JDolven/Replacing-HF_LPT120A-in-a-millheat-heater/)
* [metrafonic/MillHeat-ESPHome](https://github.com/metrafonic/MillHeat-ESPHome)

# Installing the software

* Put the mill.h and mill-zero.yaml files in your ESPHome folder
* Edit as necessary (api keys, wifi, etc)
* Flash/install to device
* Remove the HF-LPT120 chip from the heater
* Install and connect the NodeMCU as per instructions
* Turn on power 🥳

# Install

To be able to send commands on the TX pin, we must remove the existing HF-LP120 chip as it is holing the TX pin high (so that we cannot send data).

Here are the pin connections:
- PIN1 - GND
- PIN2 - 3v3 + EN
- PIN5 - RX
- PIN6 - TX
- PIN7 - GPIO02


ESP-01 - Notice that pin 2 is connected to 3v3 and the EN pin so that the esp01 can boot. This will not be needed for other devices such as the nodemcu.

We need GPIO02 to be set to high. I assume this is how the HF chip could communicate that it was online. [metrafonic](https://github.com/metrafonic) tried running it without this pin and it would stop working after a couple of minutes/hours.

![](IMG_20221018_225900.jpg)

## Debug:

Here are some of the outputs that the LP120 would send when using the mill app to set different settings:
```
SET 14 DEGREES
0x5a 0x00 0x10 0x1b 0x00 0x46 0x01 0x00 0x0e 0x00 0x00 0x00 0x00 0x00 0x80 0x5b 
0x5a 0x00 0x10 0x1c 0x00 0x50 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x7c 0x5b

SET 5 DEGREES
0x5a 0x00 0x10 0x1d 0x00 0x46 0x01 0x00 0x05 0x00 0x00 0x00 0x00 0x00 0x79 0x5b 
0x5a 0x00 0x10 0x1e 0x00 0x50 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x7e 0x5b

SET OFF
0x5a 0x00 0x10 0x1f 0x00 0x44 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x73 0x5b

SET ON
0x5a 0x00 0x10 0x20 0x00 0x44 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x75 0x5b 

SET 6 DEGREES
0x5a 0x00 0x10 0x22 0x00 0x46 0x01 0x00 0x06 0x00 0x00 0x00 0x00 0x00 0x7f 0x5b 
0x5a 0x00 0x10 0x23 0x00 0x50 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x83 0x5b 

FAN OFF
0x5a 0x00 0x10 0x26 0x00 0x48 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x7e 0x5b

FAN ON
0x5a 0x00 0x10 0x27 0x00 0x48 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x80 0x5b
```
