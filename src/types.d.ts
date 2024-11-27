declare function datagram_create(type: 4 | 6): number;

declare interface datagram_event {
  type: 'message' | 'error' | 'close';
  family?: string;
  address?: string;
  port?: number;
  data?: ArrayBuffer;
  error?: Error;
}

declare function datagram_bind(
  id: number,
  type: 4 | 6,
  host: string,
  port: number
): void;

declare function datagram_close(id: number): void;

declare function datagram_setOpt(
  id: number,
  level: number,
  opt: number,
  value1: number | string,
  value2?: number | string
): void;

declare function datagram_getOpt(
  id: number,
  level: number,
  opt: number
): number;

declare function datagram_send(
  id: number,
  type: 4 | 6,
  host: string,
  port: number,
  data: ArrayBuffer
): void;

declare function datagram_getSockName(
  id: number,
  type: 4 | 6
): {
  family: 'IPv4' | 'IPv6';
  address: string;
  port: number;
};

declare var dgc_SOL_SOCKET: number;
declare var dgc_IPPROTO_IP: number;
declare var dgc_IPPROTO_IPV6: number;
declare var dgc_SO_REUSEADDR: number;
declare var dgc_SO_REUSEPORT: number;
declare var dgc_SO_BROADCAST: number;
declare var dgc_SO_RCVBUF: number;
declare var dgc_SO_SNDBUF: number;
declare var dgc_IP_MULTICAST_TTL: number;
declare var dgc_IP_MULTICAST_LOOP: number;
declare var dgc_IP_ADD_MEMBERSHIP: number;
declare var dgc_IP_DROP_MEMBERSHIP: number;
declare var dgc_IP_TTL: number;
declare var datagram_callbacks: {
  [key: string]: (event: datagram_event) => void;
};
