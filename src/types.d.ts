declare function datagram_create(type: string): number;

declare interface datagram_event {
  type: 'message' | 'error' | 'close';
  family?: string;
  address?: string;
  port?: number;
  data?: ArrayBuffer;
  error?: Error;
}

declare function datagram_startWorker(
  fd: number,
  listener: (event: datagram_event) => void
): void;

declare function datagram_bind(
  fd: number,
  type: 'udp4' | 'udp6',
  host: string,
  port: number
): void;

declare function datagram_close(fd: number): void;

declare function datagram_setOpt(
  fd: number,
  key: string,
  value1: number | string,
  value2?: number | string
): void;

declare function datagram_getOpt(fd: number, key: string): number;

declare function datagram_send(
  fd: number,
  type: 'udp4' | 'udp6',
  host: string,
  port: number,
  data: ArrayBuffer
): void;

declare function datagram_getSockName(fd: number): {
  family: 'IPv4' | 'IPv6';
  address: string;
  port: number;
};
