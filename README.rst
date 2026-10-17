.. _nrf52_emg:

NRF52 EMG BLE Stream using Nordic nRF Connect SDK v2.5.1
####################

Overview
********

This application samples the SAADC on the nRF52840 and sends the averaged
EMG value to a BLE client as notifications at a fixed interval.


Build
***********

west build
west build -b xiao_ble -p always
west flash -r uf2

west build; west flash -r uf2; python myserial.py
west flash -r uf2; python myserial.py

Device name
***********

The peripheral advertises as ``NRF52_EMG``.

Client
******

The bundled Python client in ``src/app.py`` scans for ``NRF52_EMG`` and plots
the incoming notification stream.
