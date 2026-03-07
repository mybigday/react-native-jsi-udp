import { type Socket } from 'react-native-jsi-udp';
import {
  assert,
  assertIncludes,
  closeSockets,
  createBoundSocket,
  getWildcardAddress,
  reservePort,
  toErrorMessage,
  type TestSuite,
} from './helper';

const WILDCARD = getWildcardAddress('udp4');
const BUFFER_TARGET = 32768;

export const optionsSuite: TestSuite = {
  id: 'options',
  name: 'Socket options',
  description:
    'Covers buffer-size getters/setters, TTL APIs, broadcast flags, and same-port binds.',
  tests: [
    {
      id: 'options-buffer-sizes',
      name: 'updates send and receive buffer sizes',
      run: async () => {
        const socket = await createBoundSocket('udp4');

        try {
          socket.setRecvBufferSize(BUFFER_TARGET);
          socket.setSendBufferSize(BUFFER_TARGET);

          const recvBufferSize = socket.getRecvBufferSize();
          const sendBufferSize = socket.getSendBufferSize();

          assert(
            recvBufferSize >= BUFFER_TARGET,
            `Expected recv buffer >= ${BUFFER_TARGET}, received ${recvBufferSize}`
          );
          assert(
            sendBufferSize >= BUFFER_TARGET,
            `Expected send buffer >= ${BUFFER_TARGET}, received ${sendBufferSize}`
          );

          return `recv=${recvBufferSize}, send=${sendBufferSize}`;
        } finally {
          closeSockets(socket);
        }
      },
    },
    {
      id: 'options-ttl-and-broadcast',
      name: 'accepts broadcast, TTL, and multicast loopback settings',
      run: async () => {
        const socket = await createBoundSocket('udp4');

        try {
          socket.setBroadcast(true);
          socket.setBroadcast(false);
          socket.setTTL(42);
          socket.setMulticastTTL(8);
          socket.setMulticastLoopback(true);
          socket.setMulticastLoopback(false);

          return 'applied SO_BROADCAST, IP_TTL, IP_MULTICAST_TTL, and IP_MULTICAST_LOOP';
        } finally {
          closeSockets(socket);
        }
      },
    },
    {
      id: 'options-reuse-addr',
      name: 'documents same-port bind behavior for reuseAddr',
      run: async () => {
        const port = await reservePort('udp4', WILDCARD);
        const first = await createBoundSocket('udp4', port, WILDCARD, {
          reuseAddr: true,
        });
        let second: Socket | undefined;

        try {
          try {
            second = await createBoundSocket('udp4', port, WILDCARD, {
              reuseAddr: true,
            });
            return `same-port bind succeeded with reuseAddr on udp4:${port}`;
          } catch (error) {
            const message = toErrorMessage(error);
            assertIncludes(
              message,
              'EADDRINUSE',
              `Expected EADDRINUSE or a successful same-port bind, received ${message}`
            );
            return `same-port bind required reusePort instead of reuseAddr on udp4:${port}`;
          }
        } finally {
          closeSockets(first, second);
        }
      },
    },
    {
      id: 'options-reuse-port',
      name: 'allows two sockets with reusePort to bind the same port',
      run: async () => {
        const port = await reservePort('udp4', WILDCARD);
        const first = await createBoundSocket('udp4', port, WILDCARD, {
          reuseAddr: true,
          reusePort: true,
        });
        let second: Socket | undefined;

        try {
          second = await createBoundSocket('udp4', port, WILDCARD, {
            reuseAddr: true,
            reusePort: true,
          });
          return `two sockets shared udp4:${port} with reusePort`;
        } finally {
          closeSockets(first, second);
        }
      },
    },
  ],
};
