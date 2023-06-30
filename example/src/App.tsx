import * as React from 'react';

import { StyleSheet, View, Text, Pressable } from 'react-native';
import dgram, { Socket } from 'react-native-jsi-udp';

export default function App() {
  const socket = React.useRef<Socket | undefined>(undefined);
  const [isBound, setIsBound] = React.useState(false);
  const [address, setAddress] = React.useState('');
  const [port, setPort] = React.useState(0);

  const stop = React.useCallback(() => {
    socket.current?.close();
  }, []);

  const start = React.useCallback(() => {
    if (socket.current) return;
    socket.current = dgram.createSocket('udp4');
    socket.current!.on('error', (err) => {
      console.log('got error', err);
    });
    socket.current!.on('message', (msg, rinfo) => {
      console.log('got message', msg.toString());
      if (msg.toString() === 'hello') return;
      socket.current?.send(msg, 0, msg.length, rinfo.port, rinfo.address);
    });
    socket.current!.on('close', () => {
      setIsBound(false);
      socket.current = undefined;
    });
    socket.current!.bind(12345, () => {
      socket.current!.setBroadcast(true);
      setIsBound(true);
      const info = socket.current!.address();
      setPort(info.port);
      setAddress(info.address);
      // Send broadcast, on real iOS device may trigger permission dialog
      socket.current!.send('hello', 0, 5, info.port, '255.255.255.255');
      // Multicast also
      socket.current!.send('hello', 0, 5, info.port, '224.0.0.1');
    });
  }, []);

  return (
    <View style={styles.container}>
      <Text style={styles.text}>Bound: {isBound ? '(YES)' : '(NO)'}</Text>
      <Text style={styles.text}>Address: {address}</Text>
      <Text style={styles.text}>Port: {port}</Text>
      <Pressable
        style={({ pressed }) => [styles.box, pressed && styles.pressed]}
        onPress={isBound ? stop : start}
      >
        <Text style={styles.text}>{isBound ? 'Stop' : 'Start'}</Text>
      </Pressable>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'center',
    backgroundColor: '#fff',
  },
  box: {
    margin: 5,
    padding: 8,
    backgroundColor: '#2196F3',
  },
  pressed: {
    opacity: 0.5,
  },
  text: {
    fontSize: 20,
  },
});
