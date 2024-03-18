/*
 * hexdump.c
 *
 *  Created on: 11 Mar 2024
 *      Author: pete
 */

#include <Arduino.h>
#include "hexdump.h"

void hexdump(
    Stream& out_stream,
    const uint8_t* const data,
    const uint32_t byteCount,
    const bool header,
    const char* const message,
    const uint8_t linefeeds ,
    const uint16_t offset)
{
    // Actual memory address where data points to is unknown so first byte is shown as address 0x0000.
    // Optionally an offset for the address labeling can be specified. This imitates as if memory starts
    // at the offset. The offset feature can be useful when having to compare a hexdump with another hexdump.

    if (header)
    {
        if (message == nullptr || strlen(message) == 0)
        {
            out_stream.printf("Memory hexdump - 0x%X (%u) bytes\n", byteCount, byteCount);
        }
        else
        {
            out_stream.printf("Memory hexdump %s - 0x%X (%u) bytes\n", message, byteCount, byteCount);
        }
    }

    if (byteCount == 0)
    {
        out_stream.printf("Byte count is 0 (hexdump).\r\n");
        for (uint8_t i = 0; i < linefeeds; ++i)
        {
            out_stream.printf("\r\n");
        }
        return;
    }

    if (data == nullptr)
    {
        out_stream.printf("Data nullpointer (hexdump).\r\n");
        for (uint8_t i = 0; i < linefeeds; ++i)
        {
            out_stream.printf("\r\n");
        }
        return;
    }

    if (header)
    {
        if (offset > 0)
        {
            out_stream.printf("Showing addresses with simulated offset 0x%02X (%u).\n", offset, offset);
        }
        out_stream.printf("\r\n");

        // Output header
        out_stream.print("     ");
        for (uint8_t i = 0; i < 16; ++i)
        {
            if (i == 8)
            {
                out_stream.print("  ");
            }
            out_stream.printf(" %02X", i);
        }
        out_stream.print("\n     ");
        for (uint8_t i = 0; i < 16; ++i)
        {
            if (i == 8)
            {
                out_stream.print("  ");
            }
            out_stream.printf(" --");
        }
        out_stream.printf("\r\n");
    }

    // Output data

    uint16_t lineNr = 0;
    uint16_t address = offset;
    if ((address & 0xF) > 0)
    {
        ++lineNr;
        // Output first adreslabel and indent
        out_stream.printf("%04X:", address & 0xFFF0);
        for (uint8_t i = 0; i < (offset & 0xF); ++i)
        {
            if (i == 8 && (offset & 0xF) >= 8)
            {
                out_stream.print("  ");
            }
            out_stream.print("   ");
        }
    }

    // Output byte values
    for (uint16_t i = 0; i < byteCount; ++i)
    {
        // stream.printf("[i=%u, address=0x%04X, lineNr=%u]", i, address, lineNr); //debug
        if ((address & 0xF) == 0)
        {
            ++lineNr;
            if (lineNr > 1)
            {
                out_stream.printf("\r\n");
            }
            // Write address label
            out_stream.printf("%04X:", address & 0xFFF0);
        }
        if ((address & 0xF) ==  8)
        {
            out_stream.print(" -");
        }
        out_stream.printf(" %02X", data[i]);
        ++address;
    }
    out_stream.printf("\r\n");
    for (uint8_t i = 0; i < linefeeds; ++i)
    {
        out_stream.printf("\r\n");
    }
}

