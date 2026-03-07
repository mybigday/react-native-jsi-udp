import { Buffer } from 'buffer';
import {
  assert,
  assertEqual,
  closeSockets,
  createBoundSocket,
  createPayload,
  getLoopbackAddress,
  sendAsync,
  toRemoteInfo,
  waitForMessage,
  waitForMessages,
  type TestSuite,
} from './helper';

const LOOPBACK = getLoopbackAddress('udp4');
const RAPID_MESSAGE_COUNT = 100;
const LARGE_PACKET_BYTES = 8 * 1024;

export const sendReceiveSuite: TestSuite = {
  id: 'send-receive',
  name: 'Send / receive',
  description:
    'Verifies loopback delivery, multi-kilobyte payload handling, zero-length packets, and rapid bursts.',
  tests: [
    {
      id: 'send-receive-string-loopback',
      name: 'sends a string to the same socket over loopback',
      run: async () => {
        const socket = await createBoundSocket('udp4', 0, LOOPBACK);

        try {
          const { port } = socket.address();
          const pendingMessage = waitForMessage(socket);
          await sendAsync(socket, 'hello-loopback', port, LOOPBACK);
          const { message, info } = await pendingMessage;

          assertEqual(message.toString(), 'hello-loopback');
          assertEqual(info.family, 'IPv4');
          assert(info.port > 0, 'Expected sender port information');

          return `from ${info.address}:${info.port}`;
        } finally {
          closeSockets(socket);
        }
      },
    },
    {
      id: 'send-receive-buffer-echo',
      name: 'round-trips a Buffer between two sockets',
      run: async () => {
        const client = await createBoundSocket('udp4', 0, LOOPBACK);
        const server = await createBoundSocket('udp4', 0, LOOPBACK);
        const payload = Buffer.from('buffer-echo');

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
          const pendingReply = waitForMessage(client);
          await sendAsync(client, payload, server.address().port, LOOPBACK);
          const { message } = await pendingReply;

          assertEqual(message.toString(), payload.toString());
          return `${message.length} bytes echoed`;
        } finally {
          closeSockets(client, server);
        }
      },
    },
    {
      id: 'send-receive-large-packet',
      name: 'delivers an 8 KiB packet over loopback',
      run: async () => {
        const sender = await createBoundSocket('udp4', 0, LOOPBACK);
        const receiver = await createBoundSocket('udp4', 0, LOOPBACK);
        const payload = createPayload(LARGE_PACKET_BYTES, 0x6c);

        try {
          const pendingMessage = waitForMessage(receiver, 5000);
          await sendAsync(sender, payload, receiver.address().port, LOOPBACK);
          const { message } = await pendingMessage;

          assertEqual(
            message.length,
            payload.length,
            'Expected the large packet length to survive the round-trip'
          );
          assertEqual(
            message[0],
            payload[0],
            'Expected the payload prefix to match'
          );
          assertEqual(
            message[message.length - 1],
            payload[payload.length - 1],
            'Expected the payload suffix to match'
          );

          return `${message.length} bytes received`;
        } finally {
          closeSockets(sender, receiver);
        }
      },
    },
    {
      id: 'send-receive-zero-length',
      name: 'delivers a zero-length packet',
      run: async () => {
        const sender = await createBoundSocket('udp4', 0, LOOPBACK);
        const receiver = await createBoundSocket('udp4', 0, LOOPBACK);

        try {
          const pendingMessage = waitForMessage(receiver);
          await sendAsync(
            sender,
            Buffer.alloc(0),
            receiver.address().port,
            LOOPBACK
          );
          const { message } = await pendingMessage;

          assertEqual(message.length, 0, 'Expected a zero-length UDP payload');
          return '0-byte datagram delivered';
        } finally {
          closeSockets(sender, receiver);
        }
      },
    },
    {
      id: 'send-receive-rapid-burst',
      name: 'delivers 100 packets sent in a tight burst',
      run: async () => {
        const sender = await createBoundSocket('udp4', 0, LOOPBACK);
        const receiver = await createBoundSocket('udp4', 0, LOOPBACK);
        const payloads = Array.from(
          { length: RAPID_MESSAGE_COUNT },
          (_, index) => `burst-${index}`
        );

        try {
          const pendingMessages = waitForMessages(
            receiver,
            RAPID_MESSAGE_COUNT,
            7000
          );
          await Promise.all(
            payloads.map((payload) =>
              sendAsync(sender, payload, receiver.address().port, LOOPBACK)
            )
          );
          const received = await pendingMessages;
          const receivedPayloads = new Set(
            received.map(({ message }) => message.toString())
          );

          assertEqual(
            receivedPayloads.size,
            RAPID_MESSAGE_COUNT,
            'Expected every burst payload to be delivered once'
          );
          payloads.forEach((payload) => {
            assert(receivedPayloads.has(payload), `Missing payload ${payload}`);
          });

          return `${receivedPayloads.size}/${RAPID_MESSAGE_COUNT} messages delivered`;
        } finally {
          closeSockets(sender, receiver);
        }
      },
    },
  ],
};
