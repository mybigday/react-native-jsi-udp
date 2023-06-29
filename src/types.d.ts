declare function datagram_create(type: 4 | 6): number;

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
  type: 4 | 6,
  host: string,
  port: number
): void;

declare function datagram_close(fd: number): void;

declare function datagram_setOpt(
  fd: number,
  level: number,
  opt: number,
  value1: number | string,
  value2?: number | string
): void;

declare function datagram_getOpt(
  fd: number,
  level: number,
  opt: number
): number;

declare function datagram_send(
  fd: number,
  type: 4 | 6,
  host: string,
  port: number,
  data: ArrayBuffer
): void;

declare function datagram_getSockName(
  fd: number,
  type: 4 | 6
): {
  family: 'IPv4' | 'IPv6';
  address: string;
  port: number;
};
