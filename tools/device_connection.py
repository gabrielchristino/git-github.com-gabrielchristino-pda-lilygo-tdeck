"""Bounded USB command transport shared by physical validation tools."""
import re
import time
import serial

def fields(line):
    return {key: int(value) if value.isdigit() else value for key, value in re.findall(r'(\w+)=([^\s]+)', line)}


class Connection:
    def __init__(self, port, log):
        self.port = serial.Serial()
        self.port.port, self.port.baudrate, self.port.timeout, self.port.write_timeout = port, 115200, .05, 2
        self.port.dtr, self.port.rts = True, False
        self.port.open()
        self.log, self.buffer = log, bytearray()
        time.sleep(.5)
        self.port.reset_input_buffer()

    def read(self):
        self.buffer.extend(self.port.read(self.port.in_waiting or 1))
        lines = []
        while b'\n' in self.buffer:
            data, _, tail = self.buffer.partition(b'\n'); self.buffer = bytearray(tail)
            line = data.decode('utf-8', errors='replace').strip('\r')
            self.log.write(line + '\n'); self.log.flush(); lines.append(line)
        return lines

    def command(self, text, expected, timeout=20):
        self.log.write('> ' + text + '\n'); self.log.flush()
        started = time.perf_counter()
        self.port.write((text + '\n').encode('utf-8'))
        lines = []
        while time.perf_counter() - started < timeout:
            received = self.read(); lines += received
            if any(expected in line for line in received):
                return '\n'.join(lines), (time.perf_counter() - started) * 1000
        raise RuntimeError('Device did not confirm: ' + text)

