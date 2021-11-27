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

## Device configuration

Configuration about the hardware itself is stored in `device-config.json` in the root of the SPIFFS file system.
Basic configuration is provided in `BaseDeviceConfig` that can be extended by the application:

```jsonc
{
    "type": "chicken-door", // type of device
    "model": "mk1", // hardware variant
    "instance": "default", // the instance name
    "description": "Chicken door", // human-readable description
    "mqtt": {
        "host": "...", // broker host name, look up via mDNS if omitted
        "port": 1883, // broker port, defaults to 1883
        "clientId": "chicken-door", // client ID, defaults to "$type-$instance" if omitted
        "topic": "devices/chicken-door" // topic prefix, defaults to "devices/$type/$instance" if omitted
    }
}
```

### Zeroconf

If the `mqttHost` parameter is omitted or left empty, we'll try to look up the first MQTT server via mDNS.
If we find a hit, we'll also use the port specified in mDNS.
If there are multiple hits, the first one is used.

If `mqttClientId` is omitted, we make up an ID from the device type and instance name.
If `mqttTopic` is omitted, we also invent one using device type and instance name.

## Application configuration

Applications typically require custom configuration that can be manipulated remotely.
This can be stored in JSON format in `config.json` locally, and is automatically synced with the retained `$TOPIC_PREFIX/config` topic.

## Remote commands

FarmHub devices support remote commands via MQTT.
Commands are triggered via retained MQTT messages sent to `$TOPIC_PREFIX/commands/$COMMAND`.
They typically respond under `$TOPIC_PREFIX/responses/$COMMAND`.

Once the device receives a command it deletes the retained message.
This allows commands to be sent to sleeping devices.

There are a few commands supported out-of-the-box:

### Echo

Whatever JSON you send to `commands/echo`, it will return it under `responses/echo`.

See `EchoCommand` for more information.

### Restart

Sending a message to `commands/restart` restarts the device immediately.

See `RestartCommand` for more information.

### Firmware update via HTTP

Sending a message to `commands/update` with a URL to a firmware binary (`firmware.bin`), it will instruct the device to update its firmware:

```jsonc
{
    "url": "https://github.com/.../.../releases/download/.../firmware.bin"
}
```

See `HttpUpdateCommand` for more information.

### File commands

The following commands are available to manipulate files on the device via SPIFFS:

- `commands/files/list` returns a list of the files
- `commands/files/read` reads a file at the given `path`
- `commands/files/write` writes the given `contents` to a file at the given `path`
- `commands/files/remove` removes the file at the given `path`

See `FileCommands` for more information.

### Custom commands

Custom commands can be registered via `MqttHandler.registerCommand()`.
