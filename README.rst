.. _nrf52_emg:

NRF52 EMG BLE Stream using Nordic Connect SDK with Zephyr v2.5.1
####################

Overview
********

This application samples the SAADC on the nRF52840 and sends the averaged
EMG value to a BLE client as notifications at a fixed interval.

Device name
***********

The peripheral advertises as ``NRF52_EMG``.

Client
******

The bundled Python client in ``src/app.py`` scans for ``NRF52_EMG`` and plots
the incoming notification stream.
