# FarmHub IoT platform client

[![PlatformIO CI](https://github.com/kivancsikert/farmhub-client/actions/workflows/build.yml/badge.svg)](https://github.com/kivancsikert/farmhub-client/actions/workflows/build.yml)

FarmHub is an ecosystem of IoT devices built around a local hub installed at the farm.
The hub provides services like an MQTT broker and NTP to the devices via a reliable WIFI connection.
The hub itself is the only device connected to the internet, and manages problems with unreliable internet connection.

The platform provides the foundation to build these IoT devices that:

- are built on an Espressif ESP32 micro-controller using the Arduino framework
- have WIFI available locally, but are expected to function when service is unavailable
- store configuration about the device in JSON configuration files in SPIFFS
- connect to an unauthenticated MQTT broker on the local network at the farm under a given topic prefix to
  - send telemetry on a regular basis,
  - accept commands
- support remote firmware updates via both OTA and HTTP(S) (initiated via MQTT commands)

See [examples/SimpleApp] for an example application.

There are some optional services these devices can use:

- simple task scheduling via `TaskContainer`

## Configuration

### Device configuration

Configuration about the hardware itself is stored in `device-config.json` in the root of the SPIFFS file system.
Basic configuration is provided in `BaseDeviceConfig` that can be extended by the application:

```jsonc
{
    "type": "chicken-door", // type of device
    "model": "mk1", // hardware variant
    "description": "Chicken door" // human-readable description
}
```

### MQTT configuration

`MqttHandler` reads this configuration file under `mqtt-config.json`:

```jsonc
{
    "host": "...", // broker host name
    "port": 1883, // broker port, defaults to 1883
    "clientId": "chicken-door", // client ID
    "prefix": "devices/chicken-door" // topic prefix
}
```

If the `host` parameter is omitted, we'll try to look up the first MQTT server via mDNS.
If we find a hit, we'll also use the port specified in mDNS.
If there are multiple hits, the first one is used.

### Application configuration

Applications typically require custom configuration.
This can be stored in JSON format in `config.json` locally, and is automatically synced with the retained `$TOPIC_PREFIX/config` topic.
