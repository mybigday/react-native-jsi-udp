import { Buffer } from 'buffer';
import { Platform } from 'react-native';
import dgram, { type Options, type Socket } from 'react-native-jsi-udp';

export type SocketType = Options['type'];
export type TestStatus =
  | 'idle'
  | 'running'
  | 'passed'
  | 'failed'
  | 'skipped'
  | 'manual';

export interface TestCase {
  id: string;
  name: string;
  run?: () => Promise<string | void>;
  manualInstructions?: string;
  skip?: () => string | undefined;
}

export interface TestSuite {
  id: string;
  name: string;
  description: string;
  tests: TestCase[];
}

export interface TestResult {
  suiteId: string;
  testId: string;
  name: string;
  status: TestStatus;
  duration: number;
  details?: string;
  error?: string;
}

export interface RemoteInfo {
  address: string;
  port: number;
  family: string;
}

export interface ReceivedMessage {
  message: Buffer;
  info: RemoteInfo;
}

export class SkipTestError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'SkipTestError';
  }
}

const DEFAULT_TIMEOUT_MS = 3000;
const PORT_RELEASE_DELAY_MS = 50;

export function assert(condition: unknown, msg: string): asserts condition {
  if (!condition) {
    throw new Error(msg);
  }
}

export function assertEqual<T>(actual: T, expected: T, msg?: string): void {
  if (!Object.is(actual, expected)) {
    throw new Error(
      msg ?? `Expected ${String(expected)}, received ${String(actual)}`
    );
  }
}

export function assertIncludes(
  actual: string,
  expected: string,
  msg?: string
): void {
  if (!actual.includes(expected)) {
    throw new Error(msg ?? `Expected "${actual}" to include "${expected}"`);
  }
}

export function delay(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

export function toErrorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

export function isSkipTestError(error: unknown): error is SkipTestError {
  return error instanceof SkipTestError;
}

export function skipTest(reason: string): never {
  throw new SkipTestError(reason);
}

function matcherDescription(matcher?: RegExp | string): string {
  if (!matcher) {
    return 'any error';
  }

  return matcher instanceof RegExp ? matcher.toString() : matcher;
}

function matchesErrorMessage(
  error: unknown,
  matcher?: RegExp | string
): boolean {
  if (!matcher) {
    return true;
  }

  const message = toErrorMessage(error);
  return matcher instanceof RegExp
    ? matcher.test(message)
    : message.includes(matcher);
}

export function expectThrow(fn: () => void, matcher?: RegExp | string): Error {
  try {
    fn();
  } catch (error) {
    assert(
      matchesErrorMessage(error, matcher),
      `Expected error matching ${matcherDescription(
        matcher
      )}, received ${toErrorMessage(error)}`
    );
    return error instanceof Error ? error : new Error(String(error));
  }

  throw new Error('Expected function to throw');
}

export async function expectReject(
  promiseFactory: () => Promise<unknown>,
  matcher?: RegExp | string
): Promise<Error> {
  try {
    await promiseFactory();
  } catch (error) {
    assert(
      matchesErrorMessage(error, matcher),
      `Expected rejection matching ${matcherDescription(
        matcher
      )}, received ${toErrorMessage(error)}`
    );
    return error instanceof Error ? error : new Error(String(error));
  }

  throw new Error('Expected promise to reject');
}

export function getLoopbackAddress(type: SocketType): string {
  return type === 'udp4' ? '127.0.0.1' : '::1';
}

export function getWildcardAddress(type: SocketType): string {
  return type === 'udp4' ? '0.0.0.0' : '::';
}

export function createSocket(
  type: SocketType,
  options: Partial<Options> = {}
): Socket {
  return dgram.createSocket({ type, ...options });
}

export async function bindSocket(
  socket: Socket,
  type: SocketType,
  port = 0,
  address = getLoopbackAddress(type)
): Promise<void> {
  await new Promise<void>((resolve, reject) => {
    socket.bind(port, address, (error?: Error) => {
      if (error) {
        reject(error);
        return;
      }
      resolve();
    });
  });
}

export async function createBoundSocket(
  type: SocketType = 'udp4',
  port = 0,
  address = getLoopbackAddress(type),
  options: Partial<Options> = {}
): Promise<Socket> {
  const socket = createSocket(type, options);

  try {
    await bindSocket(socket, type, port, address);
    return socket;
  } catch (error) {
    closeSocket(socket);
    throw error;
  }
}

export function closeSocket(socket?: Socket): void {
  socket?.close();
}

export function closeSockets(...sockets: Array<Socket | undefined>): void {
  sockets.forEach((socket) => closeSocket(socket));
}

export function getSocketId(socket: Socket): number {
  const id = Reflect.get(socket, '_id');
  assert(typeof id === 'number' && id > 0, 'Expected an internal socket id');
  return id;
}

export function toRemoteInfo(value: unknown): RemoteInfo {
  assert(typeof value === 'object' && value !== null, 'Expected remote info');

  const address = Reflect.get(value, 'address');
  const port = Reflect.get(value, 'port');
  const family = Reflect.get(value, 'family');

  assert(typeof address === 'string', 'Expected remote address');
  assert(typeof port === 'number', 'Expected remote port');
  assert(typeof family === 'string', 'Expected remote family');

  return { address, port, family };
}

export function waitForEvent(
  socket: Socket,
  event: string,
  timeout = DEFAULT_TIMEOUT_MS
): Promise<unknown[]> {
  return new Promise((resolve, reject) => {
    let timer: ReturnType<typeof setTimeout> | undefined;

    const handler = (...args: unknown[]) => {
      cleanup();
      resolve(args);
    };

    const cleanup = () => {
      if (timer) {
        clearTimeout(timer);
      }
      socket.off(event, handler);
    };

    timer = setTimeout(() => {
      cleanup();
      reject(new Error(`Timed out waiting for "${event}" event`));
    }, timeout);

    socket.once(event, handler);
  });
}

export async function waitForMessage(
  socket: Socket,
  timeout = DEFAULT_TIMEOUT_MS
): Promise<ReceivedMessage> {
  const [message, info] = await waitForEvent(socket, 'message', timeout);
  assert(Buffer.isBuffer(message), 'Expected a Buffer message');

  return {
    message,
    info: toRemoteInfo(info),
  };
}

export function waitForMessages(
  socket: Socket,
  count: number,
  timeout = DEFAULT_TIMEOUT_MS
): Promise<ReceivedMessage[]> {
  return new Promise((resolve, reject) => {
    const messages: ReceivedMessage[] = [];
    let timer: ReturnType<typeof setTimeout> | undefined;

    const onMessage = (message: Buffer, info: unknown) => {
      try {
        messages.push({
          message,
          info: toRemoteInfo(info),
        });
      } catch (error) {
        cleanup();
        reject(error);
        return;
      }

      if (messages.length === count) {
        cleanup();
        resolve(messages);
      }
    };

    const onError = (error: Error) => {
      cleanup();
      reject(error);
    };

    const cleanup = () => {
      if (timer) {
        clearTimeout(timer);
      }
      socket.off('message', onMessage);
      socket.off('error', onError);
    };

    timer = setTimeout(() => {
      cleanup();
      reject(
        new Error(
          `Timed out after receiving ${messages.length}/${count} messages`
        )
      );
    }, timeout);

    socket.on('message', onMessage);
    socket.on('error', onError);
  });
}

export function expectNoMessage(
  socket: Socket,
  timeout = DEFAULT_TIMEOUT_MS
): Promise<void> {
  return new Promise((resolve, reject) => {
    let timer: ReturnType<typeof setTimeout> | undefined;

    const onMessage = (message: Buffer) => {
      cleanup();
      reject(
        new Error(
          `Expected no message, received ${message.length} bytes instead`
        )
      );
    };

    const onError = (error: Error) => {
      cleanup();
      reject(error);
    };

    const cleanup = () => {
      if (timer) {
        clearTimeout(timer);
      }
      socket.off('message', onMessage);
      socket.off('error', onError);
    };

    timer = setTimeout(() => {
      cleanup();
      resolve();
    }, timeout);

    socket.on('message', onMessage);
    socket.on('error', onError);
  });
}

export function createPayload(size: number, fill = 0x61): Buffer {
  return Buffer.alloc(size, fill);
}

export async function sendAsync(
  socket: Socket,
  data: string | Buffer,
  port: number,
  address: string,
  offset = 0,
  length?: number
): Promise<void> {
  const payloadLength =
    length ??
    (typeof data === 'string' ? Buffer.byteLength(data) : data.length);

  await new Promise<void>((resolve, reject) => {
    socket.send(data, offset, payloadLength, port, address, (error?: Error) => {
      if (error) {
        reject(error);
        return;
      }
      resolve();
    });
  });
}

export async function reservePort(
  type: SocketType = 'udp4',
  address = getLoopbackAddress(type)
): Promise<number> {
  const socket = await createBoundSocket(type, 0, address);
  const { port } = socket.address();
  closeSocket(socket);
  await delay(PORT_RELEASE_DELAY_MS);
  return port;
}

export function skipOnPlatform(
  platforms: ReadonlyArray<string>,
  reason: string
): () => string | undefined {
  return () => (platforms.includes(Platform.OS) ? reason : undefined);
}
