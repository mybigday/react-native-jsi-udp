import {
  assertEqual,
  closeSockets,
  createBoundSocket,
  getWildcardAddress,
  sendAsync,
  skipOnPlatform,
  waitForMessage,
  type TestSuite,
} from './helper';

const BROADCAST_ADDRESS = '255.255.255.255';
const MESSAGE = 'broadcast-ping';
const WILDCARD = getWildcardAddress('udp4');

export const broadcastSuite: TestSuite = {
  id: 'broadcast',
  name: 'Broadcast',
  description:
    'Sends a UDP broadcast packet to a listener on the same device when the runtime supports it.',
  tests: [
    {
      id: 'broadcast-loopback',
      name: 'receives a local UDP broadcast packet',
      skip: skipOnPlatform(
        ['android'],
        'Android emulator broadcast loopback is unreliable for automated verification.'
      ),
      run: async () => {
        const receiver = await createBoundSocket('udp4', 0, WILDCARD, {
          reuseAddr: true,
          reusePort: true,
        });
        const sender = await createBoundSocket('udp4', 0, WILDCARD);

        try {
          receiver.setBroadcast(true);
          sender.setBroadcast(true);

          const pendingMessage = waitForMessage(receiver, 5000);
          await sendAsync(
            sender,
            MESSAGE,
            receiver.address().port,
            BROADCAST_ADDRESS
          );
          const { message } = await pendingMessage;

          assertEqual(message.toString(), MESSAGE);
          return `broadcast delivered on udp4:${receiver.address().port}`;
        } finally {
          closeSockets(receiver, sender);
        }
      },
    },
  ],
};
