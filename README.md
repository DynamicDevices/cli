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

![image](https://github.com/DynamicDevices/cli/assets/1537834/5f673324-1dae-42c7-b0a5-70502c80a2b2)

![image](https://github.com/DynamicDevices/cli/assets/1537834/9fd8513e-ca29-4a6e-9550-e468ce24f437)

The flow we are using is as follows

```
[
    {
        "id": "2809316698732fb3",
        "type": "tab",
        "label": "OpenThread Soak Testing",
        "disabled": false,
        "info": "",
        "env": []
    },
    {
        "id": "ade2d79005d0abd1",
        "type": "mqtt in",
        "z": "2809316698732fb3",
        "name": "",
        "topic": "ot/f4ce361832e86d55",
        "qos": "2",
        "datatype": "auto",
        "broker": "d8af39e4.ee4308",
        "nl": false,
        "rap": true,
        "rh": 0,
        "inputs": 0,
        "x": 330,
        "y": 280,
        "wires": [
            [
                "4f2de9be4ccafc82"
            ]
        ]
    },
    {
        "id": "8632e50a9a419a39",
        "type": "debug",
        "z": "2809316698732fb3",
        "name": "",
        "active": true,
        "tosidebar": true,
        "console": false,
        "tostatus": false,
        "complete": "false",
        "statusVal": "",
        "statusType": "auto",
        "x": 1170,
        "y": 160,
        "wires": []
    },
    {
        "id": "4f2de9be4ccafc82",
        "type": "json",
        "z": "2809316698732fb3",
        "name": "",
        "property": "payload",
        "action": "",
        "pretty": false,
        "x": 590,
        "y": 280,
        "wires": [
            [
                "8632e50a9a419a39",
                "19fbbf5b68c047c1"
            ]
        ]
    },
    {
        "id": "97664cc8eb92968b",
        "type": "inject",
        "z": "2809316698732fb3",
        "name": "Reset Plug",
        "props": [
            {
                "p": "topic",
                "vt": "str"
            },
            {
                "p": "payload"
            }
        ],
        "repeat": "300",
        "crontab": "",
        "once": false,
        "onceDelay": 0.1,
        "topic": "cmnd/tasmota_460742/POWER",
        "payload": "OFF",
        "payloadType": "str",
        "x": 320,
        "y": 420,
        "wires": [
            [
                "0f39ed25e413e5c1",
                "6be8d9d5b22aadc7",
                "9e6eaba9eb82538f"
            ]
        ]
    },
    {
        "id": "0f39ed25e413e5c1",
        "type": "mqtt out",
        "z": "2809316698732fb3",
        "name": "",
        "topic": "",
        "qos": "",
        "retain": "",
        "respTopic": "",
        "contentType": "",
        "userProps": "",
        "correl": "",
        "expiry": "",
        "broker": "d8af39e4.ee4308",
        "x": 1150,
        "y": 420,
        "wires": []
    },
    {
        "id": "c304d6a75d83593e",
        "type": "delay",
        "z": "2809316698732fb3",
        "name": "",
        "pauseType": "delay",
        "timeout": "20",
        "timeoutUnits": "seconds",
        "rate": "1",
        "nbRateUnits": "1",
        "rateUnits": "second",
        "randomFirst": "1",
        "randomLast": "5",
        "randomUnits": "seconds",
        "drop": false,
        "allowrate": false,
        "outputs": 1,
        "x": 700,
        "y": 500,
        "wires": [
            [
                "0f39ed25e413e5c1",
                "9e6eaba9eb82538f"
            ]
        ]
    },
    {
        "id": "6be8d9d5b22aadc7",
        "type": "change",
        "z": "2809316698732fb3",
        "name": "",
        "rules": [
            {
                "t": "set",
                "p": "payload",
                "pt": "msg",
                "to": "ON",
                "tot": "str"
            }
        ],
        "action": "",
        "property": "",
        "from": "",
        "to": "",
        "reg": false,
        "x": 510,
        "y": 500,
        "wires": [
            [
                "c304d6a75d83593e"
            ]
        ]
    },
    {
        "id": "d0c67ab7236cb4fc",
        "type": "mqtt in",
        "z": "2809316698732fb3",
        "name": "",
        "topic": "cmnd/tasmota_460742/POWER",
        "qos": "2",
        "datatype": "auto",
        "broker": "d8af39e4.ee4308",
        "nl": false,
        "rap": true,
        "rh": 0,
        "inputs": 0,
        "x": 350,
        "y": 160,
        "wires": [
            [
                "8632e50a9a419a39"
            ]
        ]
    },
    {
        "id": "19fbbf5b68c047c1",
        "type": "mqtt out",
        "z": "2809316698732fb3",
        "name": "Soak Test Logging",
        "topic": "soaktest/log/1",
        "qos": "",
        "retain": "",
        "respTopic": "",
        "contentType": "",
        "userProps": "",
        "correl": "",
        "expiry": "",
        "broker": "d8af39e4.ee4308",
        "x": 1190,
        "y": 340,
        "wires": []
    },
    {
        "id": "9e6eaba9eb82538f",
        "type": "function",
        "z": "2809316698732fb3",
        "name": "Make power state output JSON",
        "func": "\nmsg.payload = \"{ \\\"Power\\\": \\\"\" + msg.payload + \"\\\"}\";\n\nreturn msg;",
        "outputs": 1,
        "noerr": 0,
        "initialize": "",
        "finalize": "",
        "libs": [],
        "x": 1000,
        "y": 260,
        "wires": [
            [
                "19fbbf5b68c047c1"
            ]
        ]
    },
    {
        "id": "d8af39e4.ee4308",
        "type": "mqtt-broker",
        "name": "localhost",
        "broker": "127.0.0.1",
        "port": "1883",
        "clientid": "",
        "autoConnect": true,
        "usetls": false,
        "protocolVersion": "4",
        "keepalive": "60",
        "cleansession": true,
        "birthTopic": "",
        "birthQos": "0",
        "birthPayload": "",
        "birthMsg": {},
        "closeTopic": "",
        "closeQos": "0",
        "closePayload": "",
        "closeMsg": {},
        "willTopic": "",
        "willQos": "0",
        "willPayload": "",
        "willMsg": {},
        "sessionExpiry": ""
    }
]
```
