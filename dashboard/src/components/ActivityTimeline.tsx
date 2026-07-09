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
        <p className="text-[#6b7280] text-xs italic">No recent activity shifts detected.</p>
      </div>
    );
  }

  return (
    <div className="space-y-6 relative before:absolute before:left-[11px] before:top-2 before:bottom-2 before:w-[2px] before:bg-[#1f1f23]">
      {events.map((evt, idx) => (
        <motion.div
          key={idx}
          initial={{ x: -10, opacity: 0 }}
          animate={{ x: 0, opacity: 1 }}
          transition={{ delay: idx * 0.1 }}
          className="flex gap-4 relative"
        >
          <div
            className={`w-6 h-6 rounded-full border-4 border-[#0d0d0f] z-10 flex items-center justify-center ${
              evt.label === 'User Home' ? 'bg-[#34d399]/20' : 'bg-[#f87171]/20'
            }`}
          >
            <div
              className={`w-1.5 h-1.5 rounded-full ${
                evt.label === 'User Home' ? 'bg-[#34d399]' : 'bg-[#f87171]'
              } ${evt.label === 'User Home' ? 'animate-pulse-dot' : ''}`}
            />
          </div>
          <div className="flex-1 pb-4 border-b border-[#1f1f23]">
            <div className="flex justify-between">
              <p className={`text-xs font-medium ${
                evt.label === 'User Home' ? 'text-[#34d399]' : 'text-[#f87171]'
              }`}>
                {evt.label}
              </p>
              <span className="text-[11px] text-[#6b7280] font-mono">{formatSGTime(evt.time)}</span>
            </div>
          </div>
        </motion.div>
      ))}
    </div>
  );
}
