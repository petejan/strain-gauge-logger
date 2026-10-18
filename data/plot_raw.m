file = 'SGL-2026-10-16-T06.BIN.nc';

time = days(ncread(file, 'TIME')) + datetime(1950,1,1);
ch0 = ncread(file, 'CH_0');
ch1 = ncread(file, 'CH_1');

msk = time > datetime(2020,1,1);

figure(1);
yyaxis left
plot(time(msk), ch1(msk))
yyaxis right
temp = (ch0/1024) * 0.03125;
plot(time(msk), temp(msk))

figure(2);
plot(temp(msk), ch1(msk), '.')


fit=polyfit(temp(1:end-1), ch1(1:end-1), 1)
