import { Buffer } from 'buffer';
import {
  assert,
  delay,
  assertEqual,
  closeSockets,
  createBoundSocket,
  createSocket,
  getLoopbackAddress,
  getSocketId,
  sendAsync,
  toRemoteInfo,
  waitForMessage,
  waitForMessages,
  type TestSuite,
} from './helper';

const LOOPBACK = getLoopbackAddress('udp4');
const SOCKET_COUNT = 100;
const BURST_COUNT = 1000;
const LATENCY_ITERATIONS = 100;

export const stressSuite: TestSuite = {
  id: 'stress',
  name: 'Stress / performance',
  description:
    'Creates many sockets, moves 1000 packets in one burst, and records 100 echo round-trip timings.',
  tests: [
    {
      id: 'stress-create-and-close-100',
      name: 'creates and closes 100 sockets rapidly',
      run: async () => {
        const sockets = Array.from({ length: SOCKET_COUNT }, () =>
          createSocket('udp4')
        );

        try {
          const ids = new Set(sockets.map((socket) => getSocketId(socket)));
          assertEqual(
            ids.size,
            SOCKET_COUNT,
            'Expected a unique id for every socket'
          );
          return `${ids.size} sockets created and closed`;
        } finally {
          closeSockets(...sockets);
        }
      },
    },
    {
      id: 'stress-send-1000-packets',
      name: 'delivers 1000 packets between two sockets',
      run: async () => {
        const sender = await createBoundSocket('udp4', 0, LOOPBACK);
        const receiver = await createBoundSocket('udp4', 0, LOOPBACK);
        const payloads = Array.from(
          { length: BURST_COUNT },
          (_, index) => `stress-${index}`
        );

        try {
          const pendingMessages = waitForMessages(receiver, BURST_COUNT, 10000);
          // Send in batches to avoid kernel receive buffer overflow
          const BATCH_SIZE = 100;
          const port = receiver.address().port;
          for (let i = 0; i < payloads.length; i += BATCH_SIZE) {
            const batch = payloads.slice(i, i + BATCH_SIZE);
            await Promise.all(
              batch.map((payload) => sendAsync(sender, payload, port, LOOPBACK))
            );
            // Yield to let receiver drain
            await delay(0);
          }
          const received = await pendingMessages;
          const uniquePayloads = new Set(
            received.map(({ message }) => message.toString())
          );

          assertEqual(
            uniquePayloads.size,
            BURST_COUNT,
            'Expected to receive every stress payload once'
          );

          return `${uniquePayloads.size}/${BURST_COUNT} packets delivered`;
        } finally {
          closeSockets(sender, receiver);
        }
      },
    },
    {
      id: 'stress-latency-round-trip',
      name: 'records 100 echo round-trip timings',
      run: async () => {
        const client = await createBoundSocket('udp4', 0, LOOPBACK);
        const server = await createBoundSocket('udp4', 0, LOOPBACK);
        const samples: number[] = [];

        server.on('message', (message: Buffer, info: unknown) => {
          const remoteInfo = toRemoteInfo(info);
          server.send(
            message,
            0,
            message.length,
            remoteInfo.port,
            remoteInfo.address
          );
        });

        try {
          for (let index = 0; index < LATENCY_ITERATIONS; index += 1) {
            const payload = Buffer.from(`latency-${index}`);
            const startedAt = Date.now();
            const pendingMessage = waitForMessage(client, 2000);
            await sendAsync(client, payload, server.address().port, LOOPBACK);
            const { message } = await pendingMessage;

            assertEqual(message.toString(), payload.toString());
            samples.push(Date.now() - startedAt);
          }

          const total = samples.reduce((sum, value) => sum + value, 0);
          const average = total / samples.length;
          const max = Math.max(...samples);
          const min = Math.min(...samples);

          assert(
            samples.length === LATENCY_ITERATIONS,
            'Expected 100 latency samples'
          );
          return `avg=${average.toFixed(1)}ms, min=${min}ms, max=${max}ms`;
        } finally {
          closeSockets(client, server);
        }
      },
    },
  ],
};
