"""
UDP echo latency test.

Usage:
  python test_latency.py <device-ip> [rounds]

Requires the "Latency server" test case running in the example app
(echo server on port 12345). Tap "Run Suite" on the Latency server
card in the test runner, then run this script within 60 seconds.

Example:
  python test_latency.py 192.168.1.100
  python test_latency.py 192.168.1.100 200
"""

import socket
import sys
import time
import struct
import statistics

if len(sys.argv) < 2:
    print(__doc__.strip())
    sys.exit(1)

target = sys.argv[1]
rounds = 100 if len(sys.argv) < 3 else int(sys.argv[2])

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('0.0.0.0', 0))

delays = []
for i in range(rounds):
    start_time = time.time()
    s.sendto(struct.pack('d', start_time), (target, 12345))
    data, addr = s.recvfrom(8)
    receive = struct.unpack('d', data)[0]
    assert receive == start_time, f"ERR: receive != start_time, receive: {receive}, start_time: {start_time}"
    end_time = time.time()
    delays.append((end_time - start_time) * 1000)

print(f"mean delay: {(sum(delays) / rounds):.3f}ms")
print(f"min delay: {min(delays):.3f}ms")
print(f"max delay: {max(delays):.3f}ms")
print(f"std delay: {statistics.stdev(delays):.3f}ms")
