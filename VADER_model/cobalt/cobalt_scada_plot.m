clc;
clear all;

filename='cobalt_scada.csv';
scada_raw = csvread(filename, 9,1);
%scada_raw_no_time_stamp=scada_raw(:,2:3);
scada_diff=size(scada_raw);
for i=1:(size(scada_raw,1)-1)
    current_row=scada_raw(i,:);
    next_row=scada_raw(i+1,:);
    scada_diff(i,:)=next_row-current_row;
end
real_energy=scada_diff(:,1)./1000;
reactive_energy=scada_diff(:,2)./1000;

real_energy(1,:)=[];
reactive_energy(1,:)=[];
width=8;
height=4;
aluminum_scada=figure('Units','inches',...
'Position',[0 0 width height],...
'PaperPositionMode','auto');
plot(1:size(real_energy,1), real_energy);
hold on; 
plot(1:size(real_energy,1), reactive_energy);
 axis([1 size(real_energy,1) -150 300]);
set(gca,...
'Units','normalized',...
'YTick',-150:50:300,...
'XTick',0:30:360,...
'Position',[.15 .2 .75 .7],...
'FontUnits','points',...
'FontWeight','normal',...
'FontSize',12,...
'FontName','Times', ...
'LooseInset', get(gca, 'TightInset'));
 
xticklabels({'00:00',...
    '02:00',...
    '04:00',...
    '06:00',...
    '08:00',...
    '10:00',...
    '12:00',...
    '14:00',...
    '16:00',...
    '18:00',...
    '20:00',...
    '22:00',...
    '00:00'});

ylabel({'Energy'},...
'FontUnits','points',...
'interpreter','latex',...
'FontSize',13,...
'FontName','Times')
xlabel('Time (hour) ',...
'FontUnits','points',...
'FontWeight','normal',...
'FontSize',13,...
'FontName','Times');

legend({'$Real\ (kWh)$',...
    '$Reactive\ (kVAh)$'},...
'FontUnits','points',...
'interpreter','latex',...
'FontSize',10,...
'FontName','Times',...
'Location','SouthEast');
% 
% 
% %print(aluminum_scada,'cobalt_scada.pdf','-dpdf','-r0');
 print(aluminum_scada,'cobalt_scada.png', '-dpng', '-r0');
