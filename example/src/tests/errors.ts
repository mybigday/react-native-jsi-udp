import {
  assert,
  closeSockets,
  createBoundSocket,
  expectReject,
  expectThrow,
  getLoopbackAddress,
  sendAsync,
  type TestSuite,
} from './helper';

export const errorsSuite: TestSuite = {
  id: 'errors',
  name: 'Error handling',
  description:
    'Verifies invalid socket creation, invalid destination addresses, and closed-socket failures.',
  tests: [
    {
      id: 'errors-invalid-create-type',
      name: 'throws when datagram_create receives an invalid type',
      run: async () => {
        const datagramCreate = Reflect.get(globalThis, 'datagram_create');
        assert(
          typeof datagramCreate === 'function',
          'Expected a global datagram_create function'
        );

        const error = expectThrow(() => {
          Reflect.apply(datagramCreate, globalThis, [99]);
        }, 'E_INVALID_TYPE');

        return error.message;
      },
    },
    {
      id: 'errors-invalid-address',
      name: 'rejects sends to an invalid destination address',
      run: async () => {
        const socket = await createBoundSocket('udp4');

        try {
          const error = await expectReject(
            () =>
              sendAsync(socket, 'invalid-address', 12345, 'not-an-ip-address'),
            /EADDRNOTAVAIL|EINVAL/
          );

          return error.message;
        } finally {
          closeSockets(socket);
        }
      },
    },
    {
      id: 'errors-closed-socket-operations',
      name: 'throws EBADF for operations on a closed socket',
      run: async () => {
        const socket = await createBoundSocket(
          'udp4',
          0,
          getLoopbackAddress('udp4')
        );

        try {
          socket.close();

          expectThrow(() => {
            socket.address();
          }, 'EBADF');

          const sendError = await expectReject(
            () =>
              sendAsync(
                socket,
                'after-close',
                12345,
                getLoopbackAddress('udp4')
              ),
            'EBADF'
          );

          return sendError.message;
        } finally {
          closeSockets(socket);
        }
      },
    },
  ],
};
