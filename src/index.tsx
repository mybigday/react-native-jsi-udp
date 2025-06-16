import { NativeModules, Platform } from 'react-native';
import { EventEmitter } from 'tseep';
import { Buffer } from 'buffer';

const LINKING_ERROR =
  `The package 'react-native-jsi-udp' doesn't seem to be linked. Make sure: \n\n` +
  Platform.select({ ios: "- You have run 'pod install'\n", default: '' }) +
  '- You rebuilt the app after installing the package\n' +
  '- You are not using Expo Go\n';

const JsiUdp = NativeModules.JsiUdp
  ? NativeModules.JsiUdp
  : new Proxy(
      {},
      {
        get() {
          throw new Error(LINKING_ERROR);
        },
      }
    );

export interface Options {
  type: 'udp4' | 'udp6';
  reuseAddr?: boolean;
  reusePort?: boolean;
}

export enum State {
  UNBOUND = 0,
  BOUND = 1,
  CLOSED = 2,
}

export type Callback = (...args: any[]) => void;

export class Socket extends EventEmitter {
  private state: State;
  private type: 4 | 6;
  private _id: number;
  private reuseAddr: boolean;
  private reusePort: boolean;
  private _receiving: boolean = false;
  private _timeoutId: ReturnType<typeof setTimeout> | null = null;

  constructor(options: Options, callback?: Callback) {
    super();
    if (typeof datagram_create !== 'function') {
      JsiUdp.install();
    }
    this.state = State.UNBOUND;
    this.type = options.type === 'udp4' ? 4 : 6;
    this.reuseAddr = options.reuseAddr ?? false;
    this.reusePort = options.reusePort ?? false;
    this._id = datagram_create(this.type);
    if (callback) this.on('message', callback);
  }

  private startReceiving() {
    if (this._receiving || this.state !== State.BOUND) return;

    this._receiving = true;
    this._receive();
  }

  private stopReceiving() {
    this._receiving = false;
    if (this._timeoutId) {
      clearTimeout(this._timeoutId);
      this._timeoutId = null;
    }
  }

  private _receive = async () => {
    if (!this._receiving) return;

    try {
      const result = await datagram_receive(this._id);

      if (result?.type === 'message') {
        this.emit('message', Buffer.from(result.data!), {
          address: result.address,
          port: result.port,
          family: result.family,
        });
      } else if (result?.type === 'error') {
        this.emit('error', result.error);
      }
    } catch (e) {
      this.emit('error', e);
    }

    // Schedule the next receive operation
    this._timeoutId = setTimeout(this._receive, 0);
  };

  bind(port?: number, address?: string | Callback, callback?: Callback) {
    if (this.state !== State.UNBOUND) {
      throw new Error('Socket is already bound');
    }
    if (typeof address === 'function') {
      callback = address;
      address = undefined;
    }
    if (callback) this.once('listening', callback);
    const defaultAddr = this.type === 4 ? '0.0.0.0' : '::1';
    try {
      datagram_setOpt(
        this._id,
        dgc_SOL_SOCKET,
        dgc_SO_REUSEADDR,
        this.reuseAddr ? 1 : 0
      );
      datagram_setOpt(
        this._id,
        dgc_SOL_SOCKET,
        dgc_SO_REUSEPORT,
        this.reusePort ? 1 : 0
      );
      datagram_bind(this._id, this.type, address ?? defaultAddr, port ?? 0);
      this.state = State.BOUND;
      this.emit('listening');
      this.startReceiving();
    } catch (e) {
      if (callback) callback(e as Error);
      else this.emit('error', e);
    }
  }

  send(
    data: string | Buffer,
    offset: number | undefined,
    length: number | undefined,
    port: number,
    address: string,
    callback?: (e?: Error) => void
  ) {
    let buf: Buffer;
    if (typeof data === 'string') {
      buf = Buffer.from(data);
    } else {
      buf = data;
    }
    buf = buf.slice(offset ?? 0, length ?? buf.length);
    try {
      datagram_send(
        this._id,
        this.type,
        address,
        port,
        buf.buffer as ArrayBuffer
      );
      callback?.();
    } catch (e) {
      if (callback) callback(e as Error);
      else this.emit('error', e);
    }
  }

  close(callback?: Callback) {
    if (this.state === State.CLOSED) {
      return;
    }
    if (callback) this.once('close', callback!);
    this.stopReceiving();
    datagram_close(this._id);
    this.state = State.CLOSED;
    this.emit('close');
  }

  address() {
    return datagram_getSockName(this._id, this.type);
  }

  setBroadcast(flag: boolean) {
    datagram_setOpt(this._id, dgc_SOL_SOCKET, dgc_SO_BROADCAST, flag ? 1 : 0);
  }

  getRecvBufferSize() {
    return datagram_getOpt(this._id, dgc_SOL_SOCKET, dgc_SO_RCVBUF);
  }

  setRecvBufferSize(size: number) {
    datagram_setOpt(this._id, dgc_SOL_SOCKET, dgc_SO_RCVBUF, size);
  }

  getSendBufferSize() {
    return datagram_getOpt(this._id, dgc_SOL_SOCKET, dgc_SO_SNDBUF);
  }

  setSendBufferSize(size: number) {
    datagram_setOpt(this._id, dgc_SOL_SOCKET, dgc_SO_SNDBUF, size);
  }

  addMembership(multicastAddress: string, multicastInterface?: string) {
    datagram_setOpt(
      this._id,
      this.type === 4 ? dgc_IPPROTO_IP : dgc_IPPROTO_IPV6,
      dgc_IP_ADD_MEMBERSHIP,
      multicastAddress,
      multicastInterface
    );
  }

  dropMembership(multicastAddress: string, multicastInterface?: string) {
    datagram_setOpt(
      this._id,
      this.type === 4 ? dgc_IPPROTO_IP : dgc_IPPROTO_IPV6,
      dgc_IP_DROP_MEMBERSHIP,
      multicastAddress,
      multicastInterface
    );
  }

  setMulticastTTL(ttl: number) {
    datagram_setOpt(
      this._id,
      this.type === 4 ? dgc_IPPROTO_IP : dgc_IPPROTO_IPV6,
      dgc_IP_MULTICAST_TTL,
      ttl
    );
  }

  setMulticastLoopback(flag: boolean) {
    datagram_setOpt(
      this._id,
      this.type === 4 ? dgc_IPPROTO_IP : dgc_IPPROTO_IPV6,
      dgc_IP_MULTICAST_LOOP,
      flag ? 1 : 0
    );
  }

  setTTL(ttl: number) {
    datagram_setOpt(
      this._id,
      this.type === 4 ? dgc_IPPROTO_IP : dgc_IPPROTO_IPV6,
      dgc_IP_TTL,
      ttl
    );
  }

  ref() {
    return this; // Not implemented
  }

  unref() {
    return this; // Not implemented
  }
}

export function createSocket(options: Options | 'udp4' | 'udp6') {
  if (typeof options === 'string') {
    options = { type: options };
  }
  return new Socket(options);
}

export default {
  createSocket,
  Socket,
};
