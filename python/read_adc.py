import sys
import crc

from datetime import datetime, timedelta

b_len = 512 * 6 * 2

with open(sys.argv[1], "rb") as f:
    hdr = f.read(512)
    print(hdr.decode('utf-8'))

    data_block = f.read(b_len)
    while data_block != b"":
        print('data block read len', len(data_block))

        sample = 0
        # Do stuff with data_block.

        sec = data_block[0] & 0x3f
        minute = ((data_block[0] & 0xc0) >> 6) + ((data_block[1] & 0x0f) << 2)
        hour = ((data_block[1] & 0xf0) >> 4) + ((data_block[2] & 0x1) << 4)

        day = ((data_block[2] & 0x3e) >> 1)
        month = ((data_block[2] & 0xc0) >> 6) + ((data_block[3] & 0x3) << 2)
        year = ((data_block[3] & 0xfc) >> 2) + 2000
        print(len(data_block), data_block[0], data_block[1], data_block[2], "ymd", year, month, day, "hms", hour, minute, sec)
        dt = datetime(year, month, day, hour, minute, sec)
        print(dt)

        i = 0
        while i < (b_len - 6):
            sample_data = data_block[i+6:i+12]
            #print('sample_len', len(sample_data))
            cnt = int.from_bytes(sample_data[0:1], byteorder='big', signed=False)
            data = int.from_bytes(sample_data[1:4], byteorder='big', signed=True)
            c = int.from_bytes(sample_data[4:6], byteorder='big', signed=False)

            check_data = bytearray([1, sample_data[1], sample_data[2], sample_data[3]])
            ck_calc = int(crc.crc_poly(check_data, 16, 0x1021, crc=0xFFFF)) # CRC = CCITT initial value of 0xffff, x16 + x12 + x5 + 1
            if c == ck_calc:
                print(dt, i, sample, "count", cnt, "data", data, "crc", c, ck_calc)
            else:
                print(dt, i, sample, "count", cnt, "data", data, "crc", c, ck_calc, "crc failed")

            dt = dt + timedelta(seconds=1)
            i += 6
            sample += 1

        # read next data_block
        data_block = f.read(b_len)
