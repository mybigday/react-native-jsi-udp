import { Buffer } from 'buffer';
import udp from 'react-native-udp';
import type { TestSuite } from './helper';

const ECHO_PORT_BRIDGE = 12345;
const ECHO_DURATION_MS = 60000;

export const latencyServerRNUDPSuite: TestSuite = {
  id: 'latency-server-bridge',
  name: 'Latency server (react-native-udp)',
  description: `react-native-udp echo server on port ${ECHO_PORT_BRIDGE} using react-native-udp. Echoes all received packets for ${
    ECHO_DURATION_MS / 1000
  }s.`,
  tests: [
    {
      id: 'latency-echo-server-rn-udp',
      name: `react-native-udp echo server on port ${ECHO_PORT_BRIDGE} (${
        ECHO_DURATION_MS / 1000
      }s)`,
      run: async () => {
        const socket = udp.createSocket({ type: 'udp4' });
        let echoed = 0;

        socket.on(
          'message',
          (msg: Buffer, rinfo: { port: number; address: string }) => {
            socket.send(msg, 0, msg.length, rinfo.port, rinfo.address);
            echoed++;
          }
        );

        await new Promise<void>((resolve, reject) => {
          socket.bind(ECHO_PORT_BRIDGE, '0.0.0.0', (err?: Error) => {
            if (err) {
              reject(err);
            } else {
              resolve();
            }
          });
        });

        try {
          socket.setBroadcast(true);
          const { address, port } = socket.address();

          await new Promise<void>((resolve) => {
            setTimeout(resolve, ECHO_DURATION_MS);
          });

          return `Echoed ${echoed} packets on ${address}:${port}`;
        } finally {
          socket.close();
        }
      },
    },
  ],
};
