import sys
import crc

from datetime import datetime, timedelta
from netCDF4 import num2date, date2num
from netCDF4 import Dataset
import numpy as np

import re

bytes_per_block = 512 * 6 * 2

file = sys.argv[1]
with open(file, "rb") as f:

    hdr = f.read(512).decode('utf-8')

    print(hdr.rstrip('\x00'))

    burst_match = re.search("no. burst = (\\d*)", hdr)
    no_burst = int(burst_match.group(1))
    print('burst match', burst_match.groups(), ' no. burst', no_burst)

    bytes_match = re.search("bytes per sample = (\\d*)", hdr)
    bytes_per_sample = int(bytes_match.group(1))
    print('bytes per sample', bytes_match.groups(), ' bytes', bytes_per_sample)

    sample_rate_match = re.search("sample rate = (\\d*) \\(ms\\)", hdr)
    sample_rate = int(sample_rate_match.group(1))
    print('sample rate', sample_rate_match.groups(), ' sample rate', sample_rate)

    adc_match = re.compile("ADC Channel  (\\d*) = 0x")
    for match in adc_match.finditer(hdr):
        print('adc_match', match.groups())
        channels = int(match.group(1))

    print()

    # create the netCDF file
    outputName = file + ".nc"
    print("output file : %s" % outputName)
    ncOut = Dataset(outputName, 'w', format='NETCDF4')

    # create some variables
    tDim = ncOut.createDimension("TIME")
    ncTimesOut = ncOut.createVariable("TIME", "d", ("TIME",), zlib=True)
    ncTimesOut.long_name = "time"
    ncTimesOut.units = "days since 1950-01-01 00:00:00 UTC"
    ncTimesOut.calendar = "gregorian"
    ncTimesOut.axis = "T"

    # add variables
    var_raw = ncOut.createVariable("RAW", "f4", ("TIME",), fill_value=np.nan, zlib=True)
    var_ch = ncOut.createVariable("CHANNEL", "i4", ("TIME",), fill_value=-1, zlib=True)
    var_burst = ncOut.createVariable("BURST", "i4", ("TIME",), fill_value=-1, zlib=True)
    
    sample_data = f.read(bytes_per_sample)
    sample = 0
    bytes_read = len(sample_data)
    block_sample = 0

    ch = 0
    burst = 0

    while sample_data != b"":

        print('sample_data', sample_data.hex())

        cnt = int.from_bytes(sample_data[0:1], byteorder='big', signed=False)
        data = int.from_bytes(sample_data[1:4], byteorder='big', signed=True)
        c = int.from_bytes(sample_data[4:6], byteorder='big', signed=False)

        check_data = sample_data[0:4]
        ck_calc = int(crc.crc_poly(check_data, 16, 0x1021, crc=0xFFFF)) # CRC = CCITT initial value of 0xffff, x16 + x12 + x5 + 1

        print(sample, "count", cnt, "data", data, "crc", c, ck_calc, burst)
        if c == ck_calc:
            print(dt, sample, "count", cnt, "data", data, "crc", c, ck_calc, ch, burst)
            var_raw[sample] = data
            var_ch[sample] = ch
            var_burst[sample] = burst

            dt += timedelta(seconds=50/1000)

            sample += 1
            ch += 1
            if ch > channels:
                ch = 0
                burst += 1
                if burst >= no_burst:
                    burst = 0
                    burst_time += timedelta(seconds=sample_rate/1000)
                    dt = burst_time

        else:
            sec = sample_data[0] & 0x3f
            minute = ((sample_data[0] & 0xc0) >> 6) + ((sample_data[1] & 0x0f) << 2)
            hour = ((sample_data[1] & 0xf0) >> 4) + ((sample_data[2] & 0x1) << 4)

            day = ((sample_data[2] & 0x3e) >> 1)
            month = ((sample_data[2] & 0xc0) >> 6) + ((sample_data[3] & 0x3) << 2)
            year = ((sample_data[3] & 0xfc) >> 2) + 2000
            print('len', len(sample_data), 'timstamp', sample_data[0], sample_data[1], sample_data[2], "ymd", year, month, day, "hms", hour, minute, sec)
            hdr_time = datetime(year, month, day, hour, minute, sec)
            
            print('hdr time', hdr_time)

            burst_time = hdr_time
            dt = hdr_time

            ch = 0
            burst = 0

        # read next sample_data
        sample_data = f.read(bytes_per_sample)
        bytes_read += len(sample_data)

    ncTimeFormat = "%Y-%m-%dT%H:%M:%SZ"

#    ncOut.setncattr("time_coverage_start", num2date(ncTimesOut[0], units=ncTimesOut.units, calendar=ncTimesOut.calendar).strftime(ncTimeFormat))
#    ncOut.setncattr("time_coverage_end", num2date(ncTimesOut[-1], units=ncTimesOut.units, calendar=ncTimesOut.calendar).strftime(ncTimeFormat))

    # add creating and history entry
    ncOut.setncattr("date_created", datetime.now().strftime(ncTimeFormat))
    ncOut.setncattr("history", datetime.now().strftime("%Y-%m-%d") + " created from file " + file)

    ncOut.close()
