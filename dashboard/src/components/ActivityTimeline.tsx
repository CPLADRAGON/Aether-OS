'use client';

import { motion } from 'framer-motion';

interface PresenceEvent {
  time: string;
  label: 'User Out' | 'User Home';
}

interface ActivityTimelineProps {
  events: PresenceEvent[];
  formatSGTime: (dateStr: string) => string;
}

export default function ActivityTimeline({ events, formatSGTime }: ActivityTimelineProps) {
  if (events.length === 0) {
    return (
      <div className="flex-1 flex items-center justify-center">
        <p className="text-white/30 text-xs italic font-mono">No recent activity shifts detected.</p>
      </div>
    );
  }

  return (
    <div className="space-y-6 relative before:absolute before:left-[11px] before:top-2 before:bottom-2 before:w-[2px] before:bg-white/5">
      {events.map((evt, idx) => (
        <motion.div
          key={idx}
          initial={{ x: -10, opacity: 0 }}
          animate={{ x: 0, opacity: 1 }}
          transition={{ delay: idx * 0.1 }}
          className="flex gap-4 relative"
        >
          <div
            className={`w-6 h-6 rounded-full border-4 border-[#050505] z-10 flex items-center justify-center ${
              evt.label === 'User Home' ? 'bg-emerald-500/20' : 'bg-red-500/20'
            }`}
          >
            <div
              className={`w-1.5 h-1.5 rounded-full ${
                evt.label === 'User Home' ? 'bg-emerald-500' : 'bg-red-500'
              } ${evt.label === 'User Home' ? 'animate-pulse-dot' : ''}`}
            />
          </div>
          <div className="flex-1 pb-4 border-b border-white/5">
            <div className="flex justify-between">
              <p className={`text-xs font-headline font-bold uppercase tracking-wider ${
                evt.label === 'User Home' ? 'text-emerald-500' : 'text-red-500'
              }`}>
                {evt.label}
              </p>
              <span className="text-[10px] text-white/30 font-mono">{formatSGTime(evt.time)}</span>
            </div>
          </div>
        </motion.div>
      ))}
    </div>
  );
}
