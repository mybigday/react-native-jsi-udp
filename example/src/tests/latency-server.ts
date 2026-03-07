import { Buffer } from 'buffer';
import {
  bindSocket,
  closeSockets,
  createSocket,
  toRemoteInfo,
  type TestSuite,
} from './helper';

const ECHO_PORT = 12345;
const ECHO_DURATION_MS = 60000;

export const latencyServerSuite: TestSuite = {
  id: 'latency-server',
  name: 'Latency server',
  description: `Echo server on port ${ECHO_PORT} for test_latency.py. Echoes all received packets for ${
    ECHO_DURATION_MS / 1000
  }s.`,
  tests: [
    {
      id: 'latency-echo-server',
      name: `echo server on port ${ECHO_PORT} (${ECHO_DURATION_MS / 1000}s)`,
      run: async () => {
        const socket = createSocket('udp4', { reuseAddr: true });
        let echoed = 0;

        socket.on('message', (msg: Buffer, info: unknown) => {
          const rinfo = toRemoteInfo(info);
          socket.send(msg, 0, msg.length, rinfo.port, rinfo.address);
          echoed++;
        });

        try {
          await bindSocket(socket, 'udp4', ECHO_PORT, '0.0.0.0');
          socket.setBroadcast(true);
          const { address, port } = socket.address();

          await new Promise<void>((resolve) => {
            setTimeout(resolve, ECHO_DURATION_MS);
          });

          return `Echoed ${echoed} packets on ${address}:${port}`;
        } finally {
          closeSockets(socket);
        }
      },
    },
  ],
};
