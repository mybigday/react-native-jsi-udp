import {
  assert,
  assertEqual,
  closeSockets,
  createBoundSocket,
  getLoopbackAddress,
  getWildcardAddress,
  type TestSuite,
} from './helper';

export const addressSuite: TestSuite = {
  id: 'address',
  name: 'Address info',
  description:
    'Checks address(), family reporting, and wildcard bind behavior for IPv4/IPv6.',
  tests: [
    {
      id: 'address-udp4-loopback',
      name: 'reports IPv4 address information for a bound udp4 socket',
      run: async () => {
        const socket = await createBoundSocket(
          'udp4',
          0,
          getLoopbackAddress('udp4')
        );

        try {
          const info = socket.address();
          assertEqual(info.family, 'IPv4');
          assertEqual(info.address, getLoopbackAddress('udp4'));
          assert(info.port > 0, 'Expected a bound IPv4 port');

          return `${info.address}:${info.port}`;
        } finally {
          closeSockets(socket);
        }
      },
    },
    {
      id: 'address-udp6-loopback',
      name: 'reports IPv6 address information for a bound udp6 socket',
      run: async () => {
        const socket = await createBoundSocket(
          'udp6',
          0,
          getLoopbackAddress('udp6')
        );

        try {
          const info = socket.address();
          assertEqual(info.family, 'IPv6');
          assert(info.address.includes(':'), 'Expected an IPv6 address string');
          assert(info.port > 0, 'Expected a bound IPv6 port');

          return `${info.address}:${info.port}`;
        } finally {
          closeSockets(socket);
        }
      },
    },
    {
      id: 'address-udp4-wildcard',
      name: 'preserves 0.0.0.0 when bound to the IPv4 wildcard address',
      run: async () => {
        const socket = await createBoundSocket(
          'udp4',
          0,
          getWildcardAddress('udp4')
        );

        try {
          const info = socket.address();
          assertEqual(info.address, '0.0.0.0');
          assertEqual(info.family, 'IPv4');

          return `${info.address}:${info.port}`;
        } finally {
          closeSockets(socket);
        }
      },
    },
  ],
};
