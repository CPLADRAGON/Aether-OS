import React, { useState } from 'react';

interface DatePickerProps {
  mode: 'day' | 'week' | 'month' | 'year';
  selectedDate: Date;
  onSelect: (date: Date) => void;
  minDate: Date | null;
  onClose: () => void;
}

export default function DatePicker({ mode, selectedDate, onSelect, minDate, onClose }: DatePickerProps) {
  const [viewDate, setViewDate] = useState(new Date(selectedDate));

  const daysInMonth = new Date(viewDate.getFullYear(), viewDate.getMonth() + 1, 0).getDate();
  const firstDayOfMonth = new Date(viewDate.getFullYear(), viewDate.getMonth(), 1).getDay();
  // Adjust for Monday start (0=Sun -> 6, 1=Mon -> 0, etc)
  const startOffset = firstDayOfMonth === 0 ? 6 : firstDayOfMonth - 1;

  const today = new Date();
  today.setHours(0,0,0,0);

  const safeMinDate = minDate ? new Date(minDate) : null;
  if (safeMinDate) safeMinDate.setHours(0,0,0,0);

  const isDateDisabled = (date: Date) => {
    if (date > today) return true; // Future
    if (safeMinDate && date < safeMinDate) return true; // Before records
    return false;
  };

  // UI rendering based on mode
  if (mode === 'day' || mode === 'week') {
    const days = Array.from({ length: 42 }, (_, i) => {
      const dayNum = i - startOffset + 1;
      const date = new Date(viewDate.getFullYear(), viewDate.getMonth(), dayNum);
      date.setHours(0,0,0,0);
      return { date, isCurrentMonth: dayNum > 0 && dayNum <= daysInMonth };
    });

    const shiftMonth = (dir: number) => {
      setViewDate(prev => new Date(prev.getFullYear(), prev.getMonth() + dir, 1));
    };

    return (
      <div className="absolute top-full mt-2 left-1/2 -translate-x-1/2 z-50 p-4 bg-slate-950/95 backdrop-blur-xl border border-white/10 rounded-xl shadow-2xl w-72 flex flex-col gap-4 font-space">
        <div className="flex justify-between items-center text-cyan-50">
          <button onClick={() => shiftMonth(-1)} className="p-1 hover:bg-white/10 rounded"><span className="material-symbols-outlined text-[16px]">chevron_left</span></button>
          <div className="font-bold tracking-widest text-sm uppercase">{viewDate.toLocaleDateString('en-SG', { month: 'long', year: 'numeric' })}</div>
          <button onClick={() => shiftMonth(1)} className="p-1 hover:bg-white/10 rounded"><span className="material-symbols-outlined text-[16px]">chevron_right</span></button>
        </div>
        <div className="grid grid-cols-7 gap-1 text-center text-[10px] text-outline mb-2">
          {['M', 'T', 'W', 'T', 'F', 'S', 'S'].map((d, i) => <div key={i}>{d}</div>)}
        </div>
        <div className="grid grid-cols-7 gap-y-1">
          {mode === 'day' ? (
            days.map((d, i) => {
              const disabled = isDateDisabled(d.date);
              const isSelected = d.date.getTime() === new Date(selectedDate).setHours(0,0,0,0);
              return (
                <button 
                  key={i} 
                  disabled={disabled}
                  onClick={() => { onSelect(d.date); onClose(); }}
                  className={`h-8 w-8 mx-auto flex items-center justify-center rounded text-xs transition-colors
                    ${!d.isCurrentMonth ? 'opacity-20' : ''} 
                    ${disabled ? 'opacity-20 cursor-not-allowed' : 'hover:bg-cyan-400/20'} 
                    ${isSelected ? 'bg-cyan-500 text-slate-950 font-bold hover:bg-cyan-400' : 'text-cyan-50'}`}
                >
                  {d.date.getDate()}
                </button>
              )
            })
          ) : (
            Array.from({ length: 6 }).map((_, rowIdx) => {
              const weekDays = days.slice(rowIdx * 7, rowIdx * 7 + 7);
              const selectedStart = new Date(selectedDate);
              let sDay = selectedStart.getDay();
              if (sDay === 0) sDay = 7;
              selectedStart.setDate(selectedStart.getDate() - sDay + 1);
              selectedStart.setHours(0,0,0,0);
              const selectedEnd = new Date(selectedStart);
              selectedEnd.setDate(selectedStart.getDate() + 6);
              
              const isSelected = weekDays.some(d => d.date >= selectedStart && d.date <= selectedEnd);
              const disabled = weekDays.every(d => isDateDisabled(d.date));

              return (
                <div 
                  key={rowIdx} 
                  className={`col-span-7 grid grid-cols-7 rounded transition-colors group cursor-pointer 
                    ${disabled ? 'opacity-30 cursor-not-allowed' : 'hover:bg-cyan-400/10'}
                    ${isSelected ? 'bg-cyan-500/20 ring-1 ring-cyan-500/50' : ''}`}
                  onClick={() => { if(!disabled) { onSelect(weekDays[0].date); onClose(); } }}
                >
                  {weekDays.map((d, i) => (
                    <div key={i} className={`h-8 flex items-center justify-center text-xs ${!d.isCurrentMonth ? 'opacity-30' : ''} ${isSelected ? 'text-cyan-400 font-bold' : 'text-cyan-50'}`}>
                      {d.date.getDate()}
                    </div>
                  ))}
                </div>
              );
            })
          )}
        </div>
      </div>
    );
  }

  if (mode === 'month') {
    const shiftYear = (dir: number) => {
      setViewDate(prev => new Date(prev.getFullYear() + dir, 0, 1));
    };

    const months = Array.from({ length: 12 }, (_, i) => new Date(viewDate.getFullYear(), i, 1));

    return (
      <div className="absolute top-full mt-2 left-1/2 -translate-x-1/2 z-50 p-4 bg-slate-950/95 backdrop-blur-xl border border-white/10 rounded-xl shadow-2xl w-64 flex flex-col gap-4 font-space">
        <div className="flex justify-between items-center text-cyan-50">
          <button onClick={() => shiftYear(-1)} className="p-1 hover:bg-white/10 rounded"><span className="material-symbols-outlined text-[16px]">chevron_left</span></button>
          <div className="font-bold tracking-widest text-sm uppercase">{viewDate.getFullYear()}</div>
          <button onClick={() => shiftYear(1)} className="p-1 hover:bg-white/10 rounded"><span className="material-symbols-outlined text-[16px]">chevron_right</span></button>
        </div>
        <div className="grid grid-cols-3 gap-2">
          {months.map((d, i) => {
            const isSelected = d.getFullYear() === selectedDate.getFullYear() && d.getMonth() === selectedDate.getMonth();
            const lastDayOfMonth = new Date(d.getFullYear(), d.getMonth() + 1, 0);
            const disabled = isDateDisabled(d) && isDateDisabled(lastDayOfMonth);
            return (
              <button 
                key={i} 
                disabled={disabled}
                onClick={() => { onSelect(d); onClose(); }}
                className={`py-2 rounded text-xs transition-colors uppercase tracking-widest
                  ${disabled ? 'opacity-20 cursor-not-allowed' : 'hover:bg-cyan-400/20'} 
                  ${isSelected ? 'bg-cyan-500 text-slate-950 font-bold hover:bg-cyan-400' : 'text-cyan-50 bg-white/5'}`}
              >
                {d.toLocaleDateString('en-SG', { month: 'short' })}
              </button>
            )
          })}
        </div>
      </div>
    );
  }

  if (mode === 'year') {
    const startDecade = Math.floor(viewDate.getFullYear() / 10) * 10;
    const shiftDecade = (dir: number) => {
      setViewDate(prev => new Date(prev.getFullYear() + (dir * 10), 0, 1));
    };

    const years = Array.from({ length: 12 }, (_, i) => new Date(startDecade - 1 + i, 0, 1));

    return (
      <div className="absolute top-full mt-2 left-1/2 -translate-x-1/2 z-50 p-4 bg-slate-950/95 backdrop-blur-xl border border-white/10 rounded-xl shadow-2xl w-64 flex flex-col gap-4 font-space">
        <div className="flex justify-between items-center text-cyan-50">
          <button onClick={() => shiftDecade(-1)} className="p-1 hover:bg-white/10 rounded"><span className="material-symbols-outlined text-[16px]">chevron_left</span></button>
          <div className="font-bold tracking-widest text-sm uppercase">{startDecade} - {startDecade + 9}</div>
          <button onClick={() => shiftDecade(1)} className="p-1 hover:bg-white/10 rounded"><span className="material-symbols-outlined text-[16px]">chevron_right</span></button>
        </div>
        <div className="grid grid-cols-3 gap-2">
          {years.map((d, i) => {
            const isSelected = d.getFullYear() === selectedDate.getFullYear();
            const lastDayOfYear = new Date(d.getFullYear(), 11, 31);
            const disabled = isDateDisabled(d) && isDateDisabled(lastDayOfYear);
            const isOutDecade = i === 0 || i === 11;
            return (
              <button 
                key={i} 
                disabled={disabled}
                onClick={() => { onSelect(d); onClose(); }}
                className={`py-2 rounded text-xs transition-colors uppercase tracking-widest
                  ${isOutDecade ? 'opacity-30' : ''}
                  ${disabled ? 'opacity-20 cursor-not-allowed' : 'hover:bg-cyan-400/20'} 
                  ${isSelected ? 'bg-cyan-500 text-slate-950 font-bold hover:bg-cyan-400' : 'text-cyan-50 bg-white/5'}`}
              >
                {d.getFullYear()}
              </button>
            )
          })}
        </div>
      </div>
    );
  }

  return null;
}
