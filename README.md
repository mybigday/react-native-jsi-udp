# react-native-jsi-udp

High performance UDP socket for React Native using JSI.

## Installation

```sh
npm install react-native-jsi-udp
```

### Android

Add this rule if you are using ProGuard:

```proguard
-keep class com.jsiudp.** { *; }
```

### iOS

Add `NSLocalNetworkUsageDescription` to your `Info.plist` if you need do multicast:

```xml
<key>NSLocalNetworkUsageDescription</key>
<string>Allow local network access</string>
```

## Usage

```js
import dgram from 'react-native-jsi-udp';

// The API is like Node's dgram API
const socket = dgram.createSocket('udp4');
```

## Contributing

See the [contributing guide](CONTRIBUTING.md) to learn how to contribute to the repository and the development workflow.

## License

MIT

---

Made with [create-react-native-library](https://github.com/callstack/react-native-builder-bob)

---

<p align="center">
  <a href="https://bricks.tools">
    <img width="90px" src="https://avatars.githubusercontent.com/u/17320237?s=200&v=4">
  </a>
  <p align="center">
    Built and maintained by <a href="https://bricks.tools">BRICKS</a>.
  </p>
</p>
