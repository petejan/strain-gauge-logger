/*
 * cmd.c
 *
 *  Created on: 11 Mar 2024
 *      Author: pete
 */

#include <Arduino.h>
#include "ZeroRegs.h"
#include "main.h"
#include "time.h"
#include "adc122c04.h"
#include "timer.h"

#define SerialDEBUG SERIAL_PORT_HARDWARE

int cmd_regs(char *line);
int cmd_help(char *line);
int cmd_time(char *line);
int cmd_debug(char *line);
int cmd_sleep(char *line);
int cmd_fcorr(char *line);
int cmd_ls(char *line);
int cmd_start(char *line);
int cmd_stop(char *line);
int cmd_starttime(char *line);
int cmd_startlater(char *line);
int cmd_startdetect(char *line);
int cmd_set(char *line);
int cmd_adc(char *line);
int cmd_sample(char *line);
int cmd_pwrdelay(char *line);
int cmd_text(char *line);
int cmd_file(char *line);
int cmd_format(char *line);
int cmd_sync(char *line);
int cmd_burst(char *line);
int cmd_hdr(char *line);
#ifdef ZMODEM
int cmd_sz(char *line);
int cmd_rz(char *line);
#endif

/* commands */

typedef struct
{
  const char * cmd_name;
  int (*cmd_handler)(char *line);
  const char * cmd_help;
} CMD;

void * findCommand(char * command);

CMD cmds[] =
{
		  {"---",        NULL,         "setup commands"},
		  {"time",       cmd_time,        "display/set time <YYYY-MM-DD hh:mm:ss>"},
		  {"fcorr",      cmd_fcorr,       "display/set fcorr reg"},
		  {"adc",        cmd_adc,         "display/set adc <rate> <gain>"},
		  {"nburst",     cmd_burst,       "number of samples to burst sample"},
		  {"samplerate", cmd_sample,      "display/set sample rate"},
		  {"pwrdelay",   cmd_pwrdelay,      "display/set ADC power up delay"},

		  {"text",       cmd_text,        "display/set deployment text note"},

		  {"---",        NULL,         "start/stop commands"},
		  {"startnow",   cmd_start,       "start sampling now"},
		  {"starttime",  cmd_starttime,   "set/display start time"},
		  {"startlater", cmd_startlater,  "start sampling at starttime"},
		  {"startdet",   cmd_startdetect, "start sampling when connected to load cell"},
		  {"stop",       cmd_stop,        "start stop sampling"},

		  {"set",        cmd_set,         "display settings"},

		  {"---",        NULL,         "debug commands"},
		  {"sleep",      cmd_sleep,       "set can sleep"},
		  {"debug",      cmd_debug,       "set debug level"},
		  {"regs",       cmd_regs,        "display all SAMD21 regs"},

		  {"---",        NULL,         "file commands"},
		  {"ls",         cmd_ls,          "display card file list"},
		  {"file",       cmd_file,        "display/set file name %Y-%m-%dT%H%M.BIN"},
		  {"format",     cmd_format,      "format card 'format yes' to format"},
		  {"sync",       cmd_sync,  	  "sync file"},
		  {"hdr",        cmd_hdr,  		  "print file header"},
#ifdef ZMODEM
		  {"sz",         cmd_sz,  	      "send file"},
		  {"rz",         cmd_rz,  	      "receive file"},
#endif

		  {"---",        NULL,         "help"},
		  {"help",       cmd_help,        "command help, this message"}
};


#define num_commands sizeof(cmds)/sizeof(cmds[0])

void * findCommand(char * command)
{
	//SerialDEBUG.println("find command");

	if(!command || strlen(command) == 0)
	{
		return NULL;
	}

	for(int i=0; i<num_commands; i++)
	{
		//Search by name
		if(!strcmp(command, cmds[i].cmd_name))
		{
			return &cmds[i];
		}
	}

	return NULL;
}

int executeCommand(char *line)
{
	char *tok = strtok(line, " ");

	CMD * c = (CMD *)findCommand(tok);

	if (c)
	{
		tok = strtok(NULL, " ");

		return (*c->cmd_handler)(tok);
	}

	SerialDEBUG.printf("Invalid command. Type \"help\" for more.\r\n");

	return 0;
}

int cmd_help(char *line)
{
	SerialDEBUG.printf("help command:");

	for(int i=0; i<num_commands; i++)
	{
		SerialDEBUG.printf("%-20s : %s\r\n", cmds[i].cmd_name, cmds[i].cmd_help);
	}

	return 0;
}

int cmd_regs(char *line)
{
	ZeroRegOptions opts = { SerialDEBUG, false };
	printZeroRegs(opts);

	//opts.showDisabled = true;
	//printZeroRegPORT(opts);
	//opts.showDisabled = true;
	//printZeroRegDMAC(opts);

	return 0;
}

int cmd_adc(char *line)
{

	if (line)
	{
		int new_ch, new_reg0, new_reg1;

		sscanf(line, "%d", &new_ch);
		line = strtok(NULL, " ");

		if (line)
		{
		    sscanf(line, "%x", &new_reg0);
			if (line)
			{
			    sscanf(line, "%x", &new_reg1);

				SerialDEBUG.printf("new channel %d reg0 0x%02x reg1 0x%02x\r\n", new_ch, new_reg0, new_reg1);
				SerialDEBUG.printf("*** not implemented yet ***\r\n");
			}
		}
	}

	// for each channel
	int ad_ch = 0;
	while (channel_config_reg0[ad_ch] != 0xff)
	{
		SerialDEBUG.printf("channel %d reg0 0x%02x reg1 0x%02x\r\n", ad_ch, channel_config_reg0[ad_ch], channel_config_reg1[ad_ch]);
		uint8_t reg0_val = channel_config_reg0[ad_ch];
		SerialDEBUG.printf(" MUX : %d\r\n", (reg0_val & 0xf0) >> 4);
		SerialDEBUG.printf(" Gain : %d\r\n", (reg0_val & 0x0d) >> 1);
		SerialDEBUG.printf(" PGA_BYPASS : %d\r\n", (reg0_val & 0x01));

		uint8_t reg1_val = channel_config_reg1[ad_ch];

		SerialDEBUG.printf(" Mode : %s\r\n", reg1_val & ADS122C04_OP_MODE_TURBO ? "Turbo" : "Normal");
		SerialDEBUG.printf(" CM : %s\r\n", reg1_val & ADS122C04_CONVERSION_MODE_CONTINUOUS ? "Continuous" : "Single");
		SerialDEBUG.printf(" VREF : ");
		switch ((reg1_val & 0x6) >> 1)
		{
			case 0:
				SerialDEBUG.printf("Internal\r\n");
				break;
			case 1:
				SerialDEBUG.printf("External\r\n");
				break;
			case 2:
				SerialDEBUG.printf("AVdd/AVss\r\n");
				break;
		}
		// TODO: update for turbo mode rates (x2)
		const int data_rate[] = {20, 45, 90, 175, 330, 600, 1000};

		int dr = data_rate[(reg1_val >> 5)];
		if (reg1_val & ADS122C04_OP_MODE_TURBO)
		{
			dr = dr * 2;
		}
		SerialDEBUG.printf(" data rate : %d SPS\r\n", dr);

		SerialDEBUG.printf(" Temp Sensor : %d\r\n", reg1_val & ADS122C04_TEMP_SENSOR_ON);

		ad_ch++;
	}
	int new_reg2 = adc_reg2.read();
	SerialDEBUG.printf("adc_reg2 : 0x%x\r\n", new_reg2);

	int new_reg3 = adc_reg3.read();
	SerialDEBUG.printf("adc_reg3 : 0x%x\r\n", new_reg3);


	return 0;
}

int cmd_start(char *line)
{
	run_mode.write(RUN_MODE_RUN);

	SerialDEBUG.printf("set run mode\r\n");

	return 0;
}

int cmd_startdetect(char *line)
{
	run_mode.write(RUN_MODE_DETECT);

	SerialDEBUG.printf("set wait for connect load cell mode\r\n");

	return 0;
}

int cmd_stop(char *line)
{
	run_mode.write(RUN_MODE_STOP);

	SerialDEBUG.printf("set stop mode\r\n");

	return 0;
}

int cmd_startlater(char *line)
{
	time_t start_time = datetime_start.read();
	tm tm_elements;
	unsigned long delta;

	gmtime_r(&start_time, &tm_elements);

	run_mode.write(RUN_MODE_START_LATER);

	rtc.disableAlarm();

	rtc.setAlarmTime(tm_elements.tm_hour, tm_elements.tm_min, tm_elements.tm_sec);
	rtc.setAlarmDate(tm_elements.tm_mday, tm_elements.tm_mon + 1, tm_elements.tm_year + 1900 - 2000);

	rtc.enableAlarm(rtc.MATCH_YYMMDDHHMMSS);

	SerialDEBUG.printf("set start_later mode\r\n");

	// output time when its doing to start

	now = rtc.getEpoch();
	delta = start_time - now;

	SerialDEBUG.printf("start in %d (seconds) %d (hours) %d (days)\r\n", delta, delta/3600, delta/3600/24);

	return 0;
}

int cmd_starttime(char *line)
{
	time_t start_time;
	tm tm_elements;

	if (line)
	{
		int yr, mnth, d, h, m, s;

		sscanf( line, "%4d-%2d-%2d", &yr, &mnth, &d);

		line = strtok(NULL, " ");

		sscanf( line, "%2d:%2d:%2d", &h, &m, &s);

		SerialDEBUG.printf("Set start time : %d-%02d-%02d %02d:%02d:%02d\r\n", yr, mnth, d, h, m, s);

		tm_elements.tm_year = yr - 1900;
		tm_elements.tm_mon = mnth - 1;
		tm_elements.tm_mday = d;
		tm_elements.tm_hour = h;
		tm_elements.tm_min = m;
		tm_elements.tm_sec = s;

		start_time = mktime(&tm_elements);

		datetime_start.write(start_time);
	}

	start_time = datetime_start.read();

	gmtime_r(&start_time, &tm_elements);

	SerialDEBUG.printf("Start time : ");
	print_date(&tm_elements);
	SerialDEBUG.printf("\r\n");

	return 0;
}

time_t last_set_time;

int cmd_time(char *line)
{
	struct tm t;
	time_t now, set_time;

	if (line)
	{
		SerialDEBUG.printf("args : %s ", line);

		int yr, mnth, d, h, m, s;

		sscanf( line, "%4d-%2d-%2d", &yr, &mnth, &d);

		line = strtok(NULL, " ");

		SerialDEBUG.printf(" %line\r\n", line);

		sscanf( line, "%2d:%2d:%2d", &h, &m, &s);

		SerialDEBUG.printf("Set time : %d-%02d-%02d %02d:%02d:%02d\r\n", yr, mnth, d, h, m, s);

		now = rtc.getEpoch();

		rtc.setSeconds(0); // to stop roll over

		rtc.setDate(d, mnth, yr-2000);
		rtc.setTime(h, m, s);

		t.tm_isdst = -1;
		t.tm_yday = 0;
		t.tm_wday = 0;
		t.tm_year = yr-2000+100;
		t.tm_mon = mnth - 1;
		t.tm_mday = d;
		t.tm_hour = h;
		t.tm_min = m;
		t.tm_sec = s;

		set_time = mktime(&t);
		gmtime_r(&now, &t);

		print_date(&t);

		SerialDEBUG.printf(" time difference = %d\r\n", set_time - now);

		SerialDEBUG.printf("last set time = %d\r\n", set_time - last_set_time);

		last_set_time = set_time;
	}

	now = rtc.getEpoch();
	gmtime_r(&now, &t);

	print_date(&t);
	SerialDEBUG.printf("\r\n");

	return 0;
}

int cmd_ls(char *line)
{
#ifdef SD_CAR_DET_PIN
	bool det = digitalRead(SD_CAR_DET_PIN);
#else
	bool det = true;
#endif

	File32 dir;
	File32 file;

	if (det)
	{
		SerialDEBUG.printf("card detected\r\n");
	}
	else
	{
		SerialDEBUG.printf("no card detected\r\n");
	}

#ifndef ADAFRUIT_ITSYBITSY_M0
	SerialDEBUG.printf("fattype : %d\r\n", flash.vol()->fatType());
#endif

	  // Open root directory
	  if (!dir.open("/"))
	  {
		  SerialDEBUG.printf("dir.open failed\r\n");
	  }
	  // Open next file in root.
	  // Warning, openNext starts at the current position of dir so a
	  // rewind may be necessary in your application.
	  while (file.openNext(&dir, O_RDONLY))
	  {
	    file.printFileSize(&SerialDEBUG);
	    SerialDEBUG.write(' ');
	    file.printModifyDateTime(&SerialDEBUG);
	    SerialDEBUG.write(' ');
	    file.printSFN(&SerialDEBUG);
	    SerialDEBUG.write(' ');
	    file.printName(&SerialDEBUG);
	    if (file.isDir())
	    {
	      // Indicate a directory.
	    	SerialDEBUG.write('/');
	    }
	    SerialDEBUG.printf("\r\n");
	    file.close();
	  }
	  if (dir.getError())
	  {
		  SerialDEBUG.printf("openNext failed\r\n");
	  }
	  else
	  {
		  SerialDEBUG.printf("Done!\r\n");
	  }

//	if (ft != 0)
//	{
//		SerialDEBUG.println(F("ls:"));
//		sd.ls(&SerialDEBUG, LS_DATE | LS_SIZE);
//	}

	return 0;
}

int cmd_debug(char *line)
{
	int new_debug = debug;

	if (line)
	{
	    sscanf(line, "%d", &new_debug);

		debug = new_debug;

		debug_set.write(debug);
	}

	SerialDEBUG.printf("debug %d\r\n", debug);

	return 0;
}

int cmd_burst(char *line)
{
	int16_t new_burst;

	if (line)
	{
	    sscanf(line, "%d", &new_burst);

		nburst.write(new_burst);
	}

	SerialDEBUG.printf("no. burst %d\r\n", nburst.read());

	return 0;
}

int cmd_sleep(char *line)
{
	can_sleep = false;
	can_sleep_count = 1; // sleep next second tick

	SerialDEBUG.printf("can sleep %d sleep_count %d", can_sleep, can_sleep_count);

	return 0;
}

int cmd_fcorr(char *line)
{
	int new_fcorr = RTC->MODE2.FREQCORR.reg;

	if (line)
	{
	    sscanf(line, "%d", &new_fcorr);
		RTC->MODE2.FREQCORR.reg = (uint8_t)new_fcorr;

		fcorr_set.write(new_fcorr);
	}

	SerialDEBUG.printf("fcorr %d\r\n", new_fcorr);

	return 0;
}

int cmd_sample(char *line)
{
	int new_sampleRate = sampleRate.read();

	if (line)
	{
	    sscanf(line, "%d", &new_sampleRate);

	    if (new_sampleRate >= 2)
	    {
			sampleRate.write(new_sampleRate);

			tcDisable();
			tcConfigure(new_sampleRate);
			tcStartCounter();
	    }
	    else
	    {
	    	SerialDEBUG.printf("minimum sample rate is 2 ms\r\n");
	    }
	}

	SerialDEBUG.printf("sample rate %d\r\n", new_sampleRate);

	return 0;
}

int cmd_pwrdelay(char *line)
{
	int new_pwrUpTime = pwrUpTime.read();

	if (line)
	{
	    sscanf(line, "%d", &new_pwrUpTime);

    	pwrUpTime.write(new_pwrUpTime);
	}

	SerialDEBUG.printf("power up time %d\r\n", new_pwrUpTime);

	return 0;
}

int cmd_set(char *line)
{
	char_80 txt;

	SerialDEBUG.printf("\r\n" __FILE__ " " __DATE__ " " __TIME__ "\r\n");

	time_t start_time = datetime_start.read();
	tm tm_elements;

	gmtime_r(&start_time, &tm_elements);

	SerialDEBUG.printf("Start time : ");
	print_date(&tm_elements);
	SerialDEBUG.printf("\r\n");

	now = rtc.getEpoch();

	gmtime_r(&now, &now_tm);

	SerialDEBUG.printf("clock : ");
	print_date(&now_tm);
	SerialDEBUG.printf("\r\n");

	SerialDEBUG.printf("mode : %d\r\n", run_mode.read());
	SerialDEBUG.printf("fcorr : %d\r\n", fcorr_set.read());
	SerialDEBUG.printf("can sleep : %d\r\n", can_sleep);
	SerialDEBUG.printf("debug : %d\r\n", debug);
	SerialDEBUG.printf("number in burst : %d\r\n", nburst.read());
	SerialDEBUG.printf("current file : %s\r\n", file_name);
	SerialDEBUG.printf("sample rate : %d (ms)\r\n", sampleRate.read());
	SerialDEBUG.printf("power up delay : %d (ms)\r\n", pwrUpTime.read());

	deployment_text.read(&txt);
	SerialDEBUG.printf("deployment text : %s\r\n", txt.txt);
	file_name_template.read(&txt);
	SerialDEBUG.printf("file name template: %s\r\n", txt.txt);
	SerialDEBUG.printf("file name template memory: %s\r\n", file_name_template_mem.txt);

	return 0;
}

int cmd_text(char *line)
{
	char txt[80];
	char *p = (char *)&txt[0];
	bool update = false;
	char_80 *pTxt;

	int i = 0;
	while (line != NULL)
	{
		SerialDEBUG.printf("%-2d arg : %d : %s \r\n", i, strlen(line), line);
		update = true;
		delay(100);

		strcpy(p, line);
		p += strlen(line);

		line = strtok(NULL, " ");
		i++;
		if (line)
		{
			strcpy(p, " ");
			p += 1;
		}
	}
	if (update)
	{
		pTxt = (char_80 *)&txt[0];
		SerialDEBUG.printf("deployment text set : %s\r\n", txt);

		deployment_text.write(*pTxt);
	}

	deployment_text.read(pTxt);
	SerialDEBUG.printf("deployment text : %s\r\n", pTxt->txt);

	return 0;
}

int cmd_file(char *line)
{
	if (line)
	{
		char_80 *p;
		p = (char_80 *)line;

		file_name_template.write(*p);
	}

	char_80 txt;

	file_name_template.read(&txt);
	SerialDEBUG.printf("file name : %s\r\n", txt.txt);
	file_name_template.read(&file_name_template_mem);

	return 0;
}
int cmd_hdr(char *line)
{
	SerialDEBUG.print("file header:\r\n----\r\n");
	SerialDEBUG.write((char *)hdr);
	SerialDEBUG.println("----");

	return 0;
}

#ifdef ZMODEM
int cmd_sz(char *line)
{
	if (line)
	{
		SerialDEBUG.printf("Waiting 1 sec, start receive\r\n");

		delay(1000);
		sendzrqinit();
		delay(200);
		fout.open(line, O_RDONLY);
		wcs(line);
		saybibi();
	}
	else
	{
		SerialDEBUG.printf("enter file name\r\n");
	}

	return 0;
}

int cmd_rz(char *line)
{
    if (wcreceive(0, 0))
    {
    	SerialDEBUG.printf("zmodem transfer failed\r\n");
    }
    else
    {
    	SerialDEBUG.printf("zmodem transfer successful\r\n");
    }
    fout.sync();
	fout.close();

	return 0;
}
#endif

SdCard* m_card = nullptr;

int cmd_format(char *line)
{
	FatFormatter fat_format;
	File32 file;

	if (line)
	{
		SerialDEBUG.printf("Format : %s\r\n", line);

		if (strcmp(line, "yes") == 0)
		{
#ifdef ADAFRUIT_ITSYBITSY_M0
			if (!flash.eraseChip())
			{
				SerialDEBUG.printf("Failed to erase chip!\r\n");
			}

			flash.waitUntilReady();
			SerialDEBUG.printf("Successfully erased chip!\r\n");

			//m_card = sd.card();
			//fat_format.format(m_card, hdr, &SerialDEBUG);
#else
			flash.begin(SD_CONFIG);
			m_card = flash.card();
			fat_format.format(m_card, hdr, &SerialDEBUG);
#endif
		}
#ifdef ADAFRUIT_ITSYBITSY_M0
		else
		{
			file_name_template.read(&file_name_template_mem);

			file.open(file_name_template_mem.txt, O_WRITE);
			file.truncate();
			file.remove();

			file.close();

			SerialDEBUG.printf("remove file : %s\r\n", file_name_template_mem.txt);
		}
#endif

	}
	else
	{
		SerialDEBUG.printf("Format not done try 'format yes'\r\n");
	}

	return 0;
}

int cmd_sync(char *line)
{
	binFile.sync();

	SerialDEBUG.printf("sync, position %d\r\n", binFile.position());

	return 0;
}


int line_idx = 0;
char line[80];

int read_line(char c)
{
	if (line_idx < sizeof(line))
	{
		if ((c == '\b') | (c == 127))
		{
			if (line_idx >= 1)
			{
				line_idx -= 1;
			}
			SerialDEBUG.printf("\b \b");
		}
		else if (c == '\r')
		{
			line[line_idx] = 0;
			SerialDEBUG.printf("Line : %s\r\n", line);

			executeCommand(line);

			line_idx = 0;
			memset(line, 0, sizeof(line));

			return 1;
		}
		else if (c == '\n')
		{
			// drop these
		}
		else
		{
			line[line_idx++] = c;
		}
	}
	else
	{
		line_idx = 0;
	}

	return 0;
}




