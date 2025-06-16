import socket
import sys
import time
import struct
import statistics

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
