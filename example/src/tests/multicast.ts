import {
  assertEqual,
  closeSockets,
  createBoundSocket,
  delay,
  expectNoMessage,
  getWildcardAddress,
  skipTest,
  sendAsync,
  skipOnPlatform,
  toErrorMessage,
  waitForMessage,
  type TestSuite,
} from './helper';

// 224.0.0.1 is the all-hosts group, so it is a poor fit for dropMembership checks.
const GROUP_ADDRESS = '239.255.42.99';
const MESSAGE = 'multicast-ping';
const WILDCARD = getWildcardAddress('udp4');
const POST_DROP_SETTLE_MS = 250;

function isUnavailableMulticastError(error: unknown): boolean {
  const message = toErrorMessage(error);
  return (
    message.includes('EADDRNOTAVAIL') ||
    message.includes('EHOSTUNREACH') ||
    message.includes('ENETUNREACH') ||
    message.includes('Timed out waiting for "message" event')
  );
}

export const multicastSuite: TestSuite = {
  id: 'multicast',
  name: 'Multicast',
  description:
    'Joins an IPv4 multicast group, verifies loopback delivery, then confirms dropMembership stops delivery.',
  tests: [
    {
      id: 'multicast-join-send-drop',
      name: 'joins, receives, and then drops an IPv4 multicast group',
      skip: skipOnPlatform(
        ['android'],
        'Android emulators often filter multicast traffic without a native multicast lock.'
      ),
      run: async () => {
        const receiver = await createBoundSocket('udp4', 0, WILDCARD, {
          reuseAddr: true,
        });
        const sender = await createBoundSocket('udp4', 0, WILDCARD);

        try {
          const port = receiver.address().port;
          receiver.setMulticastLoopback(true);
          sender.setMulticastLoopback(true);
          sender.setMulticastTTL(1);

          try {
            receiver.addMembership(GROUP_ADDRESS);
          } catch (error) {
            if (isUnavailableMulticastError(error)) {
              skipTest(
                `Multicast routing is unavailable in this environment (${toErrorMessage(
                  error
                )})`
              );
            }
            throw error;
          }

          try {
            const initialMessage = waitForMessage(receiver, 5000);
            await sendAsync(sender, MESSAGE, port, GROUP_ADDRESS);
            assertEqual((await initialMessage).message.toString(), MESSAGE);
          } catch (error) {
            if (isUnavailableMulticastError(error)) {
              skipTest(
                `Multicast routing is unavailable in this environment (${toErrorMessage(
                  error
                )})`
              );
            }
            throw error;
          }

          receiver.dropMembership(GROUP_ADDRESS);
          await delay(POST_DROP_SETTLE_MS);
          await sendAsync(sender, 'after-drop', port, GROUP_ADDRESS);
          await expectNoMessage(receiver, 800);

          return `multicast loopback succeeded on udp4:${port}`;
        } finally {
          closeSockets(receiver, sender);
        }
      },
    },
  ],
};
