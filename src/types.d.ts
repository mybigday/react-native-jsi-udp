function datagram_create(type: string): number;

function datagram_startWorker(
  fd: number,
  listener: ({
    type: 'message'|'error'|'close',
    family?: string,
    address?: string,
    port?: number,
    data?: string,
    error?: Error
  }) => void
): void;

function datagram_bind(fd: number, host: string, port: number): void;

function datagram_close(fd: number): void;

function datagram_setOpt(fd: number, key: string, value1: number|string, value2?: number|string): void;

function datagram_getOpt(fd: number, key: string): number;

function datagram_send(fd: number, host: string, port: number, data: ArrayBuffer): void;

function datagram_getSockName(fd: number): {
  family: 'IPv4' | 'IPv6',
  address: string,
  port: number
};
