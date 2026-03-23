import { type Socket } from 'react-native-jsi-udp';
import {
  assert,
  assertEqual,
  closeSockets,
  createBoundSocket,
  createSocket,
  delay,
  getLoopbackAddress,
  getSocketId,
  reservePort,
  type TestSuite,
} from './helper';

export const lifecycleSuite: TestSuite = {
  id: 'lifecycle',
  name: 'Socket lifecycle',
  description:
    'Exercises socket creation, binding, rebinding protection, and close behavior.',
  tests: [
    {
      id: 'lifecycle-create-unique-sockets',
      name: 'creates udp4 and udp6 sockets with unique ids',
      run: async () => {
        const udp4 = createSocket('udp4');
        const udp6 = createSocket('udp6');

        try {
          const udp4Id = getSocketId(udp4);
          const udp6Id = getSocketId(udp6);

          assert(udp4Id > 0, 'Expected a positive udp4 socket id');
          assert(udp6Id > 0, 'Expected a positive udp6 socket id');
          assert(udp4Id !== udp6Id, 'Expected unique socket ids');

          return `udp4=${udp4Id}, udp6=${udp6Id}`;
        } finally {
          closeSockets(udp4, udp6);
        }
      },
    },
    {
      id: 'lifecycle-bind-random-and-explicit-port',
      name: 'binds to port 0 and then to a specific port',
      run: async () => {
        const loopback = getLoopbackAddress('udp4');
        const autoBound = await createBoundSocket('udp4', 0, loopback);
        const autoPort = autoBound.address().port;
        const explicitPort = await reservePort('udp4', loopback);
        let explicitBound: Socket | undefined;

        try {
          assert(autoPort > 0, 'Expected bind(0) to allocate a port');
          explicitBound = await createBoundSocket(
            'udp4',
            explicitPort,
            loopback
          );
          assertEqual(
            explicitBound.address().port,
            explicitPort,
            'Expected the explicit port to be preserved'
          );

          return `autoPort=${autoPort}, explicitPort=${explicitPort}`;
        } finally {
          closeSockets(autoBound, explicitBound);
        }
      },
    },
    {
      id: 'lifecycle-rebind-and-double-close',
      name: 'rejects rebinding and treats double-close as a no-op',
      run: async () => {
        const socket = await createBoundSocket('udp4');
        let closeEvents = 0;
        socket.on('close', () => {
          closeEvents += 1;
        });

        try {
          const port = socket.address().port;
          const error = (() => {
            try {
              socket.bind(port, getLoopbackAddress('udp4'));
              return undefined;
            } catch (bindError) {
              return bindError instanceof Error
                ? bindError
                : new Error(String(bindError));
            }
          })();

          assert(
            error instanceof Error,
            'Expected bind on a bound socket to throw'
          );
          assertEqual(
            error.message,
            'Socket is already bound',
            'Expected a duplicate bind error'
          );

          socket.close();
          socket.close();
          await delay(25);
          assertEqual(closeEvents, 1, 'Expected close to emit exactly once');

          return `closeEvents=${closeEvents}`;
        } finally {
          closeSockets(socket);
        }
      },
    },
    {
      id: 'lifecycle-close-after-native-cleanup',
      name: 'close after native closeAll does not throw',
      run: async () => {
        // Simulate the scenario where native closeAll/suspendAll clears the
        // internal fd map, then JS calls socket.close() during React unmount.
        const socket = await createBoundSocket('udp4');
        const port = socket.address().port;
        let closeEvents = 0;
        socket.on('close', () => {
          closeEvents += 1;
        });

        // Directly call the native close to remove the id from the fd map,
        // simulating what closeAll/suspendAll does.
        const datagramClose = Reflect.get(globalThis, 'datagram_close');
        assert(
          typeof datagramClose === 'function',
          'Expected a global datagram_close function'
        );
        const socketId = getSocketId(socket);
        Reflect.apply(datagramClose, globalThis, [socketId]);

        // Now calling socket.close() should not throw — the JS close should
        // gracefully handle the already-cleaned-up native socket.
        socket.close();
        await delay(25);
        assertEqual(closeEvents, 1, 'Expected close to emit exactly once');

        return `port=${port}, closeEvents=${closeEvents}`;
      },
    },
  ],
};
