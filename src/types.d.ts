export function datagram_create(type: string): number;

export interface datagram_event {
  type: 'message' | 'error' | 'close';
  family?: string;
  address?: string;
  port?: number;
  data?: string;
  error?: Error;
}

export function datagram_startWorker(
  fd: number,
  listener: (event: datagram_event) => void
): void;

export function datagram_bind(fd: number, host: string, port: number): void;

export function datagram_close(fd: number): void;

export function datagram_setOpt(
  fd: number,
  key: string,
  value1: number | string,
  value2?: number | string
): void;

export function datagram_getOpt(fd: number, key: string): number;

export function datagram_send(
  fd: number,
  host: string,
  port: number,
  data: ArrayBuffer
): void;

export function datagram_getSockName(fd: number): {
  family: 'IPv4' | 'IPv6';
  address: string;
  port: number;
};
