# OpenThread CLI

## FORK OF NORDIC CLI EXAMPLE

- we've added a build configuration for nRF52840 dongle
- we've added USB serial overlay for this dongle
- we've added TCP overlay for dongle
- we've added multiprotocol (BLE) overlay for dongle future testing
- this now uses a fork of the MQTT-SN enabled OpenThread for publication

NOTE: You need to replace `~/ncs/v2.4.0/modules/lib/openthread` with the branch from here https://github.com/DynamicDevices/openthread-upstream/tree/nrf-connect-with-mqtt-sn

## NOTES on LED output

**TBD** - Shreya

## NOTES on soak testing

- Build a version of the CLI with the serial console waiting disabled `CONFIG_WAIT_FOR_CLI_CONNECTION=n`
- Flash a Tasmota plug and configure it to connect to `mqtt.dynamicdevices.co.uk`. You'll need to disable the MQTT user/password in the console with `mqttuser 0` and `mqttpasswd 0`
- The plug will then connect to the broker and you can see the pub/sub topic prefix in the console which will be based on the MAC address e.g. `cmnd/tasmota_460742/POWER`

- Create a Node-Red flow to subscribe to the topic on which the CLI firmware is publishing, also based on the CLI EUI64 e.g. `ot/f4ce361832e86d55`
- Then set an injection node to periodically publish to the plug to turn it off, delay, then turn it on again
- Connect the CLI dongle device to a USB power plug attached to the Tasmota plug

- You can then log the output as the CLI publishes periodically and is turned off and on again
